#include "game/item_names.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string_view>
#include <utility>

namespace erstats {
namespace {

struct NameEntry {
    int32_t id = 0;
    const char* name = nullptr;
};

const NameEntry kNpcs[] =
#include "game/npc_name_table.inc"
    ;

const NameEntry kWeapons[] =
#include "game/weapon_name_table.inc"
    ;

const NameEntry kTalismans[] =
#include "game/talisman_name_table.inc"
    ;

const char* lookup(const NameEntry* begin, const NameEntry* end, int32_t id) {
    const auto it = std::lower_bound(
        begin, end, id, [](const NameEntry& entry, int32_t value) { return entry.id < value; });
    if (it == end || it->id != id || it->name == nullptr) {
        return nullptr;
    }
    return it->name;
}

const char* lookup_npc(int32_t id) {
    if (id <= 0) {
        return nullptr;
    }
    if (const char* exact = lookup(std::begin(kNpcs), std::end(kNpcs), id)) {
        return exact;
    }
    const auto begin = std::begin(kNpcs);
    const auto end = std::end(kNpcs);
    auto it = std::lower_bound(
        begin, end, id, [](const NameEntry& entry, int32_t value) { return entry.id < value; });
    if (it == begin) {
        return nullptr;
    }
    --it;
    if (it->name == nullptr || it->id / 1000 != id / 1000) {
        return nullptr;
    }
    return it->name;
}

std::string clean_npc_name(std::string name) {
    auto strip_wrap = [](std::string& s, char open, char close) {
        while (!s.empty()) {
            const auto close_at = s.find_last_of(close);
            if (close_at == std::string::npos || close_at + 1 != s.size()) {
                break;
            }
            const auto open_at = s.find_last_of(open);
            if (open_at == std::string::npos || open_at == 0) {
                break;
            }
            if (s[open_at - 1] != ' ' && s[open_at - 1] != '{') {
                // still strip " Name (Place)"
            }
            s.erase(open_at);
            while (!s.empty() && s.back() == ' ') {
                s.pop_back();
            }
        }
    };
    strip_wrap(name, '(', ')');
    strip_wrap(name, '{', ')');
    strip_wrap(name, '{', '}');
    return name;
}

}  // namespace

bool item_slot_empty(uint32_t item_id) {
    return item_id == 0 || item_id == 0xFFFFFFFFu || item_id == 110000u;
}

uint32_t weapon_param_id(uint32_t item_id) {
    const uint32_t raw = item_id & 0x0FFFFFFFu;
    const uint32_t upgrade = raw % 100u;
    if (upgrade <= 25u) {
        return raw - upgrade;
    }
    return raw;
}

uint32_t talisman_param_id(uint32_t item_id) {
    if (item_slot_empty(item_id)) {
        return 0;
    }
    const uint32_t category = item_id >> 28;
    if (category == 2) {
        return item_id & 0x0FFFFFFFu;
    }
    if (category == 0xAu) {
        return item_id & 0x00FFFFFFu;
    }
    return item_id & 0x00FFFFFFu;
}

uint8_t weapon_upgrade_level(uint32_t item_id) {
    if (item_slot_empty(item_id)) {
        return 0;
    }
    const uint32_t raw = item_id & 0x0FFFFFFFu;
    const uint32_t upgrade = raw % 100u;
    return upgrade <= 25u ? static_cast<uint8_t>(upgrade) : 0;
}

std::string npc_display_name(int32_t npc_param) {
    if (npc_param <= 0) {
        return {};
    }
    const char* found = lookup_npc(npc_param);
    if (found == nullptr) {
        return {};
    }
    return clean_npc_name(found);
}

int32_t prefer_mapped_npc_param(int32_t display_id, int32_t chr_npc_param) {
    if (!npc_display_name(display_id).empty()) {
        return display_id;
    }
    if (!npc_display_name(chr_npc_param).empty()) {
        return chr_npc_param;
    }
    return display_id > 0 ? display_id : chr_npc_param;
}

std::string format_encounter_name(const std::vector<int32_t>& npc_params) {
    std::vector<std::string> names;
    names.reserve(npc_params.size());
    for (const int32_t id : npc_params) {
        auto name = npc_display_name(id);
        if (name.empty()) {
            continue;
        }
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(std::move(name));
        }
    }
    if (names.empty()) {
        return {};
    }
    std::set<std::string> unique(names.begin(), names.end());
    if (unique.count("Godskin Apostle") && unique.count("Godskin Noble")) {
        return "Godskin Duo";
    }
    if (unique.size() == 1 && names[0] == "Valiant Gargoyle" && npc_params.size() >= 2) {
        return "Valiant Gargoyles";
    }
    std::string out = names[0];
    for (size_t i = 1; i < names.size(); ++i) {
        out += ", ";
        out += names[i];
    }
    return out;
}

std::string format_weapon_name(uint32_t item_id) {
    if (item_slot_empty(item_id)) {
        return {};
    }
    const uint32_t param = weapon_param_id(item_id);
    const uint8_t level = weapon_upgrade_level(item_id);
    const char* found = lookup(std::begin(kWeapons), std::end(kWeapons), static_cast<int32_t>(param));
    std::string name = found != nullptr ? found : "Unknown";
    name += " +";
    name += std::to_string(level);
    return name;
}

std::string format_talisman_name(uint32_t item_id) {
    const uint32_t raw = talisman_param_id(item_id);
    if (raw == 0) {
        return {};
    }
    const char* found = lookup(std::begin(kTalismans), std::end(kTalismans), static_cast<int32_t>(raw));
    return found != nullptr ? found : std::string("Unknown");
}

std::string format_weapons_line(std::string_view right, std::string_view left) {
    if (right.empty() && left.empty()) {
        return "No weapons";
    }
    const std::string r = right.empty() ? "No weapon" : std::string(right);
    const std::string l = left.empty() ? "No weapon" : std::string(left);
    return r + " / " + l;
}

std::string format_talismans_line(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& name : names) {
        if (name.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += ", ";
        }
        out += name;
    }
    return out.empty() ? "No talismans" : out;
}

}  // namespace erstats
