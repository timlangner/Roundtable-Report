#include "run/save_headers.hpp"

#include "util.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <cstring>

namespace erstats {
namespace {

uint32_t read_u32(std::span<const uint8_t> data, size_t offset) {
    uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

std::optional<std::vector<uint8_t>> aes128_cbc_decrypt(
    std::span<const uint8_t> key,
    std::span<const uint8_t> iv,
    std::span<const uint8_t> cipher) {
    if (key.size() != 16 || iv.size() != 16 || cipher.empty() || (cipher.size() % 16) != 0) {
        return std::nullopt;
    }

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key_handle = nullptr;
    std::vector<uint8_t> plain(cipher.size());

    const auto cleanup = [&]() {
        if (key_handle) {
            BCryptDestroyKey(key_handle);
        }
        if (alg) {
            BCryptCloseAlgorithmProvider(alg, 0);
        }
    };

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
        cleanup();
        return std::nullopt;
    }
    if (BCryptSetProperty(
            alg,
            BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
            sizeof(BCRYPT_CHAIN_MODE_CBC),
            0)
        != 0) {
        cleanup();
        return std::nullopt;
    }

    BCRYPT_KEY_DATA_BLOB_HEADER header{};
    header.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
    header.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
    header.cbKeyData = static_cast<ULONG>(key.size());
    std::vector<uint8_t> blob(sizeof(header) + key.size());
    std::memcpy(blob.data(), &header, sizeof(header));
    std::memcpy(blob.data() + sizeof(header), key.data(), key.size());

    if (BCryptImportKey(
            alg, nullptr, BCRYPT_KEY_DATA_BLOB, &key_handle, nullptr, 0, blob.data(),
            static_cast<ULONG>(blob.size()), 0)
        != 0) {
        cleanup();
        return std::nullopt;
    }

    ULONG written = 0;
    UCHAR iv_copy[16]{};
    std::memcpy(iv_copy, iv.data(), 16);
    const NTSTATUS status = BCryptDecrypt(
        key_handle,
        const_cast<PUCHAR>(cipher.data()),
        static_cast<ULONG>(cipher.size()),
        nullptr,
        iv_copy,
        16,
        plain.data(),
        static_cast<ULONG>(plain.size()),
        &written,
        0);
    cleanup();
    if (status != 0) {
        return std::nullopt;
    }
    plain.resize(written);
    return plain;
}

}  // namespace

bool starts_with_bnd4(std::span<const uint8_t> data) {
    return data.size() >= 4 && std::memcmp(data.data(), "BND4", 4) == 0;
}

std::vector<uint8_t> maybe_decrypt_sl2(std::vector<uint8_t> data) {
    if (starts_with_bnd4(data)) {
        return data;
    }
    if (data.size() < 32) {
        return {};
    }
    const std::span<const uint8_t> iv(data.data(), 16);
    const std::span<const uint8_t> cipher(data.data() + 16, data.size() - 16);
    auto plain = aes128_cbc_decrypt(kSl2AesKey, iv, cipher);
    if (!plain || !starts_with_bnd4(*plain)) {
        return {};
    }
    return std::move(*plain);
}

std::optional<std::vector<SlotHeader>> parse_save_headers(std::span<const uint8_t> data) {
    const size_t needed = kHeaderStartOffset + kHeaderStride * static_cast<size_t>(kSlotCount);
    if (data.size() < needed || !starts_with_bnd4(data)) {
        return std::nullopt;
    }

    std::vector<SlotHeader> slots;
    slots.reserve(kSlotCount);
    for (int i = 0; i < kSlotCount; ++i) {
        SlotHeader slot;
        slot.index = i;
        slot.active = data[kActiveFlagsOffset + static_cast<size_t>(i)] == 1;
        const size_t base = kHeaderStartOffset + static_cast<size_t>(i) * kHeaderStride;
        slot.name = read_utf16le(data.data() + base, kHeaderNameBytes);
        slot.level = read_u32(data, base + kHeaderLevelOffset);
        slot.playtime_seconds = read_u32(data, base + kHeaderPlaytimeOffset);
        slots.push_back(std::move(slot));
    }
    return slots;
}

std::vector<uint8_t> make_synthetic_save(const std::vector<SlotHeader>& slots, size_t minimum_size) {
    std::vector<uint8_t> data(minimum_size, 0);
    std::memcpy(data.data(), "BND4", 4);
    for (const auto& slot : slots) {
        if (slot.index < 0 || slot.index >= kSlotCount) {
            continue;
        }
        data[kActiveFlagsOffset + static_cast<size_t>(slot.index)] = slot.active ? 1 : 0;
        const size_t base = kHeaderStartOffset + static_cast<size_t>(slot.index) * kHeaderStride;
        const std::wstring wide = utf8_to_wide(slot.name);
        for (size_t i = 0; i < wide.size() && (i * 2 + 1) < kHeaderNameBytes; ++i) {
            data[base + i * 2] = static_cast<uint8_t>(wide[i] & 0xFF);
            data[base + i * 2 + 1] = static_cast<uint8_t>((wide[i] >> 8) & 0xFF);
        }
        std::memcpy(data.data() + base + kHeaderLevelOffset, &slot.level, sizeof(slot.level));
        std::memcpy(
            data.data() + base + kHeaderPlaytimeOffset, &slot.playtime_seconds,
            sizeof(slot.playtime_seconds));
    }
    return data;
}

}  // namespace erstats
