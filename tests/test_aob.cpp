#include <catch2/catch_test_macros.hpp>

#include "game/aob.hpp"
#include "game/gamedata.hpp"
#include "game/offsets.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

using namespace erstats;

TEST_CASE("compile and find IDA pattern with wildcards") {
    const auto pattern = compile_ida_pattern("48 8B 05 ?? ?? ?? ?? 48 85 C0");
    REQUIRE(pattern.bytes.size() == 10);
    REQUIRE(pattern.mask[0] == 1);
    REQUIRE(pattern.mask[3] == 0);

    std::vector<uint8_t> haystack = {
        0x90, 0x90,
        0x48, 0x8B, 0x05, 0x11, 0x22, 0x33, 0x44, 0x48, 0x85, 0xC0,
        0xC3,
    };
    const auto match = find_pattern(haystack, pattern);
    REQUIRE(match);
    REQUIRE(*match == 2);
}

TEST_CASE("find_pattern_from continues after the first match") {
    const auto pattern = compile_ida_pattern("48 8B 0D ?? ?? ?? ??");
    std::vector<uint8_t> haystack(32, 0x90);
    haystack[2] = 0x48;
    haystack[3] = 0x8B;
    haystack[4] = 0x0D;
    haystack[16] = 0x48;
    haystack[17] = 0x8B;
    haystack[18] = 0x0D;
    REQUIRE(*find_pattern(haystack, pattern) == 2);
    REQUIRE(*find_pattern_from(haystack, pattern, 3) == 16);
}

TEST_CASE("resolve RIP-relative pointer from a synthetic instruction") {
    // 48 8B 05 [disp32] at offset 0, instruction size 7, buffer_base 0x1000
    // disp = 0x20, next_ip = 0x1007, target = 0x1027
    std::vector<uint8_t> bytes = {0x48, 0x8B, 0x05, 0x20, 0x00, 0x00, 0x00, 0xC3};
    const auto addr = resolve_rel32(bytes, 0, 3, 7, 0x1000);
    REQUIRE(addr);
    REQUIRE(*addr == 0x1027);
}

TEST_CASE("scan_gamedataman finds the TGA pattern") {
    const auto& patterns = gamedataman_patterns();
    REQUIRE_FALSE(patterns.empty());

    std::vector<uint8_t> image(64, 0x90);
    // TGA bytes: 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3
    const uint8_t insn[] = {
        0x48, 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0,
        0x74, 0x05, 0x48, 0x8B, 0x40, 0x58, 0xC3, 0xC3,
    };
    std::copy(std::begin(insn), std::end(insn), image.begin() + 8);
    const auto ptr = scan_gamedataman(image, 0x140000000);
    REQUIRE(ptr);
    // next_ip = 0x140000000 + 8 + 7 = 0x14000000F, + 0x10 = 0x14000001F
    REQUIRE(*ptr == 0x14000001F);
}

TEST_CASE("scan_csfeman finds the PostureBar pattern") {
    std::vector<uint8_t> image(64, 0x90);
    const uint8_t insn[] = {
        0x48, 0x8B, 0x0D, 0x10, 0x00, 0x00, 0x00, 0x8B, 0xDA, 0x48, 0x85, 0xC9,
        0x75, 0x05, 0x48, 0x8D, 0x0D,
    };
    std::copy(std::begin(insn), std::end(insn), image.begin() + 8);
    const auto ptr = scan_csfeman(image, 0x140000000);
    REQUIRE(ptr);
    REQUIRE(*ptr == 0x14000001F);
}

TEST_CASE("scan_get_chr_ins_from_handle finds the function entry") {
    std::vector<uint8_t> image(64, 0x90);
    const uint8_t insn[] = {
        0x48, 0x83, 0xEC, 0x28, 0xE8, 0x17, 0xFF, 0xFF, 0xFF, 0x48, 0x85, 0xC0,
        0x74, 0x08, 0x48, 0x8B, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3,
    };
    std::copy(std::begin(insn), std::end(insn), image.begin() + 4);
    const auto ptr = scan_get_chr_ins_from_handle(image, 0x140000000);
    REQUIRE(ptr);
    REQUIRE(*ptr == 0x140000004);
}
