#pragma once

#include "game/progress.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace erstats {

struct BossDeathRecord {
    std::string encounter;
    std::optional<uint8_t> hp_pct;
    std::string right_weapon;
    std::string left_weapon;
    std::string dealt_damage;
    std::vector<std::string> talismans;
};

struct LiveSnapshot {
    bool character_loaded = false;
    std::string character_name;
    uint32_t deaths = 0;
    uint32_t session_deaths = 0;
    uint32_t level = 0;
    uint32_t runes = 0;
    uint32_t rune_memory = 0;
    uint32_t igt_ms = 0;
    uint32_t session_ms = 0;
    uint32_t ng_cycle = 0;
    uint32_t map_id = 0;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    bool has_position = false;
    bool in_boss_fight = false;
    uint8_t scadutree_blessing = 0;
    uint8_t revered_ash = 0;
    CharacterStats stats;
    FlaskInfo flasks;
    uint32_t last_grace_id = 0;
    std::string last_grace;
    std::string last_boss;
    std::string last_killed;
    std::optional<uint8_t> last_boss_hp_pct;
    std::string last_boss_right_weapon;
    std::string last_boss_left_weapon;
    std::string last_boss_dealt_damage;
    std::vector<std::string> last_boss_talismans;
    std::map<std::string, BossDeathRecord> boss_death_best;
    std::vector<std::string> bosses_down;
    std::string location = "Unknown";
};

struct RunIdentity {
    std::string steam_id;
    std::string save_filename;
    int slot_index = -1;
    std::string character_name;

    std::string key() const;
    bool valid() const;
};

struct RunSnapshot {
    RunIdentity identity;
    LiveSnapshot live;
    std::string discord_message_id;
    std::string updated_at;
};

inline uint32_t compute_session_deaths(uint32_t current, uint32_t at_load) {
    return current >= at_load ? current - at_load : 0;
}

}  // namespace erstats
