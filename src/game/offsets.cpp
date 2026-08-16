#include "game/offsets.hpp"

namespace erstats {

const Offsets& current_offsets() {
    static const Offsets kOffsets{};
    return kOffsets;
}

const std::vector<AobPattern>& gamedataman_patterns() {
    static const std::vector<AobPattern> kPatterns = {
        {"tga_gamedataman", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3", 3, 7},
        {"alt_gamedataman", "48 8B 05 ?? ?? ?? ?? 33 DB 48 8B 48 08", 3, 7},
    };
    return kPatterns;
}

const std::vector<AobPattern>& worldchrman_patterns() {
    static const std::vector<AobPattern> kPatterns = {
        {"soulmemory_worldchrman",
         "48 8B 35 ?? ?? ?? ?? 48 85 F6 ?? ?? BB 01 00 00 00 89 5C 24 20 48 8B B6", 3, 7},
        {"tga_worldchrman", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0F 48 39 88", 3, 7},
        {"tga_alt_worldchrman", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 58 08", 3, 7},
        {"mov_rcx_worldchrman", "48 8B 0D ?? ?? ?? ?? 48 85 C9 75 ?? 48 8D 0D", 3, 7},
    };
    return kPatterns;
}

const std::vector<AobPattern>& gameman_patterns() {
    static const std::vector<AobPattern> kPatterns = {
        {"tga_gameman", "48 8B 05 ?? ?? ?? ?? 80 B8 ?? ?? ?? ?? 00 0F 94 C0 C3", 3, 7},
        {"tga_gameman_alt", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 80 B8", 3, 7},
    };
    return kPatterns;
}

const std::vector<AobPattern>& eventflag_patterns() {
    static const std::vector<AobPattern> kPatterns = {
        {"soulmemory_vmf", "44 89 7C 24 28 4C 8B 25 ?? ?? ?? ?? 4D 85 E4", 8, 12},
        {"eventflagman", "48 8B 3D ?? ?? ?? ?? 48 85 FF ?? ?? 32 C0 E9", 3, 7},
        {"eventflagman_alt", "48 8B 0D ?? ?? ?? ?? 44 8B C6 89 9E", 3, 7},
    };
    return kPatterns;
}

}  // namespace erstats
