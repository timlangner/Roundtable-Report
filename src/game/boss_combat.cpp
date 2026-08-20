#include "game/boss_combat.hpp"

#include "game/item_names.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace erstats {
namespace {

std::vector<BossBarSlot> mapped_bars(const std::vector<BossBarSlot>& bars) {
    std::vector<BossBarSlot> mapped;
    mapped.reserve(bars.size());
    for (const auto& bar : bars) {
        if (!boss_handle_occupied(bar.handle)) {
            continue;
        }
        if (npc_display_name(bar.npc_param).empty()) {
            continue;
        }
        mapped.push_back(bar);
    }
    return mapped;
}

std::string encounter_from_bars(const std::vector<BossBarSlot>& mapped) {
    std::vector<int32_t> params;
    params.reserve(mapped.size());
    for (const auto& bar : mapped) {
        params.push_back(bar.npc_param);
    }
    return format_encounter_name(params);
}

std::string last_killed_from_flag_diff(
    const std::vector<std::string>& previous, const std::vector<std::string>& current) {
    std::vector<std::string> added;
    for (const auto& name : current) {
        if (std::find(previous.begin(), previous.end(), name) == previous.end()) {
            added.push_back(name);
        }
    }
    if (added.empty()) {
        return {};
    }
    std::set<std::string> unique(added.begin(), added.end());
    if (unique.count("Godskin Apostle") && unique.count("Godskin Noble")) {
        return "Godskin Duo";
    }
    if (unique.size() == 1 && *unique.begin() == "Valiant Gargoyle" && added.size() >= 2) {
        return "Valiant Gargoyles";
    }
    return added.back();
}

uint32_t attacking_weapon_id(const EquippedLoadout& loadout) {
    if (!item_slot_empty(loadout.attack_id)) {
        return loadout.attack_id;
    }
    if (!item_slot_empty(loadout.right_id)) {
        return loadout.right_id;
    }
    if (!item_slot_empty(loadout.left_id)) {
        return loadout.left_id;
    }
    return 0;
}

std::string pick_dealt_damage(const BossCombatTracker& tracker, const EquippedLoadout& loadout) {
    if (!tracker.credited_hp.empty()) {
        uint32_t winner = 0;
        int32_t best = -1;
        for (const auto& [id, drop] : tracker.credited_hp) {
            if (drop > best || (drop == best && id == loadout.right_id)) {
                best = drop;
                winner = id;
            }
        }
        auto name = format_weapon_name(winner);
        return name.empty() ? "No weapons" : name;
    }
    auto name = format_weapon_name(attacking_weapon_id(loadout));
    return name.empty() ? "No weapons" : name;
}

BossDeathRecord make_death_record(
    const BossCombatTracker& tracker, const EquippedLoadout& loadout) {
    BossDeathRecord record;
    record.encounter = tracker.latch_encounter;
    record.hp_pct = tracker.latch_hp_pct;
    record.right_weapon = format_weapon_name(loadout.right_id);
    record.left_weapon = format_weapon_name(loadout.left_id);
    record.dealt_damage = pick_dealt_damage(tracker, loadout);
    for (const uint32_t id : loadout.talisman_ids) {
        auto name = format_talisman_name(id);
        if (!name.empty()) {
            record.talismans.push_back(std::move(name));
        }
    }
    return record;
}

bool should_replace_best(const BossDeathRecord* existing, const BossDeathRecord& incoming) {
    if (existing == nullptr) {
        return true;
    }
    if (incoming.hp_pct && existing->hp_pct) {
        return *incoming.hp_pct < *existing->hp_pct;
    }
    if (incoming.hp_pct && !existing->hp_pct) {
        return true;
    }
    return false;
}

void consider_best(std::map<std::string, BossDeathRecord>& map, const BossDeathRecord& record) {
    auto& slot = map[record.encounter];
    const BossDeathRecord* existing =
        slot.encounter.empty() && !slot.hp_pct ? nullptr : &slot;
    if (should_replace_best(existing, record)) {
        slot = record;
    }
}

}  // namespace

std::optional<uint8_t> combined_hp_pct(const std::vector<BossBarSlot>& bars) {
    int64_t hp = 0;
    int64_t max = 0;
    for (const auto& bar : bars) {
        if (bar.hp_max <= 0) {
            continue;
        }
        hp += bar.hp < 0 ? 0 : bar.hp;
        max += bar.hp_max;
    }
    if (max <= 0) {
        return std::nullopt;
    }
    if (hp <= 0) {
        return static_cast<uint8_t>(0);
    }
    const int rounded = static_cast<int>(std::lround(100.0 * static_cast<double>(hp) / static_cast<double>(max)));
    if (rounded < 1) {
        return static_cast<uint8_t>(1);
    }
    if (rounded > 100) {
        return static_cast<uint8_t>(100);
    }
    return static_cast<uint8_t>(rounded);
}

void tick_boss_combat(BossCombatTracker& tracker, const BossCombatInput& input, uint64_t now_ms) {
    if (!tracker.initialized) {
        tracker.last_deaths = input.deaths;
        tracker.prev_bosses_down = input.bosses_down;
        tracker.initialized = true;
    }

    const auto mapped = mapped_bars(input.bars);
    const std::string encounter = encounter_from_bars(mapped);
    const bool any_mapped = !encounter.empty();
    const bool died = input.deaths > tracker.last_deaths;
    bool hud_killed = false;

    if (any_mapped) {
        tracker.latch_encounter = encounter;
        tracker.latch_bars_were_up = true;
        tracker.latch_until_ms = now_ms + kBossLatchGraceMs;
        if (const auto pct = combined_hp_pct(mapped)) {
            tracker.latch_hp_pct = pct;
            if (*pct == 0) {
                tracker.latch_saw_zero_hp = true;
            } else {
                tracker.latch_saw_zero_hp = false;
            }
        }

        uint32_t hp_sum = 0;
        for (const auto& bar : mapped) {
            hp_sum += static_cast<uint32_t>(bar.hp < 0 ? 0 : bar.hp);
        }
        if (tracker.have_hp_sum && hp_sum < tracker.last_hp_sum) {
            const int32_t drop = static_cast<int32_t>(tracker.last_hp_sum - hp_sum);
            const uint32_t attack = attacking_weapon_id(input.loadout);
            if (!item_slot_empty(attack)) {
                tracker.credited_hp[attack] += drop;
            }
        }
        tracker.last_hp_sum = hp_sum;
        tracker.have_hp_sum = true;
    } else {
        if (tracker.latch_bars_were_up) {
            tracker.latch_until_ms = now_ms + kBossLatchGraceMs;
            tracker.latch_bars_were_up = false;
        }
        const bool latch_live = !tracker.latch_encounter.empty() && now_ms <= tracker.latch_until_ms;
        if (tracker.latch_saw_zero_hp && latch_live && !died) {
            tracker.last_killed = tracker.latch_encounter;
            hud_killed = true;
        }
        tracker.latch_saw_zero_hp = false;
        tracker.have_hp_sum = false;
    }

    const bool latch_live = !tracker.latch_encounter.empty() && now_ms <= tracker.latch_until_ms;
    if (died && latch_live) {
        const auto record = make_death_record(tracker, input.loadout);
        consider_best(tracker.best_by_encounter, record);
        consider_best(tracker.session_best_by_encounter, record);
        tracker.last_boss = tracker.latch_encounter;
        tracker.credited_hp.clear();
        tracker.latch_saw_zero_hp = false;
    }

    if (!hud_killed) {
        if (auto from_flags =
                last_killed_from_flag_diff(tracker.prev_bosses_down, input.bosses_down);
            !from_flags.empty()) {
            tracker.last_killed = std::move(from_flags);
        }
    }

    tracker.last_deaths = input.deaths;
    tracker.prev_bosses_down = input.bosses_down;
}

void apply_best_to_snapshot(LiveSnapshot& live, const BossCombatTracker& tracker) {
    live.last_boss = tracker.last_boss;
    live.last_killed = tracker.last_killed;
    live.boss_death_best = tracker.best_by_encounter;
    live.last_boss_hp_pct.reset();
    if (tracker.last_boss.empty()) {
        return;
    }
    const auto it = tracker.session_best_by_encounter.find(tracker.last_boss);
    if (it == tracker.session_best_by_encounter.end()) {
        return;
    }
    live.last_boss_hp_pct = it->second.hp_pct;
}

void apply_current_loadout_to_snapshot(LiveSnapshot& live, const EquippedLoadout& loadout) {
    if (live.last_boss.empty()) {
        return;
    }
    const bool has_weapon =
        !item_slot_empty(loadout.right_id) || !item_slot_empty(loadout.left_id);
    bool has_talisman = false;
    for (const uint32_t id : loadout.talisman_ids) {
        if (id != 0 && !item_slot_empty(id)) {
            has_talisman = true;
            break;
        }
    }
    if (!has_weapon && !has_talisman) {
        return;
    }
    live.last_boss_right_weapon = format_weapon_name(loadout.right_id);
    live.last_boss_left_weapon = format_weapon_name(loadout.left_id);
    live.last_boss_talismans.clear();
    for (const uint32_t id : loadout.talisman_ids) {
        auto name = format_talisman_name(id);
        if (!name.empty()) {
            live.last_boss_talismans.push_back(std::move(name));
        }
    }
}

void restore_tracker_from_snapshot(BossCombatTracker& tracker, const LiveSnapshot& live) {
    tracker = {};
    tracker.last_boss = live.last_boss;
    tracker.last_killed = live.last_killed;
    tracker.best_by_encounter = live.boss_death_best;
    tracker.last_deaths = live.deaths;
    tracker.prev_bosses_down = live.bosses_down;
    tracker.initialized = true;
}

void clear_session_boss_best(BossCombatTracker& tracker) {
    tracker.session_best_by_encounter.clear();
}

}  // namespace erstats
