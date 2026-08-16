#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace erstats {

struct InventoryItem {
    uint32_t id = 0;
    uint32_t quantity = 0;
};

struct FlaskInfo {
    uint8_t charges = 0;
    uint8_t tier = 0;
    bool valid = false;
};

struct CharacterStats {
    uint32_t vigor = 0;
    uint32_t mind = 0;
    uint32_t endurance = 0;
    uint32_t strength = 0;
    uint32_t dexterity = 0;
    uint32_t intelligence = 0;
    uint32_t faith = 0;
    uint32_t arcane = 0;
};

struct JourneyBoss {
    uint32_t flag = 0;
    const char* name = nullptr;
};

uint32_t goods_item_id(uint32_t raw_id);
FlaskInfo flasks_from_inventory(const std::vector<InventoryItem>& items);
FlaskInfo flasks_from_allocation(uint8_t hp, uint8_t fp, uint8_t tier);
FlaskInfo prefer_allocated_flasks(const FlaskInfo& allocated, const FlaskInfo& remaining);
bool plausible_stat(uint32_t value);
bool stats_look_valid(const CharacterStats& stats);
std::string format_flasks(const FlaskInfo& flasks);
std::string format_attributes(const CharacterStats& stats);
std::string format_bosses_down(const std::vector<std::string>& names);
std::vector<std::string> merge_bosses_down(
    const std::vector<std::string>& previous, const std::vector<std::string>& current);
std::string grace_name_from_id(uint32_t grace_id);
std::string grace_name_from_bonfire_id(uint32_t bonfire_id);
std::string last_boss_from_location(std::string_view location);
const std::vector<JourneyBoss>& journey_bosses();
bool event_flag_bit(uint32_t remainder, uint8_t packed_byte);

}  // namespace erstats
