#include "game/aob.hpp"

#include <cctype>
#include <cstdlib>

namespace erstats {
namespace {

int hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

}  // namespace

CompiledPattern compile_ida_pattern(std::string_view ida) {
    CompiledPattern out;
    size_t i = 0;
    while (i < ida.size()) {
        while (i < ida.size() && std::isspace(static_cast<unsigned char>(ida[i]))) {
            ++i;
        }
        if (i >= ida.size()) {
            break;
        }
        if (ida[i] == '?') {
            out.bytes.push_back(0);
            out.mask.push_back(0);
            ++i;
            if (i < ida.size() && ida[i] == '?') {
                ++i;
            }
            continue;
        }
        if (i + 1 >= ida.size()) {
            break;
        }
        const int hi = hex_nibble(ida[i]);
        const int lo = hex_nibble(ida[i + 1]);
        if (hi < 0 || lo < 0) {
            ++i;
            continue;
        }
        out.bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
        out.mask.push_back(1);
        i += 2;
    }
    return out;
}

std::optional<size_t> find_pattern_from(
    std::span<const uint8_t> haystack, const CompiledPattern& pattern, size_t start) {
    if (pattern.bytes.empty() || haystack.size() < pattern.bytes.size()) {
        return std::nullopt;
    }
    const size_t last = haystack.size() - pattern.bytes.size();
    if (start > last) {
        return std::nullopt;
    }
    for (size_t i = start; i <= last; ++i) {
        bool ok = true;
        for (size_t j = 0; j < pattern.bytes.size(); ++j) {
            if (pattern.mask[j] && haystack[i + j] != pattern.bytes[j]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> find_pattern(std::span<const uint8_t> haystack, const CompiledPattern& pattern) {
    return find_pattern_from(haystack, pattern, 0);
}

std::optional<uintptr_t> resolve_rel32(
    std::span<const uint8_t> haystack,
    size_t match_offset,
    int rel32_offset,
    int instruction_size,
    uintptr_t buffer_base) {
    const size_t disp_at = match_offset + static_cast<size_t>(rel32_offset);
    if (disp_at + 4 > haystack.size()) {
        return std::nullopt;
    }
    int32_t disp = 0;
    disp |= haystack[disp_at];
    disp |= static_cast<int32_t>(haystack[disp_at + 1]) << 8;
    disp |= static_cast<int32_t>(haystack[disp_at + 2]) << 16;
    disp |= static_cast<int32_t>(haystack[disp_at + 3]) << 24;
    const uintptr_t next_ip = buffer_base + match_offset + static_cast<uintptr_t>(instruction_size);
    return next_ip + static_cast<intptr_t>(disp);
}

std::optional<uintptr_t> find_rip_pointer(
    std::span<const uint8_t> haystack,
    std::string_view ida,
    int rel32_offset,
    int instruction_size,
    uintptr_t buffer_base) {
    const auto compiled = compile_ida_pattern(ida);
    const auto match = find_pattern(haystack, compiled);
    if (!match) {
        return std::nullopt;
    }
    return resolve_rel32(haystack, *match, rel32_offset, instruction_size, buffer_base);
}

std::optional<uintptr_t> find_function(
    std::span<const uint8_t> haystack, std::string_view ida, uintptr_t buffer_base) {
    const auto compiled = compile_ida_pattern(ida);
    const auto match = find_pattern(haystack, compiled);
    if (!match) {
        return std::nullopt;
    }
    return buffer_base + *match;
}

}  // namespace erstats
