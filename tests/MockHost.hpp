#pragma once
#include <Briefcase/NativeHookApi.h>
#include <Briefcase/StartupApi.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
using HMODULE = void*;
#endif
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
using Json = nlohmann::json;
static unsigned checks;
static void check(bool result, const char *message) {
    ++checks;
    if (!result)
        throw std::runtime_error(message);
}
struct Backend {
    BcApi api{};
    BcUnrealApi unreal{};
    BcNativeHookApi native{};
    BcConfigApi configApi{};
    BcStartupApi startup{};
    Json config;
    Json contracts = Json::parse(
        std::ifstream(std::filesystem::path(MOD_SOURCE) / "tests/data/server-function-contracts.json"));
    struct Object {
        int phase = 1, timer = 90, role = 3;
        uint32_t flags{};
        double multiplier = 2, stamina = 100, cover = .5;
        bool run = true;
    };
    std::map<BcHandle, Object> objects{{1, {}}, {2, {}}};
    std::map<BcHandle, int> refs{{1, 1}, {2, 1}};
    std::map<BcHandle, std::string> functions;
    struct Hook {
        BcHandle function;
        uint32_t phase;
        BcHookCallback callback;
        void *user;
    };
    struct Deleted {
        BcDeletedCallback callback;
        void *user;
    };
    std::map<BcHandle, Hook> hooks;
    std::map<BcHandle, Deleted> deleted;
    BcTask task{};
    void *taskUser{};
    BcHandle next = 100;
    bool gameThread{}, supported = true, rejectPost{}, rejectReadback{}, rejectWrite{};
    unsigned resolves{}, registrations{}, failResolve{}, failHook{}, errors{}, timerWrites{};
    double delta = 20;
    std::string mode = "Solo", maxPlayers;
    std::vector<int> staged;
    std::vector<std::string> logs;
    #ifdef __linux__
    static constexpr auto hash = "b0b275eac71bb8314b8afb5b36368d882faefafc993d5eac05bb5956a7334ef7";
#else
    static constexpr auto hash = "78afe1dbeecb09027c274def4f0ac855b447dc52ffe3cd9482c1be4341b0dae6";
#endif
    static Backend &get(void *c) { return *static_cast<Backend *>(c); }
    BcResult thread() {
        if (gameThread)
            return BC_OK;
        ++errors;
        return BC_WRONG_THREAD;
    }
    BcResult validate(BcHandle h) {
        if (auto e = thread(); e != BC_OK)
            return e;
        return refs.contains(h) && refs.at(h) > 0 ? BC_OK : BC_STALE_HANDLE;
    }
    bool clean() const {
        if (errors || !hooks.empty() || !deleted.empty())
            return false;
        for (auto [h, n] : refs)
            if (n != (objects.contains(h) ? 1 : 0))
                return false;
        return true;
    }
    static BcResult copy(const std::string &s, char *out, uint32_t capacity, uint32_t *needed) {
        *needed = uint32_t(s.size() + 1);
        if (!out || capacity < *needed)
            return BC_LIMIT;
        memcpy(out, s.c_str(), *needed);
        return BC_OK;
    }
    BcResult hook(BcHandle fn, uint32_t phase, BcHookCallback cb, void *user, BcHandle *out) {
        if (auto e = thread(); e != BC_OK)
            return e;
        if (++registrations == failHook)
            return BC_DENIED;
        if (!cb || !user || !functions.contains(fn)) {
            ++errors;
            return BC_INVALID_ARGUMENT;
        }
        *out = next++;
        hooks[*out] = {fn, phase, cb, user};
        return BC_OK;
    }
    Backend(Json cfg) : config(std::move(cfg)) {
        api.size = sizeof(api);
        api.version = BC_API_VERSION;
        api.context = this;
        api.log = [](void *c, uint32_t, const char *s, uint32_t n) -> BcResult {
            get(c).logs.emplace_back(s, n);
            return BC_OK;
        };
        api.get_build = [](void *c, BcBuild *out) -> BcResult {
            *out = {sizeof(*out)};
#ifdef __linux__
            out->pe_timestamp = get(c).supported ? 0 : 1;
            out->image_size = 0;
#else
            out->pe_timestamp = get(c).supported ? 0x6a966107 : 0;
            out->image_size = 0x05b60000;
#endif
            out->engine_major = 4;
            out->engine_minor = 27;
            std::strcpy(out->executable_sha256, hash);
            return BC_OK;
        };
        api.validate_handle = [](void *c, BcHandle h) { return get(c).validate(h); };
        api.release_handle = [](void *c, BcHandle h) -> BcResult {
            auto &b = get(c);
            if (auto e = b.thread(); e != BC_OK)
                return e;
            if (!b.refs.contains(h))
                return BC_STALE_HANDLE;
            if (b.refs[h] <= 0) {
                ++b.errors;
                return BC_STALE_HANDLE;
            }
            --b.refs[h];
            return BC_OK;
        };
        api.post_game_thread = [](void *c, BcTask cb, void *user) -> BcResult {
            auto &b = get(c);
            if (b.rejectPost)
                return BC_DENIED;
            if (b.task || !user) {
                ++b.errors;
                return BC_LIMIT;
            }
            b.task = cb;
            b.taskUser = user;
            return BC_OK;
        };
        api.get_service = [](void *c, const char *name, uint32_t, const void **out) -> BcResult {
            auto &b = get(c);
            if (!strcmp(name, BC_UNREAL_SERVICE))
                *out = &b.unreal;
            else if (!strcmp(name, BC_NATIVE_HOOK_SERVICE))
                *out = &b.native;
            else if (!strcmp(name, BC_CONFIG_SERVICE))
                *out = &b.configApi;
            else if (!strcmp(name, BC_STARTUP_SERVICE))
                *out = &b.startup;
            else
                return BC_NOT_FOUND;
            return BC_OK;
        };
        configApi = {sizeof(configApi), 1,
                     [](void *c, const char *, uint32_t, char *out, uint32_t cap, uint32_t *needed) {
                         return copy(get(c).config.dump(), out, cap, needed);
                     }};
        startup.size = sizeof(startup);
        startup.version = 1;
        startup.read_ini = [](void *c, const char *, const char *, const char *, char *out, uint32_t cap,
                              uint32_t *needed) { return copy(get(c).mode, out, cap, needed); };
        startup.stage_ini = [](void *c, const char *binding, const char *section, const char *key,
                               const char *value) -> BcResult {
            auto &b = get(c);
            if (strcmp(binding, "server-settings") ||
                strcmp(section, "/Script/DeceiveInc.TripwireServerSettings") || strcmp(key, "MaxPlayers")) {
                ++b.errors;
                return BC_INVALID_ARGUMENT;
            }
            b.maxPlayers = value;
            return BC_OK;
        };
        startup.stage_code = [](void* c,const char* sha,const BcCodePatch* p,uint32_t n) -> BcResult {
            if(!sha || std::strcmp(sha,hash) || n!=2 || p[0].window_size!=23 || p[1].window_size!=25) return BC_INVALID_ARGUMENT;
            auto& b=get(c);
            b.staged={p[0].replacement[5],p[0].replacement[17],p[1].replacement[12],p[1].replacement[20]};
            return BC_OK;
        };
        startup.stage_i32 = [](void *c, const char *sha, const BcImmediatePatch *p,
                               uint32_t count) -> BcResult {
            auto &b = get(c);
            if (strcmp(sha, hash) || count != 4) {
                ++b.errors;
                return BC_INVALID_ARGUMENT;
            }
            for (uint32_t i = 0; i < count; ++i)
                b.staged.push_back(p[i].replacement);
            return BC_OK;
        };
        // The deployed server predates the optional client-oriented invocation/enumeration tail.
        unreal.size = offsetof(BcUnrealApi, invoke_outputs);
        unreal.version = 1;
        unreal.resolve_function = [](void *c, const char *path, const char *sig, BcHandle *out) -> BcResult {
            auto &b = get(c);
            if (auto e = b.thread(); e != BC_OK)
                return e;
            if (++b.resolves == b.failResolve)
                return BC_VERSION_MISMATCH;
            if (!b.contracts.contains(path) || b.contracts.at(path) != Json::parse(sig)) {
                ++b.errors;
                return BC_VERSION_MISMATCH;
            }
            *out = b.next++;
            b.functions[*out] = path;
            b.refs[*out] = 1;
            return BC_OK;
        };
        unreal.enum_value = [](void *c, const char *, const char *, int64_t *out) -> BcResult {
            if (auto e = get(c).thread(); e != BC_OK)
                return e;
            *out = 1;
            return BC_OK;
        };
        unreal.retain = [](void *c, BcHandle h) -> BcResult {
            auto &b = get(c);
            if (auto e = b.validate(h); e != BC_OK)
                return e;
            ++b.refs[h];
            return BC_OK;
        };
        unreal.object_info = [](void *c, BcHandle h, BcObjectInfo *out) -> BcResult {
            auto &b = get(c);
            if (auto e = b.validate(h); e != BC_OK)
                return e;
            *out = {sizeof(*out)};
            out->flags = b.objects.at(h).flags;
            std::strcpy(out->path, "World.ServerObject");
            return BC_OK;
        };
        unreal.read_property = [](void *c, BcHandle h, const char *name, BcValue *out) -> BcResult {
            auto &b = get(c);
            if (auto e = b.validate(h); e != BC_OK)
                return e;
            auto &o = b.objects.at(h);
            *out = {};
            if (!strcmp(name, "GamePhase") || !strcmp(name, "Role")) {
                out->kind = BC_VALUE_U8;
                out->data.integer = !strcmp(name, "Role") ? o.role : o.phase;
            } else {
                out->kind = BC_VALUE_F32;
                if (!strcmp(name, "StaminaDrainRateMultiplier"))
                    out->data.number = o.multiplier;
                else if (!strcmp(name, "StaminaCurrent"))
                    out->data.number = o.stamina;
                else if (!strcmp(name, "CoverRatio"))
                    out->data.number = o.cover;
                else
                    return BC_NOT_FOUND;
            }
            return BC_OK;
        };
        unreal.write_property = [](void *c, BcHandle h, const char *name, const BcValue *v) -> BcResult {
            auto &b = get(c);
            if (auto e = b.validate(h); e != BC_OK)
                return e;
            if (b.rejectWrite)
                return BC_DENIED;
            if (strcmp(name, "StaminaDrainRateMultiplier") || v->kind != BC_VALUE_F32) {
                ++b.errors;
                return BC_INVALID_ARGUMENT;
            }
            b.objects.at(h).multiplier = v->data.number;
            return BC_OK;
        };
        unreal.invoke = [](void *c, BcHandle self, BcHandle fn, const BcNamedValue *args, uint32_t count,
                           BcValue *out) -> BcResult {
            auto &b = get(c);
            if (auto e = b.validate(self); e != BC_OK)
                return e;
            if (!b.functions.contains(fn)) {
                ++b.errors;
                return BC_STALE_HANDLE;
            }
            const auto &name = b.functions.at(fn);
            auto &o = b.objects.at(self);
            *out = {};
            if (name.ends_with(":GetCurrentPhaseTimeLeftInSeconds")) {
                out->kind = BC_VALUE_I32;
                out->data.integer = o.timer;
            } else if (name.ends_with(":SetCurrentPhaseTimeLeftInSeconds")) {
                if (count != 1 || strcmp(args[0].name, "SecondsLeft") || args[0].value.kind != BC_VALUE_I32) {
                    ++b.errors;
                    return BC_INVALID_ARGUMENT;
                }
                ++b.timerWrites;
                if (!b.rejectReadback)
                    o.timer = int(args[0].value.data.integer);
            } else if (name.ends_with(":GetRunDrainEnabled")) {
                out->kind = BC_VALUE_BOOL;
                out->data.integer = o.run;
            } else if (name.ends_with(":SetRunDrainEnabled")) {
                if (count != 1 || strcmp(args[0].name, "bEnabled") || args[0].value.kind != BC_VALUE_BOOL) {
                    ++b.errors;
                    return BC_INVALID_ARGUMENT;
                }
                o.run = args[0].value.data.integer != 0;
            } else
                return BC_NOT_FOUND;
            return BC_OK;
        };
        unreal.hook = [](void *c, BcHandle fn, uint32_t phase, BcHookCallback cb, void *user, BcHandle *out) {
            return get(c).hook(fn, phase, cb, user, out);
        };
        unreal.on_deleted = [](void *c, BcDeletedCallback cb, void *user, BcHandle *out) -> BcResult {
            auto &b = get(c);
            if (auto e = b.thread(); e != BC_OK)
                return e;
            if (!user) {
                ++b.errors;
                return BC_INVALID_ARGUMENT;
            }
            *out = b.next++;
            b.deleted[*out] = {cb, user};
            return BC_OK;
        };
        unreal.unhook = [](void *c, BcHandle h) -> BcResult {
            auto &b = get(c);
            if (auto e = b.thread(); e != BC_OK)
                return e;
            if (b.hooks.erase(h) || b.deleted.erase(h))
                return BC_OK;
            ++b.errors;
            return BC_NOT_FOUND;
        };
        unreal.read_argument = [](void *c, BcHandle, const char *name, BcValue *out) -> BcResult {
            auto &b = get(c);
            if (auto e = b.thread(); e != BC_OK)
                return e;
            if (strcmp(name, "Delta")) {
                ++b.errors;
                return BC_NOT_FOUND;
            }
            out->kind = BC_VALUE_F32;
            out->data.number = b.delta;
            return BC_OK;
        };
        unreal.write_argument = [](void *c, BcHandle, const char *name, const BcValue *v) -> BcResult {
            auto &b = get(c);
            if (auto e = b.thread(); e != BC_OK)
                return e;
            if (strcmp(name, "Delta") || v->kind != BC_VALUE_F32) {
                ++b.errors;
                return BC_INVALID_ARGUMENT;
            }
            b.delta = v->data.number;
            return BC_OK;
        };
        native = {sizeof(native), 1,
                  [](void *c, BcHandle fn, const char *sha, const BcNativeSite *site, uint32_t phase,
                     BcHookCallback cb, void *user, BcHandle *out) -> BcResult {
                      auto &b = get(c);
                      if (strcmp(sha, hash) || !site || !site->exec_size || !site->target_size) {
                          ++b.errors;
                          return BC_INVALID_ARGUMENT;
                      }
                      return b.hook(fn, phase, cb, user, out);
                  }};
    }
    void pump() {
        check(task && taskUser, "Deferred initialization carries instance");
        auto cb = task;
        auto user = taskUser;
        task = nullptr;
        taskUser = nullptr;
        gameThread = true;
        cb(user);
        gameThread = false;
    }
    void event(std::string_view suffix, uint32_t phase, BcHandle self = 1) {
        auto current = hooks;
        gameThread = true;
        for (auto [id, h] : current)
            if (hooks.contains(id) && h.phase == phase && functions.at(h.function).ends_with(suffix)) {
                BcHookEvent e{sizeof(e), phase, self, h.function, 999, BC_TRANSPORT_NATIVE, 1};
                h.callback(&e, h.user);
            }
        gameThread = false;
    }
    void remove(BcHandle self) {
        auto current = deleted;
        refs.erase(self);
        objects.erase(self);
        gameThread = true;
        for (auto [id, d] : current)
            if (deleted.contains(id))
                d.callback(self, d.user);
        gameThread = false;
    }
};
class Module {
    HMODULE dll_;
    Backend *backend_{};
    BcModLoad load_;
    BcModUnload unload_;

  public:
    explicit Module(const wchar_t *name) {
#ifdef _WIN32
        dll_ = LoadLibraryW((std::filesystem::current_path() / name).c_str());
#else
        auto path=std::filesystem::current_path()/name;
        path.replace_extension(".so");
        dll_=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
        check(dll_ != nullptr, "Load server mod DLL");
#ifdef _WIN32
        load_ = reinterpret_cast<BcModLoad>(GetProcAddress(dll_, "BriefcaseModLoad"));
        unload_ = reinterpret_cast<BcModUnload>(GetProcAddress(dll_, "BriefcaseModUnload"));
#else
        load_ = reinterpret_cast<BcModLoad>(dlsym(dll_,"BriefcaseModLoad"));
        unload_ = reinterpret_cast<BcModUnload>(dlsym(dll_,"BriefcaseModUnload"));
#endif
        check(load_ && unload_, "Server mod lifecycle entrypoints");
    }
    ~Module() {
        Stop();
#ifdef _WIN32
        FreeLibrary(dll_);
#else
        dlclose(dll_);
#endif
    }
    BcResult Load(Backend &b) {
        backend_ = &b;
        return load_(&b.api);
    }
    BcResult InvalidLoad() { return load_(nullptr); }
    void Stop() {
        if (backend_) {
            backend_->gameThread = true;
            unload_();
            backend_->gameThread = false;
            backend_ = nullptr;
        }
    }
};
