#pragma once

#include "run/snapshot.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace erstats {

inline constexpr uint64_t kBossLatchGraceMs = 60'000;

struct BossBarSlot {
    uint64_t handle = 0;
    int32_t npc_param = 0;
    int32_t hp = 0;
    int32_t hp_max = 0;
};

struct EquippedLoadout {
    uint32_t right_id = 0;
    uint32_t left_id = 0;
    uint32_t attack_id = 0;
    std::array<uint32_t, 4> talisman_ids{};
    bool two_handing = false;
};

struct BossCombatTracker {
    bool initialized = false;
    std::string latch_encounter;
    std::optional<uint8_t> latch_hp_pct;
    bool latch_saw_zero_hp = false;
    bool latch_bars_were_up = false;
    uint64_t latch_until_ms = 0;
    bool have_hp_sum = false;
    uint32_t last_hp_sum = 0;
    std::map<uint32_t, int32_t> credited_hp;
    uint32_t last_deaths = 0;
    std::vector<std::string> prev_bosses_down;
    std::string last_boss;
    std::string last_killed;
    std::map<std::string, BossDeathRecord> best_by_encounter;
    std::map<std::string, BossDeathRecord> session_best_by_encounter;
};

struct BossCombatInput {
    std::vector<BossBarSlot> bars;
    EquippedLoadout loadout;
    uint32_t deaths = 0;
    std::vector<std::string> bosses_down;
};

inline bool boss_handle_occupied(uint64_t handle) {
    const uint32_t selector = static_cast<uint32_t>(handle);
    if (selector == 0 || selector == 0xFFFFFFFFu) {
        return false;
    }
    const uint32_t field_ins_type = selector >> 28;
    return field_ins_type <= 8;
}

inline bool boss_bar_ids_plausible(int32_t fmg_id, uint64_t handle) {
    const bool fmg_ok =
        fmg_id == -1 || fmg_id == 0 || (fmg_id >= 1 && fmg_id < 2'000'000'000);
    if (!fmg_ok) {
        return false;
    }
    const uint32_t selector = static_cast<uint32_t>(handle);
    if (selector == 0 || selector == 0xFFFFFFFFu) {
        return true;
    }
    const uint32_t field_ins_type = selector >> 28;
    return field_ins_type <= 8;
}

std::optional<uint8_t> combined_hp_pct(const std::vector<BossBarSlot>& bars);
void tick_boss_combat(BossCombatTracker& tracker, const BossCombatInput& input, uint64_t now_ms);
void apply_best_to_snapshot(LiveSnapshot& live, const BossCombatTracker& tracker);
void apply_current_loadout_to_snapshot(LiveSnapshot& live, const EquippedLoadout& loadout);
void restore_tracker_from_snapshot(BossCombatTracker& tracker, const LiveSnapshot& live);
void clear_session_boss_best(BossCombatTracker& tracker);

}  // namespace erstats
