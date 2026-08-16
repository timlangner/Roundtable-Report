#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace erstats {

struct CompiledPattern {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;  // 1 = compare, 0 = wildcard
};

CompiledPattern compile_ida_pattern(std::string_view ida);

std::optional<size_t> find_pattern(std::span<const uint8_t> haystack, const CompiledPattern& pattern);

// Resolve RIP-relative qword: match + rel32_offset is a signed 32-bit displacement.
// Absolute address = (match_addr + instruction_size) + displacement.
// When scanning a local buffer, pass buffer_base as the imagined address of haystack[0].
std::optional<uintptr_t> resolve_rel32(
    std::span<const uint8_t> haystack,
    size_t match_offset,
    int rel32_offset,
    int instruction_size,
    uintptr_t buffer_base = 0);

std::optional<uintptr_t> find_rip_pointer(
    std::span<const uint8_t> haystack,
    std::string_view ida,
    int rel32_offset,
    int instruction_size,
    uintptr_t buffer_base = 0);

}  // namespace erstats
