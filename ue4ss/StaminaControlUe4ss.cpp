#include "../Diagnostics.hpp"
#include "ModVersion.hpp"

#include <Briefcase/DeceiveInc/Hooks.hpp>
#include <Briefcase/DeceiveInc/NativeStamina.hpp>
#include <Briefcase/DeceiveInc/Paths.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>
#include <Mod/CppUserModBase.hpp>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

namespace {
using namespace RC;
using briefcase::deceive::FunctionHook;
using briefcase::deceive::NativeStaminaHook;
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
        reset_hook_.reset();
        reduce_hook_.reset();
        restore_owned_settings();
    }

    void on_unreal_init() override { try_install(); }

    void on_update() override {
        if (reduce_hook_ || std::chrono::steady_clock::now() < next_attempt_)
            return;
        next_attempt_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        try_install();
    }

  private:
    struct Tracked {
        Spy spy;
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
    NativeStaminaHook reduce_hook_;
    FunctionHook reset_hook_;
    std::unordered_map<RC::Unreal::UObject *, Tracked> tracked_;
    std::vector<Pending> pending_;
    std::chrono::steady_clock::time_point next_attempt_{};
    std::uint64_t reduce_calls_{};

    void try_install() noexcept {
        try {
            reduce_hook_ = briefcase::deceive::hook_native_reduce_stamina(
                [this](Spy spy, float &delta) { before_reduce(spy, delta); },
                [this](Spy spy, float &delta) { after_reduce(spy, delta); });
            reset_hook_ = briefcase::deceive::hook_reset_stamina([this](Spy spy) { configure(spy); });
            Output::send(STR("[Briefcase.SuspicionControl] ready: multiplier={}, diagnostics={}\n"),
                         config_.multiplier, config_.diagnostics);
        } catch (const std::exception &error) {
            reduce_hook_.reset();
            reset_hook_.reset();
            Output::send<LogLevel::Warning>(STR("[Briefcase.SuspicionControl] initialization failed: {}\n"),
                                            RC::to_wstring(error.what()));
        } catch (...) {
            reduce_hook_.reset();
            reset_hook_.reset();
        }
    }

    bool configure(Spy spy) {
        if (!spy || !spy.is_authoritative_player())
            return false;
        if (tracked_.contains(spy.object()))
            return true;
        if (tracked_.size() >= 64)
            throw std::runtime_error("Live Spy limit reached");

        const auto original = spy.stamina_drain_multiplier();
        const auto applied = suspicion::effective_multiplier(original, config_.multiplier);
        const auto original_run = spy.run_drain_enabled();
        Tracked state{spy, original, applied, original_run};
        if (config_.multiplier != 1 && applied != original) {
            spy.set_stamina_drain_multiplier(applied);
            if (spy.stamina_drain_multiplier() != applied)
                throw std::runtime_error("Stamina multiplier readback mismatch");
            state.changed_multiplier = true;
        }
        if (config_.multiplier == 0 && original_run) {
            spy.set_run_drain_enabled(false);
            if (spy.run_drain_enabled())
                throw std::runtime_error("Run drain disable readback mismatch");
            state.changed_run = true;
        }
        tracked_.emplace(spy.object(), state);
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
                if (!state.spy)
                    continue;
                if (state.changed_multiplier &&
                    state.spy.stamina_drain_multiplier() == state.applied_multiplier)
                    state.spy.set_stamina_drain_multiplier(state.original_multiplier);
                if (state.changed_run && !state.spy.run_drain_enabled())
                    state.spy.set_run_drain_enabled(state.original_run);
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
