#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace erstats {

enum class MapAtlas {
    LandsBetween,
    Shadow,
    Underground,
};

struct MapPin {
    MapAtlas atlas = MapAtlas::LandsBetween;
    float u = 0.5f;
    float v = 0.5f;
    bool valid = false;
};

struct MapIdParts {
    uint8_t area = 0;
    uint8_t grid_x = 0;
    uint8_t grid_z = 0;
    uint8_t size = 0;
};

std::string format_journey(uint32_t ng_cycle);
std::string format_boss_status(bool in_boss_fight);
bool boss_flag_active(uint32_t raw);
std::string location_from_map_id(uint32_t map_id);
bool is_shadow_realm(uint32_t map_id);
bool shows_dlc_stats(uint32_t map_id, uint8_t scadutree_blessing, uint8_t revered_ash);
MapIdParts parse_map_id(uint32_t map_id);
std::pair<float, float> overworld_global_xz(uint32_t map_id, float local_x, float local_z);
MapPin pin_from_world(uint32_t map_id, float x, float y, float z, std::string_view location_name);

}  // namespace erstats
