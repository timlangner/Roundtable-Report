#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace erstats {

// Single offset table so a game patch update is one-file change.
// Values match Yosna / LiveSplit ASL / eldenring-practice-tool (GameDataMan + PlayerGameData).
struct Offsets {
    std::size_t player_game_data = 0x08;
    std::size_t deaths = 0x94;
    std::size_t igt_ms = 0xA0;
    std::size_t ng_cycle = 0x120;
    std::size_t boss_fight = 0xC0;
    std::size_t name = 0x9C;
    std::size_t name_bytes = 0x20;
    std::size_t vigor = 0x3C;
    std::size_t mind = 0x40;
    std::size_t endurance = 0x44;
    std::size_t strength = 0x48;
    std::size_t dexterity = 0x4C;
    std::size_t intelligence = 0x50;
    std::size_t faith = 0x54;
    std::size_t arcane = 0x58;
    std::size_t level = 0x68;
    std::size_t runes = 0x6C;
    std::size_t rune_memory = 0x70;
    std::size_t scadutree_blessing = 0xFC;
    std::size_t revered_ash = 0xFD;
    std::size_t flask_hp = 0x101;
    std::size_t flask_fp = 0x102;
    std::size_t inventory = 0x5D0;
    std::size_t inventory_list = 0x10;
    std::size_t inventory_count = 0x18;
    std::size_t inventory_stride = 0x18;
    std::size_t item_id = 0x04;
    std::size_t item_qty = 0x08;
    std::size_t last_grace = 0xB60;
    std::size_t last_grace_alt = 0xB6C;
    std::size_t player_ins = 0x1E508;
    std::size_t player_ins_legacy = 0x18468;
    std::size_t player_ins_net = 0x10EF8;
    std::size_t position = 0x6C0;
    std::size_t map_id = 0x6D0;
};

struct AobPattern {
    std::string_view name;
    std::string_view ida;
    int rel32_offset = 3;
    int instruction_size = 7;
};

const Offsets& current_offsets();

// Grand Archives / LiveSplit GameDataMan accessor:
// 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3
const std::vector<AobPattern>& gamedataman_patterns();
const std::vector<AobPattern>& worldchrman_patterns();
const std::vector<AobPattern>& gameman_patterns();
const std::vector<AobPattern>& eventflag_patterns();

}  // namespace erstats
