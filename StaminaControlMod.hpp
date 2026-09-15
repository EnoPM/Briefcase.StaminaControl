#pragma once
#include "Diagnostics.hpp"
#include <Briefcase/Unreal.hpp>
#include <array>
#include <vector>

namespace stamina {
// The existing package ID, DLL name and configuration remain compatible.
// This class controls stamina; no Heat system is involved.
class StaminaControlMod {
    const BcApi *api_{};
    const BcUnrealApi *unreal_{};
    suspicion::Config config_;
    suspicion::Budget budget_{120}, configurationBudget_{16};
    BcHandle reduceFunction_{}, resetFunction_{}, getRunFunction_{}, setRunFunction_{}, deletion_{};
    std::array<BcHandle, 3> hooks_{};
    bool accepting_{}, enabled_{}, failed_{};
    unsigned depth_{};
    uint64_t reduceCalls_{}, configuredCount_{};

    struct Tracked {
        BcHandle self;
        double original, applied;
        bool original_run, changed_multiplier = false, changed_run = false;
    };
    struct Snapshot {
        double stamina, cover;
    };
    struct Pending {
        BcHandle self;
        unsigned depth;
        Snapshot before;
        double requested, applied;
    };
    std::vector<Tracked> tracked_;
    std::vector<Pending> pending_;

    double ReadNumber(BcHandle self, const char *name, uint32_t kind);
    Snapshot ReadSnapshot(BcHandle self);
    bool IsRunDrainEnabled(BcHandle self);
    void SetRunDrainEnabled(BcHandle self, bool on);
    void WriteMultiplier(BcHandle self, double value);
    bool Configure(BcHandle self);
    void Restore(Tracked &state) noexcept;
    void Clear() noexcept;
    void Fail(const std::exception &error) noexcept;
    void Initialize() noexcept;
    void OnDeleted(BcHandle self) noexcept;
    void OnReset(const BcHookEvent *event) noexcept;
    void OnBefore(const BcHookEvent *event) noexcept;
    void OnAfter(const BcHookEvent *event) noexcept;
    void Log(std::string_view message) const noexcept;

    static void BC_CALL InitializeCallback(void *user) noexcept;
    static void BC_CALL DeletedCallback(BcHandle self, void *user) noexcept;
    static void BC_CALL ResetCallback(const BcHookEvent *event, void *user) noexcept;
    static void BC_CALL BeforeCallback(const BcHookEvent *event, void *user) noexcept;
    static void BC_CALL AfterCallback(const BcHookEvent *event, void *user) noexcept;

  public:
    StaminaControlMod() = default;
    StaminaControlMod(const StaminaControlMod &) = delete;
    StaminaControlMod &operator=(const StaminaControlMod &) = delete;
    BcResult Load(const BcApi *api) noexcept;
    void Unload() noexcept;
};
} // namespace stamina
