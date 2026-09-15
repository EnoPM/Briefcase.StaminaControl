#include "StaminaControlMod.hpp"
#include "SupportedBuild.hpp"
#include "NativeSites.hpp"
#include <algorithm>
#include <format>

namespace stamina {
void StaminaControlMod::Log(std::string_view message) const noexcept {
    if (api_ && api_->log)
        briefcase::Api(api_).log(message);
}
void StaminaControlMod::InitializeCallback(void *user) noexcept {
    static_cast<StaminaControlMod *>(user)->Initialize();
}
void StaminaControlMod::DeletedCallback(BcHandle self, void *user) noexcept {
    static_cast<StaminaControlMod *>(user)->OnDeleted(self);
}
void StaminaControlMod::ResetCallback(const BcHookEvent *event, void *user) noexcept {
    static_cast<StaminaControlMod *>(user)->OnReset(event);
}
void StaminaControlMod::BeforeCallback(const BcHookEvent *event, void *user) noexcept {
    static_cast<StaminaControlMod *>(user)->OnBefore(event);
}
void StaminaControlMod::AfterCallback(const BcHookEvent *event, void *user) noexcept {
    static_cast<StaminaControlMod *>(user)->OnAfter(event);
}
double StaminaControlMod::ReadNumber(BcHandle self, const char *name, uint32_t kind) {
    BcValue value{};
    briefcase::require(unreal_->read_property(api_->context, self, name, &value), name);
    if (value.kind != kind)
        throw std::runtime_error(std::string("Unexpected property type: ") + name);
    return kind == BC_VALUE_F32 ? value.data.number : double(value.data.integer);
}
StaminaControlMod::Snapshot StaminaControlMod::ReadSnapshot(BcHandle self) {
    return {ReadNumber(self, "StaminaCurrent", BC_VALUE_F32), ReadNumber(self, "CoverRatio", BC_VALUE_F32)};
}
bool StaminaControlMod::IsRunDrainEnabled(BcHandle self) {
    BcValue value{};
    briefcase::require(unreal_->invoke(api_->context, self, getRunFunction_, nullptr, 0, &value),
                       "GetRunDrainEnabled");
    if (value.kind != BC_VALUE_BOOL)
        throw std::runtime_error("Run drain result type mismatch");
    return value.data.integer != 0;
}
void StaminaControlMod::SetRunDrainEnabled(BcHandle self, bool on) {
    BcNamedValue arg{};
    arg.name = "bEnabled";
    arg.value.kind = BC_VALUE_BOOL;
    arg.value.data.integer = on;
    BcValue ignored{};
    briefcase::require(unreal_->invoke(api_->context, self, setRunFunction_, &arg, 1, &ignored),
                       "SetRunDrainEnabled");
}
void StaminaControlMod::WriteMultiplier(BcHandle self, double value) {
    auto v = briefcase::f32(static_cast<float>(value));
    briefcase::require(unreal_->write_property(api_->context, self, "StaminaDrainRateMultiplier", &v),
                       "write StaminaDrainRateMultiplier");
}
void StaminaControlMod::Restore(Tracked &state) noexcept {
    if (api_->validate_handle(api_->context, state.self) != BC_OK)
        return;
    try {
        // Restore only the value still owned by this mod; do not overwrite a later gameplay change.
        if (state.changed_multiplier &&
            ReadNumber(state.self, "StaminaDrainRateMultiplier", BC_VALUE_F32) == state.applied)
            WriteMultiplier(state.self, state.original);
        if (state.changed_run && !IsRunDrainEnabled(state.self))
            SetRunDrainEnabled(state.self, state.original_run);
    } catch (...) {
        Log("Suspicion restore skipped: instance unavailable or changed.");
    }
}
void StaminaControlMod::Clear() noexcept {
    enabled_ = false;
    if (!api_ || !unreal_)
        return;
    for (auto &h : hooks_)
        if (h) {
            unreal_->unhook(api_->context, h);
            h = 0;
        }
    if (deletion_) {
        unreal_->unhook(api_->context, deletion_);
        deletion_ = 0;
    }
    for (auto &state : tracked_) {
        Restore(state);
        api_->release_handle(api_->context, state.self);
    }
    tracked_.clear();
    for (auto &p : pending_)
        api_->release_handle(api_->context, p.self);
    pending_.clear();
    depth_ = 0;
    for (auto h : {reduceFunction_, resetFunction_, getRunFunction_, setRunFunction_})
        if (h)
            api_->release_handle(api_->context, h);
    reduceFunction_ = resetFunction_ = getRunFunction_ = setRunFunction_ = 0;
}
void StaminaControlMod::Fail(const std::exception &e) noexcept {
    Clear();
    if (!failed_) {
        failed_ = true;
        try {
            Log(std::string("Suspicion Control DISABLED: ") + e.what());
        } catch (...) {
        }
    }
}
bool StaminaControlMod::Configure(BcHandle self) {
    if (std::any_of(tracked_.begin(), tracked_.end(), [&](const auto &s) { return s.self == self; }))
        return true;
    BcObjectInfo info{};
    info.size = sizeof(info);
    briefcase::require(unreal_->object_info(api_->context, self, &info), "Spy instance");
    if ((info.flags & 48) || ReadNumber(self, "Role", BC_VALUE_U8) != 3)
        return false;
    if (tracked_.size() >= 64)
        throw std::runtime_error("Live Spy limit reached");
    const auto original = ReadNumber(self, "StaminaDrainRateMultiplier", BC_VALUE_F32);
    const auto applied = suspicion::effective_multiplier(original, config_.multiplier);
    const auto original_run = IsRunDrainEnabled(self);
    briefcase::require(unreal_->retain(api_->context, self), "retain configured Spy");
    try {
        tracked_.push_back({self, original, applied, original_run});
    } catch (...) {
        api_->release_handle(api_->context, self);
        throw;
    }
    auto &state = tracked_.back();
    if (config_.multiplier != 1 && applied != original) {
        state.changed_multiplier = true;
        WriteMultiplier(self, applied);
        if (ReadNumber(self, "StaminaDrainRateMultiplier", BC_VALUE_F32) != applied)
            throw std::runtime_error("Stamina multiplier readback mismatch");
    }
    if (config_.multiplier == 0 && original_run) {
        state.changed_run = true;
        SetRunDrainEnabled(self, false);
        if (IsRunDrainEnabled(self))
            throw std::runtime_error("Run drain disable readback mismatch");
    }
    ++configuredCount_;
    if (config_.diagnostics && configurationBudget_.take())
        Log(std::format("SUSPICION CONFIGURED factor={} rateMultiplier={}->{} runDrain={}->{} object={}",
                        config_.multiplier, original, applied, original_run,
                        config_.multiplier == 0 ? false : original_run, info.path));
    return true;
}
void StaminaControlMod::OnDeleted(BcHandle self) noexcept {
    std::erase_if(tracked_, [&](const auto &s) { return s.self == self; });
}
void StaminaControlMod::OnReset(const BcHookEvent *e) noexcept {
    if (!enabled_ || !e)
        return;
    try {
        Configure(e->self);
    } catch (const std::exception &error) {
        Fail(error);
    } catch (...) {
        Clear();
    }
}
void StaminaControlMod::OnBefore(const BcHookEvent *e) noexcept {
    if (!enabled_ || !e)
        return;
    ++depth_;
    ++reduceCalls_;
    try {
        if (!Configure(e->self))
            return;
        BcValue delta{};
        briefcase::require(unreal_->read_argument(api_->context, e->call, "Delta", &delta), "read Delta");
        if (delta.kind != BC_VALUE_F32)
            throw std::runtime_error("Delta type mismatch");
        const auto requested = delta.data.number;
        const auto applied = suspicion::scaled_delta(requested, config_.multiplier);
        if (applied != requested) {
            auto value = briefcase::f32(applied);
            briefcase::require(unreal_->write_argument(api_->context, e->call, "Delta", &value),
                               "scale native Delta");
        }
        if (config_.diagnostics && pending_.size() < 32 && budget_.take()) {
            const auto state = ReadSnapshot(e->self);
            briefcase::require(unreal_->retain(api_->context, e->self), "retain drain sample");
            try {
                pending_.push_back({e->self, depth_, state, requested, applied});
            } catch (...) {
                api_->release_handle(api_->context, e->self);
                throw;
            }
        }
    } catch (const std::exception &error) {
        Fail(error);
    } catch (...) {
        Clear();
    }
}
void StaminaControlMod::OnAfter(const BcHookEvent *e) noexcept {
    if (!enabled_ || !e)
        return;
    if (!pending_.empty() && pending_.back().depth == depth_ && pending_.back().self == e->self) {
        const auto p = pending_.back();
        pending_.pop_back();
        try {
            const auto now = ReadSnapshot(e->self);
            Log(std::format("SUSPICION APPLIED factor={} delta={}->{} stamina={}->{} cover={}->{}",
                            config_.multiplier, p.requested, p.applied, p.before.stamina, now.stamina,
                            p.before.cover, now.cover));
        } catch (const std::exception &error) {
            Fail(error);
        } catch (...) {
            Clear();
        }
        api_->release_handle(api_->context, p.self);
    }
    if (depth_)
        --depth_;
}
void StaminaControlMod::Initialize() noexcept {
    if (!accepting_)
        return;
    try {
        briefcase::Unreal u(api_);
        reduceFunction_ = u.resolve(
            "/Script/DeceiveInc.Spy:ReduceStamina",
            R"({"parameterSize":4,"flags":67634177,"parameters":[{"name":"Delta","type":"float","offset":0}]})");
        resetFunction_ = u.resolve("/Script/DeceiveInc.Spy:ResetStaminaToMax",
                                   R"({"parameterSize":0,"flags":67634177,"parameters":[]})");
        getRunFunction_ = u.resolve(
            "/Script/DeceiveInc.Spy:GetRunDrainEnabled",
            R"({"parameterSize":1,"flags":1409811457,"parameters":[{"name":"ReturnValue","type":"bool","offset":0,"return":true}]})");
        setRunFunction_ = u.resolve(
            "/Script/DeceiveInc.Spy:SetRunDrainEnabled",
            R"({"parameterSize":1,"flags":67634177,"parameters":[{"name":"bEnabled","type":"bool","offset":0}]})");
        auto &n = briefcase::Services(api_).service<BcNativeHookApi>(BC_NATIVE_HOOK_SERVICE);
        briefcase::require(n.hook(api_->context, reduceFunction_, suspicion::sha256, &suspicion::reduce_site,
                                  BC_HOOK_PRE, BeforeCallback, this, &hooks_[0]),
                           "native ReduceStamina PRE");
        briefcase::require(n.hook(api_->context, reduceFunction_, suspicion::sha256, &suspicion::reduce_site,
                                  BC_HOOK_POST, AfterCallback, this, &hooks_[1]),
                           "native ReduceStamina POST");
        briefcase::require(n.hook(api_->context, resetFunction_, suspicion::sha256, &suspicion::reset_site,
                                  BC_HOOK_POST, ResetCallback, this, &hooks_[2]),
                           "native ResetStaminaToMax POST");
        deletion_ = u.deleted(DeletedCallback, this);
        enabled_ = true;
        Log(std::format("Suspicion Control ACTIVE: multiplier={}, diagnostics={}, maxSamples={}; "
                        "running multiplier + native discrete Delta; zero disables run drain.",
                        config_.multiplier, config_.diagnostics, config_.maximum));
    } catch (const std::exception &error) {
        Fail(error);
    } catch (...) {
        Clear();
    }
}

BcResult StaminaControlMod::Load(const BcApi *api) noexcept {
    if (accepting_ || api_)
        return BC_INVALID_ARGUMENT;
    try {
        if (!server_mods::supported_build(api))
            return BC_VERSION_MISMATCH;
        api_ = api;
        const briefcase::Services services(api_);
        config_ = suspicion::Config::parse(services.config(suspicion::schema));
        unreal_ = &services.service<BcUnrealApi>(BC_UNREAL_SERVICE, offsetof(BcUnrealApi, invoke_outputs));
        budget_ = suspicion::Budget(config_.maximum);
        configurationBudget_ = suspicion::Budget(16);
        failed_ = false;
        reduceCalls_ = configuredCount_ = 0;
        accepting_ = true;
        const auto result = api_->post_game_thread(api_->context, InitializeCallback, this);
        if (result != BC_OK) {
            accepting_ = false;
            api_ = nullptr;
            unreal_ = nullptr;
        }
        return result;
    } catch (const std::exception &e) {
        try {
            Log(e.what());
        } catch (...) {
        }
        accepting_ = false;
        api_ = nullptr;
        unreal_ = nullptr;
        return BC_INVALID_ARGUMENT;
    } catch (...) {
        accepting_ = false;
        api_ = nullptr;
        unreal_ = nullptr;
        return BC_INTERNAL;
    }
}
void StaminaControlMod::Unload() noexcept {
    accepting_ = false;
    if (!api_)
        return;
    Clear();
    try {
        Log(std::format("Suspicion Control unloaded: configured={}, ReduceStamina calls={}, samples={}; "
                        "owned settings restored.",
                        configuredCount_, reduceCalls_, budget_.used()));
    } catch (...) {
    }
    api_ = nullptr;
    unreal_ = nullptr;
}
} // namespace stamina
