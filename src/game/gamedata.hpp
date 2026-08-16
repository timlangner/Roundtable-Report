#pragma once

#include "run/snapshot.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace erstats {

class GameDataReader {
public:
    bool locate();
    bool ready() const { return game_data_man_ != 0; }
    bool world_ready() const { return world_chr_man_slot_ != 0; }
    uintptr_t game_data_man() const { return game_data_man_; }

    LiveSnapshot read() const;

    // Test helper: interpret a synthetic GameDataMan / PlayerGameData layout.
    static LiveSnapshot read_from_buffers(
        std::span<const uint8_t> game_data_man,
        std::span<const uint8_t> player_game_data,
        uint32_t map_id = 0);

private:
    uintptr_t game_data_man_ = 0;
    uintptr_t world_chr_man_slot_ = 0;
    uintptr_t game_man_slot_ = 0;
    uintptr_t event_flag_slot_ = 0;
};

std::optional<uintptr_t> scan_gamedataman(std::span<const uint8_t> image, uintptr_t image_base);
std::optional<uintptr_t> scan_worldchrman(std::span<const uint8_t> image, uintptr_t image_base);

}  // namespace erstats
