#include "StaminaControlMod.hpp"
namespace {
stamina::StaminaControlMod mod;
}
extern "C" BC_EXPORT BcResult BC_CALL BriefcaseModLoad(const BcApi *api) noexcept {
    return mod.Load(api);
}
extern "C" BC_EXPORT void BC_CALL BriefcaseModUnload() noexcept {
    mod.Unload();
}
