#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace erstats {

std::string npc_display_name(int32_t npc_param);
std::string format_encounter_name(const std::vector<int32_t>& npc_params);
int32_t prefer_mapped_npc_param(int32_t display_id, int32_t chr_npc_param);

// Empty / unarmed ids yield an empty string.
std::string format_weapon_name(uint32_t item_id);
std::string format_talisman_name(uint32_t item_id);

std::string format_weapons_line(std::string_view right, std::string_view left);
std::string format_talismans_line(const std::vector<std::string>& names);

uint32_t weapon_param_id(uint32_t item_id);
uint32_t talisman_param_id(uint32_t item_id);
uint8_t weapon_upgrade_level(uint32_t item_id);
bool item_slot_empty(uint32_t item_id);

}  // namespace erstats
