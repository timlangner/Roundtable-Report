#pragma once

#include "game/boss_combat.hpp"
#include "run/snapshot.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace erstats {

struct CombatFrame {
    std::vector<BossBarSlot> bars;
    EquippedLoadout loadout;
};

class GameDataReader {
public:
    bool locate();
    bool ready() const { return game_data_man_ != 0; }
    bool world_ready() const { return world_chr_man_slot_ != 0; }
    uintptr_t game_data_man() const { return game_data_man_; }

    LiveSnapshot read() const;
    CombatFrame read_combat() const;

    // Test helper: interpret a synthetic GameDataMan / PlayerGameData layout.
    static LiveSnapshot read_from_buffers(
        std::span<const uint8_t> game_data_man,
        std::span<const uint8_t> player_game_data,
        uint32_t map_id = 0);

    static EquippedLoadout loadout_from_player_bytes(std::span<const uint8_t> player_game_data);
    static std::array<uint64_t, 3> boss_handles_from_csfeman_bytes(std::span<const uint8_t> csfeman);
    static std::array<BossBarSlot, 3> boss_slots_from_csfeman_bytes(std::span<const uint8_t> csfeman);

private:
    uintptr_t game_data_man_ = 0;
    uintptr_t world_chr_man_slot_ = 0;
    uintptr_t game_man_slot_ = 0;
    uintptr_t event_flag_slot_ = 0;
    uintptr_t csfeman_slot_ = 0;
    uintptr_t get_chr_ins_fn_ = 0;
};

std::optional<uintptr_t> scan_gamedataman(std::span<const uint8_t> image, uintptr_t image_base);
std::optional<uintptr_t> scan_worldchrman(std::span<const uint8_t> image, uintptr_t image_base);
std::optional<uintptr_t> scan_csfeman(std::span<const uint8_t> image, uintptr_t image_base);
std::optional<uintptr_t> scan_get_chr_ins_from_handle(
    std::span<const uint8_t> image, uintptr_t image_base);

bool csfeman_layout_plausible(std::span<const uint8_t> csfeman);

struct CsFeManCandidate {
    uintptr_t slot = 0;
    bool unique_pattern = false;
    uintptr_t instance = 0;
    bool layout_ok = false;
};

// Unique AOB hits win even when the instance is still null. Fallback hits
// require a live object that passes layout checks.
std::optional<uintptr_t> pick_csfeman_slot(std::span<const CsFeManCandidate> candidates);

}  // namespace erstats
