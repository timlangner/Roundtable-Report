#include "game/location.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

namespace erstats {
namespace {

// Nested m60/m61 tiles: size 2 is 4x4 small tiles, size 1 is 2x2.
// Local (0,0,0) is the center of whichever tile is loaded. Playable small
// tiles start around 32, so a low index with size 0 is a large/medium LOD.
void small_tile_center(uint8_t grid_x, uint8_t grid_z, uint8_t size, float& sx, float& sz) {
    if (size == 2 || (size != 1 && grid_x <= 20 && grid_z <= 20)) {
        sx = static_cast<float>(grid_x) * 4.f + 1.5f;
        sz = static_cast<float>(grid_z) * 4.f + 1.5f;
        return;
    }
    if (size == 1 || (size != 2 && grid_x < 32 && grid_z < 32)) {
        sx = static_cast<float>(grid_x) * 2.f + 0.5f;
        sz = static_cast<float>(grid_z) * 2.f + 0.5f;
        return;
    }
    sx = static_cast<float>(grid_x);
    sz = static_cast<float>(grid_z);
}

std::string overworld_name(float sx, float sz) {
    const int x = static_cast<int>(std::floor(sx));
    const int z = static_cast<int>(std::floor(sz));
    if (x >= 40 && x <= 46 && z <= 35) {
        return "Weeping Peninsula";
    }
    if (x <= 35 && z >= 40 && z <= 42) {
        return "Moonlight Altar";
    }
    if (x >= 47 && z <= 42) {
        return "Caelid";
    }
    if (x >= 46 && z >= 40 && z <= 42) {
        return "Caelid";
    }
    if (x >= 47 && z >= 42 && z <= 48) {
        return "Greyoll's Dragonbarrow";
    }
    if (x >= 48 && z >= 54) {
        return "Consecrated Snowfield";
    }
    if (z >= 53 && x >= 44) {
        return "Mountaintops of the Giants";
    }
    if (x >= 42 && x <= 47 && z >= 50 && z <= 53) {
        return "Capital Outskirts";
    }
    if (x >= 35 && x <= 39 && z >= 52) {
        return "Mt. Gelmir";
    }
    if (x >= 36 && x <= 45 && z >= 50) {
        return "Altus Plateau";
    }
    if (x >= 33 && x <= 40 && z >= 39 && z <= 51) {
        return "Liurnia of the Lakes";
    }
    if (x >= 39 && x <= 47 && z >= 35 && z <= 41) {
        return "Limgrave";
    }
    return "The Lands Between";
}

std::string shadow_overworld_name(uint8_t x, uint8_t z, uint8_t size) {
    float fsx = 0;
    float fsz = 0;
    small_tile_center(x, z, size, fsx, fsz);
    const int sx = static_cast<int>(std::floor(fsx));
    const int sz = static_cast<int>(std::floor(fsz));

    if (sx >= 52 && sz <= 41) {
        return "Jagged Peak";
    }
    if (sz >= 47 && sx >= 48) {
        return "Scaduview";
    }
    if (sx <= 45 && sz >= 45) {
        return "Ancient Ruins of Rauh";
    }
    if (sx <= 47 && sz >= 45) {
        return "Rauh Base";
    }
    if (sx >= 49 && sz >= 40 && sz <= 43) {
        return "Abyssal Woods";
    }
    if (sz <= 38 && sx <= 50) {
        return "Cerulean Coast";
    }
    if (sx >= 49 && sz <= 40) {
        return "Finger Ruins of Rhia";
    }
    if (sz >= 38 && sz <= 39 && sx >= 46 && sx <= 49) {
        return "Charo's Hidden Grave";
    }
    if (sx >= 47 && sx <= 48 && sz == 44) {
        return "Castle Ensis";
    }
    if (sz >= 44 && sx >= 47) {
        return "Scadu Altus";
    }
    if (sz >= 40 && sx <= 48) {
        return "Gravesite Plain";
    }
    return "Realm of Shadow";
}

struct NamedPin {
    const char* name;
    MapAtlas atlas;
    float u;
    float v;
};

const NamedPin k_named_pins[] = {
    {"Weeping Peninsula", MapAtlas::LandsBetween, 0.50f, 0.89f},
    {"Limgrave", MapAtlas::LandsBetween, 0.45f, 0.78f},
    {"Stormveil Castle", MapAtlas::LandsBetween, 0.41f, 0.70f},
    {"Stranded Graveyard", MapAtlas::LandsBetween, 0.45f, 0.78f},
    {"Divine Tower of Limgrave", MapAtlas::LandsBetween, 0.50f, 0.70f},
    {"Liurnia of the Lakes", MapAtlas::LandsBetween, 0.23f, 0.52f},
    {"Moonlight Altar", MapAtlas::LandsBetween, 0.09f, 0.63f},
    {"Academy of Raya Lucaria", MapAtlas::LandsBetween, 0.14f, 0.41f},
    {"Divine Tower of Liurnia", MapAtlas::LandsBetween, 0.32f, 0.48f},
    {"Ruin-Strewn Precipice", MapAtlas::LandsBetween, 0.27f, 0.26f},
    {"Mt. Gelmir", MapAtlas::LandsBetween, 0.18f, 0.11f},
    {"Volcano Manor", MapAtlas::LandsBetween, 0.18f, 0.11f},
    {"Altus Plateau", MapAtlas::LandsBetween, 0.41f, 0.22f},
    {"Capital Outskirts", MapAtlas::LandsBetween, 0.50f, 0.19f},
    {"Leyndell, Royal Capital", MapAtlas::LandsBetween, 0.59f, 0.19f},
    {"Leyndell, Ashen Capital", MapAtlas::LandsBetween, 0.59f, 0.19f},
    {"Divine Tower of East Altus", MapAtlas::LandsBetween, 0.64f, 0.22f},
    {"Isolated Divine Tower", MapAtlas::LandsBetween, 0.86f, 0.38f},
    {"Caelid", MapAtlas::LandsBetween, 0.68f, 0.70f},
    {"Greyoll's Dragonbarrow", MapAtlas::LandsBetween, 0.73f, 0.56f},
    {"Divine Tower of Caelid", MapAtlas::LandsBetween, 0.77f, 0.59f},
    {"Mountaintops of the Giants", MapAtlas::LandsBetween, 0.77f, 0.11f},
    {"Consecrated Snowfield", MapAtlas::LandsBetween, 0.68f, 0.07f},
    {"Miquella's Haligtree", MapAtlas::LandsBetween, 0.73f, 0.04f},
    {"Crumbling Farum Azula", MapAtlas::LandsBetween, 0.95f, 0.19f},
    {"Stone Platform", MapAtlas::LandsBetween, 0.59f, 0.16f},
    {"Siofra River", MapAtlas::Underground, 0.50f, 0.20f},
    {"Nokron, Eternal City", MapAtlas::Underground, 0.52f, 0.28f},
    {"Ancestral Woods", MapAtlas::Underground, 0.54f, 0.32f},
    {"Mohgwyn Palace", MapAtlas::Underground, 0.68f, 0.22f},
    {"Deeproot Depths", MapAtlas::Underground, 0.70f, 0.78f},
    {"Ainsel River", MapAtlas::Underground, 0.22f, 0.48f},
    {"Nokstella, Eternal City", MapAtlas::Underground, 0.20f, 0.42f},
    {"Lake of Rot", MapAtlas::Underground, 0.24f, 0.62f},
    {"Uhl Palace Ruins", MapAtlas::Underground, 0.26f, 0.50f},
    {"Underground", MapAtlas::Underground, 0.50f, 0.50f},
    {"Belurat, Tower Settlement", MapAtlas::Shadow, 0.26f, 0.50f},
    {"Enir-Ilim", MapAtlas::Shadow, 0.20f, 0.40f},
    {"Shadow Keep", MapAtlas::Shadow, 0.50f, 0.28f},
    {"Specimen Storehouse", MapAtlas::Shadow, 0.52f, 0.24f},
    {"Stone Coffin Fissure", MapAtlas::Shadow, 0.42f, 0.88f},
    {"Finger Birthing Grounds", MapAtlas::Shadow, 0.70f, 0.34f},
    {"Midra's Manse", MapAtlas::Shadow, 0.68f, 0.58f},
    {"Gravesite Plain", MapAtlas::Shadow, 0.34f, 0.56f},
    {"Castle Ensis", MapAtlas::Shadow, 0.42f, 0.46f},
    {"Scadu Altus", MapAtlas::Shadow, 0.54f, 0.36f},
    {"Rauh Base", MapAtlas::Shadow, 0.28f, 0.34f},
    {"Ancient Ruins of Rauh", MapAtlas::Shadow, 0.22f, 0.30f},
    {"Cerulean Coast", MapAtlas::Shadow, 0.38f, 0.80f},
    {"Charo's Hidden Grave", MapAtlas::Shadow, 0.34f, 0.70f},
    {"Jagged Peak", MapAtlas::Shadow, 0.84f, 0.68f},
    {"Foot of the Jagged Peak", MapAtlas::Shadow, 0.76f, 0.72f},
    {"Abyssal Woods", MapAtlas::Shadow, 0.70f, 0.52f},
    {"Scaduview", MapAtlas::Shadow, 0.62f, 0.18f},
    {"Finger Ruins of Rhia", MapAtlas::Shadow, 0.58f, 0.78f},
    {"Fog Rift Catacombs", MapAtlas::Shadow, 0.40f, 0.42f},
    {"Darklight Catacombs", MapAtlas::Shadow, 0.72f, 0.44f},
    {"Belurat Gaol", MapAtlas::Shadow, 0.28f, 0.54f},
    {"Bonny Gaol", MapAtlas::Shadow, 0.60f, 0.40f},
    {"Lamenter's Gaol", MapAtlas::Shadow, 0.32f, 0.72f},
    {"Realm of Shadow", MapAtlas::Shadow, 0.50f, 0.50f},
};

std::optional<MapPin> pin_from_name(std::string_view name) {
    if (name.empty() || name == "Unknown") {
        return std::nullopt;
    }
    for (const auto& entry : k_named_pins) {
        if (name == entry.name) {
            return MapPin{entry.atlas, entry.u, entry.v, true};
        }
    }
    return std::nullopt;
}

bool finite_world(float x, float z) {
    return std::isfinite(x) && std::isfinite(z) && std::fabs(x) < 1.0e6f && std::fabs(z) < 1.0e6f;
}

float clampf(float value, float lo, float hi) {
    return std::max(lo, std::min(hi, value));
}

// Dungeon-to-worldmap conversion anchors (WorldMapLegacyConvParam).
struct ConvAnchor {
    uint8_t area;
    uint8_t block;
    float src_x;
    float src_z;
    uint8_t dst_area;
    uint8_t dst_grid_x;
    uint8_t dst_grid_z;
    float dst_x;
    float dst_z;
};

const ConvAnchor k_conv_anchors[] = {
#include "game/conv_table.inc"
};

const ConvAnchor* find_anchor(uint8_t area, uint8_t block) {
    for (const auto& anchor : k_conv_anchors) {
        if (anchor.area == area && anchor.block == block) {
            return &anchor;
        }
    }
    return nullptr;
}

struct GlobalPos {
    uint8_t area = 0;   // 60 or 61 after resolution
    float gx = 0.f;     // world units, small tile center at index * 256
    float gz = 0.f;
};

// Converts dungeon-local coordinates into m60/m61 world coordinates by
// chaining conversion anchors (e.g. Deeproot m12_03 -> m35 -> m11 -> m60).
std::optional<GlobalPos> resolve_dungeon_world(uint8_t area, uint8_t block, float x, float z) {
    for (int hop = 0; hop < 4; ++hop) {
        const ConvAnchor* anchor = find_anchor(area, block);
        if (anchor == nullptr) {
            return std::nullopt;
        }
        x = x - anchor->src_x + anchor->dst_x;
        z = z - anchor->src_z + anchor->dst_z;
        area = anchor->dst_area;
        block = anchor->dst_grid_x;
        if (area == 60 || area == 61) {
            GlobalPos out;
            out.area = area;
            out.gx = x + static_cast<float>(anchor->dst_grid_x) * 256.f;
            out.gz = z + static_cast<float>(anchor->dst_grid_z) * 256.f;
            return out;
        }
    }
    return std::nullopt;
}

// UV bounds of each atlas image, in small-tile units, calibrated against the
// bundled map images using known landmarks (Leyndell, Academy, Stormveil,
// Haligtree, Redmane, Shadow Keep, Belurat, Ainsel, Siofra, Deeproot).
struct AtlasBounds {
    float min_x;
    float max_x;
    float min_z;  // bottom edge of the image
    float max_z;  // top edge of the image
};

AtlasBounds atlas_bounds(MapAtlas atlas) {
    switch (atlas) {
        case MapAtlas::Shadow:
            // Linear fit of two in-game screenshot matches on realm_of_shadow.png:
            // Viaduct Minor Tower (m61 46,47) and Divine Gate Front Staircase (m20_01).
            return {39.85f, 55.69f, 33.45f, 56.95f};
        case MapAtlas::Underground:
            // Offset fixed against the Palace Approach Ledge-Road in-game
            // reference shot (template-matched embed crop vs. map image).
            return {30.46f, 63.86f, 28.93f, 63.43f};
        case MapAtlas::LandsBetween:
        default:
            return {27.9f, 66.8f, 29.0f, 64.2f};
    }
}

MapPin project_global(MapAtlas atlas, float gx, float gz) {
    const auto bounds = atlas_bounds(atlas);
    const float u = (gx / 256.f - bounds.min_x) / (bounds.max_x - bounds.min_x);
    const float v = 1.f - (gz / 256.f - bounds.min_z) / (bounds.max_z - bounds.min_z);
    return MapPin{atlas, clampf(u, 0.01f, 0.99f), clampf(v, 0.01f, 0.99f), true};
}

}  // namespace

std::string format_journey(uint32_t ng_cycle) {
    if (ng_cycle == 0) {
        return "NG";
    }
    return "NG+" + std::to_string(ng_cycle);
}

std::string format_boss_status(bool in_boss_fight) {
    return in_boss_fight ? "In a boss fight" : "Exploring";
}

bool boss_flag_active(uint32_t raw) {
    return raw == 1;
}

bool looks_like_area(uint8_t area) {
    return area == 60 || area == 61 || (area >= 10 && area <= 51);
}

std::string location_from_map_id(uint32_t map_id) {
    if (map_id == 0 || map_id == 0xFFFFFFFFu) {
        return "Unknown";
    }
    const auto parts = parse_map_id(map_id);
    const uint8_t area = parts.area;
    const uint8_t block = parts.grid_x;
    const uint8_t region = parts.grid_z;

    switch (area) {
        case 10:
            return "Stormveil Castle";
        case 11:
            if (block == 10) {
                return "Roundtable Hold";
            }
            return block >= 5 ? "Leyndell, Ashen Capital" : "Leyndell, Royal Capital";
        case 12:
            if (block == 1 || block == 4) {
                return "Ainsel River";
            }
            if (block == 3) {
                return "Deeproot Depths";
            }
            if (block == 5) {
                return "Mohgwyn Palace";
            }
            if (block == 9) {
                return "Nokron, Eternal City";
            }
            return "Siofra River";
        case 13:
            return "Crumbling Farum Azula";
        case 14:
            return "Academy of Raya Lucaria";
        case 15:
            return "Miquella's Haligtree";
        case 16:
            return "Volcano Manor";
        case 17:
            return "Stranded Graveyard";
        case 18:
            return "Stranded Graveyard";
        case 19:
            return "Stone Platform";
        case 20:
            return block >= 1 ? "Enir-Ilim" : "Belurat, Tower Settlement";
        case 21:
            if (block >= 2) {
                return "Shadow Keep";
            }
            return block >= 1 ? "Specimen Storehouse" : "Shadow Keep";
        case 22:
            return "Stone Coffin Fissure";
        case 25:
            return "Finger Birthing Grounds";
        case 28:
            return "Midra's Manse";
        case 30:
            return "Catacombs";
        case 31:
            return "Cave";
        case 32:
            return "Tunnel";
        case 34:
            switch (block) {
                case 10:
                    return "Divine Tower of Limgrave";
                case 11:
                case 16:
                    return "Divine Tower of Liurnia";
                case 12:
                    return "Sealed Tower";
                case 13:
                    return "Divine Tower of Caelid";
                case 14:
                    return "Divine Tower of East Altus";
                case 15:
                    return "Isolated Divine Tower";
                default:
                    return "Divine Tower";
            }
        case 35:
            return "Subterranean Shunning-Grounds";
        case 39:
            return "Ruin-Strewn Precipice";
        case 40:
            return block >= 2 ? "Darklight Catacombs" : "Fog Rift Catacombs";
        case 41:
            if (block >= 2) {
                return "Lamenter's Gaol";
            }
            return block >= 1 ? "Bonny Gaol" : "Belurat Gaol";
        case 42:
            return block >= 2 ? "Ruined Forge of Starfall Past" : "Ruined Forge Lava Intake";
        case 43:
            return block >= 1 ? "Dragon's Pit" : "Rivermouth Cave";
        case 45:
            return "Ainsel River";
        case 47:
            return "Lake of Rot";
        case 49:
            return "Nokron, Eternal City";
        case 50:
            return "Nokstella, Eternal City";
        case 51:
            return "Ancestral Woods";
        case 60: {
            float sx = 0;
            float sz = 0;
            small_tile_center(block, region, parts.size, sx, sz);
            return overworld_name(sx, sz);
        }
        case 61:
            return shadow_overworld_name(block, region, parts.size);
        default:
            break;
    }

    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "Map m%02u_%02u_%02u", area, block, region);
    return buf;
}

MapIdParts parse_map_id(uint32_t map_id) {
    const uint8_t b0 = static_cast<uint8_t>(map_id & 0xFF);
    const uint8_t b1 = static_cast<uint8_t>((map_id >> 8) & 0xFF);
    const uint8_t b2 = static_cast<uint8_t>((map_id >> 16) & 0xFF);
    const uint8_t b3 = static_cast<uint8_t>((map_id >> 24) & 0xFF);
    // Live memory packs area,x,z,size in low-to-high bytes. Tests and some
    // tools pack area in the high byte. Accept both.
    if (looks_like_area(b0) && !looks_like_area(b3)) {
        return MapIdParts{b0, b1, b2, b3};
    }
    return MapIdParts{b3, b2, b1, b0};
}

std::pair<float, float> overworld_global_xz(uint32_t map_id, float local_x, float local_z) {
    const auto parts = parse_map_id(map_id);
    float sx = 0;
    float sz = 0;
    small_tile_center(parts.grid_x, parts.grid_z, parts.size, sx, sz);
    return {local_x + sx * 256.f, local_z + sz * 256.f};
}

bool is_shadow_realm(uint32_t map_id) {
    const auto parts = parse_map_id(map_id);
    if (parts.area >= 20 && parts.area <= 28) {
        return true;
    }
    if (parts.area == 61) {
        return true;
    }
    if (parts.area == 40 || parts.area == 41 || parts.area == 42) {
        return true;
    }
    return parts.area == 43 && parts.grid_x >= 1;
}

bool shows_dlc_stats(uint32_t map_id, uint8_t scadutree_blessing, uint8_t revered_ash) {
    return scadutree_blessing > 0 || revered_ash > 0 || is_shadow_realm(map_id);
}

MapPin pin_from_world(uint32_t map_id, float x, float y, float z, std::string_view location_name) {
    (void)y;
    const auto parts = parse_map_id(map_id);
    if (parts.area == 61 && finite_world(x, z)) {
        float sx = 0;
        float sz = 0;
        small_tile_center(parts.grid_x, parts.grid_z, parts.size, sx, sz);
        return project_global(MapAtlas::Shadow, x + sx * 256.f, z + sz * 256.f);
    }
    if (parts.area == 60 && finite_world(x, z)) {
        const auto [gx, gz] = overworld_global_xz(map_id, x, z);
        return project_global(MapAtlas::LandsBetween, gx, gz);
    }
    if (finite_world(x, z)) {
        if (const auto global = resolve_dungeon_world(parts.area, parts.grid_x, x, z)) {
            MapAtlas atlas = MapAtlas::LandsBetween;
            if (global->area == 61) {
                atlas = MapAtlas::Shadow;
            } else if (parts.area == 12) {
                // Siofra/Ainsel/Deeproot/Mohgwyn lie beneath the overworld;
                // the underground map shares the overworld coordinate frame.
                atlas = MapAtlas::Underground;
            }
            return project_global(atlas, global->gx, global->gz);
        }
    }
    if (const auto named = pin_from_name(location_name)) {
        return *named;
    }
    if (is_shadow_realm(map_id)) {
        return MapPin{MapAtlas::Shadow, 0.50f, 0.50f, true};
    }
    if ((parts.area >= 45 && parts.area <= 51) || parts.area == 12) {
        return MapPin{MapAtlas::Underground, 0.50f, 0.50f, true};
    }
    if (parts.area != 0) {
        return MapPin{MapAtlas::LandsBetween, 0.50f, 0.50f, true};
    }
    return {};
}

}  // namespace erstats
