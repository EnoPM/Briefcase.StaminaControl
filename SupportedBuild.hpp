#pragma once
#include <Briefcase/ModApi.h>
#include <cstring>
namespace server_mods {
inline bool supported_build(const BcApi *api) {
    if (!api || api->version != BC_API_VERSION || api->size < sizeof(BcApi))
        return false;
    BcBuild b{};
    b.size = sizeof(b);
#ifdef __linux__
    return api->get_build(api->context, &b)==BC_OK && b.pe_timestamp==0 && b.image_size==0 &&
           b.engine_major==4 && b.engine_minor==27 &&
           std::strcmp(b.executable_sha256,"b0b275eac71bb8314b8afb5b36368d882faefafc993d5eac05bb5956a7334ef7")==0;
#else
    return api->get_build(api->context, &b) == BC_OK && b.pe_timestamp == 0x6ab71ea7 &&
           b.image_size == 0x05afe000 && b.engine_major == 4 && b.engine_minor == 27 &&
           std::strcmp(b.executable_sha256,
                       "f2125f09cbeb7922a4912706cc546477454ce229c15ed477af2731a21c828fd3") == 0;
#endif
}
} // namespace server_mods
