#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace erstats {

inline constexpr uint32_t kBnd4Magic = 0x34444E42;  // "BND4"
inline constexpr size_t kActiveFlagsOffset = 0x1901D04;
inline constexpr size_t kHeaderStartOffset = 0x1901D0E;
inline constexpr size_t kHeaderStride = 0x24C;
inline constexpr size_t kHeaderNameBytes = 0x22;
inline constexpr size_t kHeaderLevelOffset = 0x22;
inline constexpr size_t kHeaderPlaytimeOffset = 0x26;
inline constexpr int kSlotCount = 10;

// Steam Cloud AES-128-CBC key documented by EldenRing-SaveForge.
inline constexpr uint8_t kSl2AesKey[16] = {
    0x99, 0xAD, 0x2D, 0x50, 0xED, 0xF2, 0xFB, 0x01,
    0xC5, 0xF3, 0xEC, 0x3A, 0x2B, 0xCA, 0xB6, 0x9D,
};

struct SlotHeader {
    bool active = false;
    int index = 0;
    std::string name;
    uint32_t level = 0;
    uint32_t playtime_seconds = 0;
};

bool starts_with_bnd4(std::span<const uint8_t> data);
std::vector<uint8_t> maybe_decrypt_sl2(std::vector<uint8_t> data);
std::optional<std::vector<SlotHeader>> parse_save_headers(std::span<const uint8_t> data);
std::vector<uint8_t> make_synthetic_save(
    const std::vector<SlotHeader>& slots,
    size_t minimum_size = kHeaderStartOffset + kHeaderStride * kSlotCount);

}  // namespace erstats
