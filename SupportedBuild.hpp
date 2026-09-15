#pragma once
#include <Briefcase/ModApi.h>
#include <cstring>
namespace server_mods {
inline bool supported_build(const BcApi *api) {
    if (!api || api->version != BC_API_VERSION || api->size < sizeof(BcApi))
        return false;
    BcBuild b{};
    b.size = sizeof(b);
    return api->get_build(api->context, &b) == BC_OK && b.pe_timestamp == 0x6a966107 &&
           b.image_size == 0x05b60000 && b.engine_major == 4 && b.engine_minor == 27 &&
           std::strcmp(b.executable_sha256,
                       "78afe1dbeecb09027c274def4f0ac855b447dc52ffe3cd9482c1be4341b0dae6") == 0;
}
} // namespace server_mods
