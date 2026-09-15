#include "MockHost.hpp"
static Json staminaConfig(double factor) { return {{"multiplier",factor},{"diagnostics",true},{"maximumSamples",120}}; }
static void stamina() {
    for (double factor : {0., .5, 1.}) {
        Backend b(staminaConfig(factor));
        Module mod(L"Briefcase.SuspicionControl.dll");
        check(mod.InvalidLoad() == BC_VERSION_MISMATCH, "Stamina rejects missing host");
        check(mod.Load(b) == BC_OK, "Stamina loads");
        b.pump();
        check(b.hooks.size() == 3 && b.deleted.size() == 1, "Stamina registers native callbacks on instance");
        b.event(":ResetStaminaToMax", BC_HOOK_POST);
        check(b.objects[1].multiplier == 2 * factor && b.objects[1].run == (factor != 0),
              "Running settings applied");
        b.delta = 20;
        b.event(":ReduceStamina", BC_HOOK_PRE);
        check(b.delta == 20 * factor, "Discrete positive stamina delta scaled");
        b.objects[1].stamina -= b.delta;
        b.event(":ReduceStamina", BC_HOOK_POST);
        check(b.refs[1] == 2, "Diagnostic sample reference released after POST");
        b.delta = -20;
        b.event(":ReduceStamina", BC_HOOK_PRE);
        b.event(":ReduceStamina", BC_HOOK_POST);
        check(b.delta == -20, "Stamina healing preserved");
        mod.Stop();
        check(b.objects[1].multiplier == 2 && b.objects[1].run, "Owned stamina settings restored");
        check(b.clean(), "Stamina teardown releases functions hooks and references");
    }
    {
        Backend b(staminaConfig(0));
        Module mod(L"Briefcase.SuspicionControl.dll");
        check(mod.Load(b) == BC_OK, "Stamina ownership scenario loads");
        b.pump();
        b.event(":ResetStaminaToMax", BC_HOOK_POST);
        b.objects[1].multiplier = 7;
        b.objects[1].run = true;
        mod.Stop();
        check(b.objects[1].multiplier == 7 && b.objects[1].run,
              "Later gameplay settings are not overwritten");
        check(b.clean(), "Stamina changed-ownership teardown");
        check(mod.Load(b) == BC_OK, "Stamina next session loads");
        b.pump();
        b.event(":ResetStaminaToMax", BC_HOOK_POST);
        b.remove(1);
        mod.Stop();
        check(b.clean(), "Deleted Spy is forgotten without duplicate release");
    }
    for (unsigned failure = 1; failure <= 7; ++failure) {
        Backend b(staminaConfig(0));
        Module mod(L"Briefcase.SuspicionControl.dll");
        if (failure <= 4)
            b.failResolve = failure;
        else
            b.failHook = failure - 4;
        check(mod.Load(b) == BC_OK, "Stamina accepts deferred setup");
        b.pump();
        check(b.clean(), "Stamina partial setup cleans all resources");
        mod.Stop();
        check(b.clean(), "Stamina failed setup stop");
    }
    {
        Backend b(staminaConfig(0));
        Module mod(L"Briefcase.SuspicionControl.dll");
        check(mod.Load(b) == BC_OK, "Stamina loads before cancellation");
        mod.Stop();
        b.pump();
        check(b.resolves == 0 && b.clean(), "Queued stamina initialization after stop is ignored");
        b.rejectPost = true;
        check(mod.Load(b) == BC_DENIED, "Stamina reports queue failure");
        b.rejectPost = false;
        check(mod.Load(b) == BC_OK, "Stamina reloads after queue failure");
        b.pump();
        b.rejectWrite = true;
        b.event(":ResetStaminaToMax", BC_HOOK_POST);
        mod.Stop();
        check(b.clean(), "Stamina callback failure releases resources");
    }
    {
        Backend b(staminaConfig(.5));
        Module mod(L"Briefcase.SuspicionControl.dll");
        b.config["diagnostics"] = false;
        check(mod.Load(b) == BC_OK, "Stamina loads without diagnostics");
        b.pump();
        b.objects[1].role = 2;
        b.event(":ResetStaminaToMax", BC_HOOK_POST);
        check(b.objects[1].multiplier == 2, "Non-authoritative Spy ignored");
        b.objects[1].role = 3;
        b.delta = 20;
        b.event(":ReduceStamina", BC_HOOK_PRE);
        b.event(":ReduceStamina", BC_HOOK_POST);
        check(b.delta == 10 && b.objects[1].multiplier == 1, "Gameplay independent from diagnostics");
        mod.Stop();
        check(b.clean(), "Stamina no-diagnostics teardown");
    }
}
static void prefixCompatibility() {
    {
        Backend b(staminaConfig(0));
        Module mod(L"Briefcase.SuspicionControl.dll");
        b.unreal.size = offsetof(BcUnrealApi, write_argument);
        check(mod.Load(b) == BC_INVALID_ARGUMENT && !b.task, "Stamina requires the argument-write extension");
        b.unreal.size = offsetof(BcUnrealApi, invoke_outputs);
        check(mod.Load(b) == BC_OK, "Stamina accepts complete server prefix without client tail");
        b.pump();
        mod.Stop();
        check(b.clean(), "Server-prefix stamina cleanup");
    }
}
int main() { try { stamina(); prefixCompatibility(); std::cout << "PASS " << checks << " lifecycle checks\n"; return 0; } catch(const std::exception& e) { std::cerr << e.what() << "\n"; return 1; } }
