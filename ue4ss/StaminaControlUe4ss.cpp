#include "../Diagnostics.hpp"
#include "ModVersion.hpp"

#include <Briefcase/DeceiveInc/Hooks.hpp>
#include <Briefcase/DeceiveInc/NativeStamina.hpp>
#include <Briefcase/DeceiveInc/Paths.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/FWeakObjectPtr.hpp>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

namespace {
using namespace RC;
using briefcase::deceive::FunctionHook;
using briefcase::deceive::NativeStaminaHook;
using briefcase::deceive::NativeResetStaminaHook;
using briefcase::deceive::Spy;

extern "C" __declspec(dllexport) RC::CppUserModBase *start_mod();

suspicion::Config load_config() {
    const auto path = briefcase::deceive::configuration_file(
        reinterpret_cast<const void *>(&start_mod), "briefcase.suspicion-control", "config.json");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Missing Data/config.json");
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    return suspicion::Config::parse(text);
}

class StaminaControlUe4ss final : public CppUserModBase {
  public:
    StaminaControlUe4ss() : config_(load_config()), sample_budget_(config_.maximum) {
        ModName = STR("Briefcase.SuspicionControl");
        ModVersion = briefcase_mod_version;
        ModDescription = STR("Configurable suspicion-related stamina drain");
        ModAuthors = STR("EnoPM");
    }

    ~StaminaControlUe4ss() override {
        spy_begin_hook_.reset();
        actor_begin_hook_.reset();
        reset_hook_.reset();
        native_reset_hook_.reset();
        reduce_hook_.reset();
        restore_owned_settings();
    }

    void on_unreal_init() override { try_install(); }

    void on_update() override {
        if ((reduce_hook_ && native_reset_hook_ && reset_hook_ && spy_begin_hook_ && actor_begin_hook_) ||
            std::chrono::steady_clock::now() < next_attempt_)
            return;
        next_attempt_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        try_install();
    }

  private:
    struct Tracked {
        RC::Unreal::FWeakObjectPtr weak;
        float original_multiplier{};
        float applied_multiplier{};
        bool original_run{};
        bool changed_multiplier{};
        bool changed_run{};
    };
    struct Pending {
        Spy spy;
        float stamina{};
        float cover{};
        float requested{};
        float applied{};
    };

    suspicion::Config config_;
    suspicion::Budget sample_budget_;
    suspicion::Budget skipped_budget_{8};
    NativeStaminaHook reduce_hook_;
    NativeResetStaminaHook native_reset_hook_;
    FunctionHook reset_hook_;
    FunctionHook spy_begin_hook_;
    FunctionHook actor_begin_hook_;
    std::unordered_map<RC::Unreal::UObject *, Tracked> tracked_;
    std::vector<Pending> pending_;
    std::chrono::steady_clock::time_point next_attempt_{};
    std::uint64_t reduce_calls_{};
    std::uint32_t callback_errors_{};
    bool ready_logged_{};
    bool lifecycle_logged_{};

    void report_error(const wchar_t *stage, const char *message) noexcept {
        if (callback_errors_++ >= 8)
            return;
        try {
            Output::send<LogLevel::Warning>(
                STR("[Briefcase.SuspicionControl] {} failed: {}\n"),
                stage, RC::to_wstring(message));
        } catch (...) {
        }
    }

    void try_install() noexcept {
        try {
            if (!reduce_hook_) reduce_hook_ = briefcase::deceive::hook_native_reduce_stamina(
                [this](Spy spy, float &delta) {
                    try {
                        before_reduce(spy, delta);
                    } catch (const std::exception &error) {
                        report_error(L"ReduceStamina PRE", error.what());
                    } catch (...) {
                        report_error(L"ReduceStamina PRE", "unexpected exception");
                    }
                },
                [this](Spy spy, float &delta) {
                    try {
                        after_reduce(spy, delta);
                    } catch (const std::exception &error) {
                        report_error(L"ReduceStamina POST", error.what());
                    } catch (...) {
                        report_error(L"ReduceStamina POST", "unexpected exception");
                    }
                });
            if (!ready_logged_) {
                ready_logged_ = true;
                Output::send(STR("[Briefcase.SuspicionControl] ready: multiplier={}, diagnostics={}\n"),
                             config_.multiplier, config_.diagnostics);
            }
        } catch (const std::exception &error) {
            report_error(L"native ReduceStamina hook installation", error.what());
            return;
        } catch (...) {
            report_error(L"native ReduceStamina hook installation", "unexpected exception");
            return;
        }
        auto configure_from_lifecycle = [this](Spy spy) {
            try {
                configure(spy, true);
            } catch (const std::exception &error) {
                report_error(L"Spy lifecycle callback", error.what());
            } catch (...) {
                report_error(L"Spy lifecycle callback", "unexpected exception");
            }
        };
        try {
            if (!native_reset_hook_) {
                native_reset_hook_ = briefcase::deceive::hook_native_reset_stamina(
                    configure_from_lifecycle);
                Output::send(STR("[Briefcase.SuspicionControl] native Spy reset hook installed\n"));
            }
        } catch (const std::exception &error) {
            report_error(L"native ResetStaminaToMax hook installation", error.what());
        } catch (...) {
            report_error(L"native ResetStaminaToMax hook installation", "unexpected exception");
        }
        try {
            if (!reset_hook_) reset_hook_ = briefcase::deceive::hook_reset_stamina(configure_from_lifecycle);
        } catch (const std::exception &error) {
            report_error(L"ResetStaminaToMax hook installation", error.what());
        } catch (...) {
            report_error(L"ResetStaminaToMax hook installation", "unexpected exception");
        }
        try {
            if (!spy_begin_hook_) spy_begin_hook_ = briefcase::deceive::hook_spy_server_begin_play(configure_from_lifecycle);
        } catch (const std::exception &error) {
            report_error(L"Spy begin-play hook installation", error.what());
        } catch (...) {
            report_error(L"Spy begin-play hook installation", "unexpected exception");
        }
        try {
            if (!actor_begin_hook_) actor_begin_hook_ = briefcase::deceive::hook_actor_receive_begin_play(configure_from_lifecycle);
        } catch (const std::exception &error) {
            report_error(L"Actor begin-play hook installation", error.what());
        } catch (...) {
            report_error(L"Actor begin-play hook installation", "unexpected exception");
        }
        if (!lifecycle_logged_ && spy_begin_hook_ && actor_begin_hook_) {
            lifecycle_logged_ = true;
            try {
                Output::send(STR("[Briefcase.SuspicionControl] early Spy lifecycle hooks installed\n"));
            } catch (...) {
            }
        }
    }

    void prune_stale_spies() {
        for (auto it = tracked_.begin(); it != tracked_.end();) {
            if (it->second.weak.Get() != it->first)
                it = tracked_.erase(it);
            else
                ++it;
        }
    }

    void apply_to_spy(Spy spy, Tracked &state) {
        if (config_.multiplier != 1 &&
            spy.stamina_drain_multiplier() != state.applied_multiplier) {
            state.changed_multiplier = true;
            spy.set_stamina_drain_multiplier(state.applied_multiplier);
            if (spy.stamina_drain_multiplier() != state.applied_multiplier)
                throw std::runtime_error("Stamina multiplier readback mismatch");
        }
        if (config_.multiplier == 0 && spy.run_drain_enabled()) {
            if (!state.changed_run) state.original_run = true;
            state.changed_run = true;
            spy.set_run_drain_enabled(false);
            if (spy.run_drain_enabled())
                throw std::runtime_error("Run drain disable readback mismatch");
        }
    }

    bool configure(Spy spy, bool reapply_existing = false) {
        if (!spy)
            return false;
        if (!spy.is_authoritative_player()) {
            if (config_.diagnostics && skipped_budget_.take())
                Output::send(STR("[Briefcase.SuspicionControl] skipped non-authoritative Spy role={} template={}\n"),
                             spy.role(), spy.is_template());
            return false;
        }
        if (auto it = tracked_.find(spy.object()); it != tracked_.end()) {
            if (it->second.weak.Get() == spy.object()) {
                if (reapply_existing) apply_to_spy(spy, it->second);
                return true;
            }
            tracked_.erase(it);
        }
        prune_stale_spies();
        if (tracked_.size() >= 256)
            throw std::runtime_error("Live Spy limit reached");

        const auto original = spy.stamina_drain_multiplier();
        const auto applied = suspicion::effective_multiplier(original, config_.multiplier);
        const auto original_run = spy.run_drain_enabled();
        Tracked state{};
        state.weak = spy.object();
        state.original_multiplier = original;
        state.applied_multiplier = applied;
        state.original_run = original_run;
        auto it = tracked_.emplace(spy.object(), state).first;
        apply_to_spy(spy, it->second);
        if (config_.diagnostics)
            Output::send(STR("[Briefcase.SuspicionControl] configured multiplier={}->{} runDrain={}->{} object={}\n"),
                         original, applied, original_run, config_.multiplier == 0 ? false : original_run,
                         spy.path());
        return true;
    }

    void before_reduce(Spy spy, float &delta) {
        ++reduce_calls_;
        if (!configure(spy))
            return;
        const auto requested = delta;
        const auto applied = suspicion::scaled_delta(requested, config_.multiplier);
        if (config_.diagnostics && sample_budget_.take())
            pending_.push_back({spy, spy.stamina(), spy.cover_ratio(), requested, applied});
        delta = applied;
    }

    void after_reduce(Spy spy, float &) {
        if (pending_.empty() || pending_.back().spy.object() != spy.object())
            return;
        const auto sample = pending_.back();
        pending_.pop_back();
        Output::send(STR("[Briefcase.SuspicionControl] delta={}->{} stamina={}->{} cover={}->{}\n"),
                     sample.requested, sample.applied, sample.stamina, spy.stamina(), sample.cover,
                     spy.cover_ratio());
    }

    void restore_owned_settings() noexcept {
        for (auto &[_, state] : tracked_) {
            try {
                auto *object = state.weak.Get();
                if (!object)
                    continue;
                Spy spy{object};
                if (state.changed_multiplier &&
                    spy.stamina_drain_multiplier() == state.applied_multiplier)
                    spy.set_stamina_drain_multiplier(state.original_multiplier);
                if (state.changed_run && !spy.run_drain_enabled())
                    spy.set_run_drain_enabled(state.original_run);
            } catch (...) {
            }
        }
        tracked_.clear();
        pending_.clear();
    }
};
} // namespace

extern "C" __declspec(dllexport) RC::CppUserModBase *start_mod() {
    try {
        return new StaminaControlUe4ss();
    } catch (const std::exception &error) {
        RC::Output::send<RC::LogLevel::Warning>(STR("[Briefcase.SuspicionControl] load failed: {}\n"),
                                                RC::to_wstring(error.what()));
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase *mod) { delete mod; }
