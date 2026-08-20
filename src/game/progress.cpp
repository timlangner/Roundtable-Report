#include "game/progress.hpp"

#include <algorithm>
#include <sstream>

namespace erstats {
namespace {

struct GraceEntry {
    uint32_t id;
    const char* name;
};

const GraceEntry k_graces[] = {
#include "game/grace_table.inc"
};

// Bonfire warp ids as stored at GameMan+0xB60 (e.g. 1042362950 = Church of
// Elleh). Distinct from the event-flag ids in grace_table.inc.
const GraceEntry k_bonfires[] = {
#include "game/bonfire_table.inc"
};

bool is_crimson_flask(uint32_t goods) {
    return goods >= 1000 && goods <= 1025;
}

bool is_cerulean_flask(uint32_t goods) {
    return goods >= 1050 && goods <= 1075;
}

uint8_t flask_tier_from_goods(uint32_t goods) {
    if (is_crimson_flask(goods)) {
        return static_cast<uint8_t>((goods - 1000) / 2);
    }
    if (is_cerulean_flask(goods)) {
        return static_cast<uint8_t>((goods - 1050) / 2);
    }
    return 0;
}

}  // namespace

uint32_t goods_item_id(uint32_t raw_id) {
    return raw_id & 0x00FFFFFFu;
}

FlaskInfo flasks_from_inventory(const std::vector<InventoryItem>& items) {
    FlaskInfo out;
    uint32_t charges = 0;
    uint8_t tier = 0;
    bool found = false;
    for (const auto& item : items) {
        const uint32_t goods = goods_item_id(item.id);
        if (!is_crimson_flask(goods) && !is_cerulean_flask(goods)) {
            continue;
        }
        found = true;
        if (item.quantity <= 14) {
            charges += item.quantity;
        }
        tier = std::max(tier, flask_tier_from_goods(goods));
    }
    if (!found) {
        return {};
    }
    if (charges > 14) {
        charges = 14;
    }
    out.charges = static_cast<uint8_t>(charges);
    out.tier = tier;
    out.valid = true;
    return out;
}

FlaskInfo flasks_from_allocation(uint8_t hp, uint8_t fp, uint8_t tier) {
    const unsigned total = static_cast<unsigned>(hp) + static_cast<unsigned>(fp);
    if (hp > 14 || fp > 14 || total < 1 || total > 14) {
        return {};
    }
    FlaskInfo out;
    out.charges = static_cast<uint8_t>(total);
    out.tier = tier;
    out.valid = true;
    return out;
}

FlaskInfo prefer_allocated_flasks(const FlaskInfo& allocated, const FlaskInfo& remaining) {
    if (!allocated.valid) {
        return remaining;
    }
    FlaskInfo out = allocated;
    if (out.tier == 0 && remaining.tier != 0) {
        out.tier = remaining.tier;
    }
    return out;
}

bool plausible_stat(uint32_t value) {
    return value >= 1 && value <= 99;
}

bool stats_look_valid(const CharacterStats& stats) {
    return plausible_stat(stats.vigor) && plausible_stat(stats.mind)
        && plausible_stat(stats.endurance) && plausible_stat(stats.strength)
        && plausible_stat(stats.dexterity) && plausible_stat(stats.intelligence)
        && plausible_stat(stats.faith) && plausible_stat(stats.arcane);
}

std::string format_flasks(const FlaskInfo& flasks) {
    if (!flasks.valid) {
        return "Unknown";
    }
    return std::to_string(flasks.charges) + " flasks +" + std::to_string(flasks.tier);
}

std::string format_attributes(const CharacterStats& stats) {
    std::ostringstream oss;
    oss << "Vigor " << stats.vigor << " | Mind " << stats.mind << " | Endurance "
        << stats.endurance << " | Strength " << stats.strength << "\n"
        << "Dexterity " << stats.dexterity << " | Intelligence " << stats.intelligence
        << " | Faith " << stats.faith << " | Arcane " << stats.arcane;
    return oss.str();
}

std::string format_bosses_down(const std::vector<std::string>& names) {
    if (names.empty()) {
        return {};
    }
    std::string out;
    size_t shown = 0;
    for (const auto& name : names) {
        const std::string next = out.empty() ? name : out + ", " + name;
        if (next.size() > 900) {
            out += " +" + std::to_string(names.size() - shown) + " more";
            return out;
        }
        out = next;
        ++shown;
    }
    return out;
}

std::string grace_name_from_id(uint32_t grace_id) {
    if (grace_id == 0) {
        return {};
    }
    const auto begin = std::begin(k_graces);
    const auto end = std::end(k_graces);
    const auto it = std::lower_bound(
        begin, end, grace_id, [](const GraceEntry& entry, uint32_t id) { return entry.id < id; });
    if (it != end && it->id == grace_id) {
        return it->name;
    }
    if (grace_id > 100000) {
        return grace_name_from_id(grace_id % 100000);
    }
    return {};
}

std::string grace_name_from_bonfire_id(uint32_t bonfire_id) {
    if (bonfire_id == 0) {
        return {};
    }
    const auto begin = std::begin(k_bonfires);
    const auto end = std::end(k_bonfires);
    const auto it = std::lower_bound(
        begin, end, bonfire_id, [](const GraceEntry& entry, uint32_t id) { return entry.id < id; });
    if (it != end && it->id == bonfire_id) {
        return it->name;
    }
    return {};
}

std::vector<std::string> merge_bosses_down(
    const std::vector<std::string>& previous, const std::vector<std::string>& current) {
    // Event-flag reads can transiently fail (loading screens, tree updates),
    // so a fresh poll may miss bosses that were already reported. Within one
    // journey bosses stay dead, so keep the union in canonical order.
    const auto contains = [](const std::vector<std::string>& list, const std::string& name) {
        return std::find(list.begin(), list.end(), name) != list.end();
    };
    std::vector<std::string> merged;
    merged.reserve(previous.size() + current.size());
    for (const auto& boss : journey_bosses()) {
        if (contains(previous, boss.name) || contains(current, boss.name)) {
            merged.emplace_back(boss.name);
        }
    }
    for (const auto* list : {&previous, &current}) {
        for (const auto& name : *list) {
            if (!contains(merged, name)) {
                merged.push_back(name);
            }
        }
    }
    return merged;
}

const std::vector<JourneyBoss>& journey_bosses() {
    static const std::vector<JourneyBoss> kBosses = {
        {10000850, "Margit, the Fell Omen"},
        {10000800, "Godrick the Grafted"},
        {14000850, "Red Wolf of Radagon"},
        {14000800, "Rennala, Queen of the Full Moon"},
        {1252380800, "Starscourge Radahn"},
        {16000800, "Rykard, Lord of Blasphemy"},
        {11000800, "Morgott, the Omen King"},
        {1052520800, "Fire Giant"},
        {12050800, "Mohg, Lord of Blood"},
        {15000800, "Malenia, Blade of Miquella"},
        {12040800, "Astel, Naturalborn of the Void"},
        {12030850, "Lichdragon Fortissax"},
        {13000830, "Dragonlord Placidusax"},
        {13000800, "Maliketh, the Black Blade"},
        {11050850, "Sir Gideon Ofnir"},
        {11050800, "Hoarah Loux"},
        {19000800, "Elden Beast"},
        {20000800, "Divine Beast Dancing Lion"},
        {2048440800, "Rellana, Twin Moon Knight"},
        {22000800, "Putrescent Knight"},
        {21010800, "Messmer the Impaler"},
        {2049480800, "Commander Gaius"},
        {2050480800, "Scadutree Avatar"},
        {28000800, "Midra, Lord of Frenzied Flame"},
        {25000800, "Metyr, Mother of Fingers"},
        {2044450800, "Romina, Saint of the Bud"},
        {2054390800, "Bayle the Dread"},
        {20010800, "Radahn, Consort of Miquella"},
    };
    return kBosses;
}

bool event_flag_bit(uint32_t remainder, uint8_t packed_byte) {
    const uint32_t shift = 7 - (remainder & 7u);
    return (packed_byte & static_cast<uint8_t>(1u << shift)) != 0;
}

}  // namespace erstats
