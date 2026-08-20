#include <catch2/catch_test_macros.hpp>

#include "game/gamedata.hpp"
#include "game/offsets.hpp"

#include <cstring>
#include <vector>

using namespace erstats;

TEST_CASE("offset table matches documented GameDataMan layout") {
    const auto& off = current_offsets();
    REQUIRE(off.player_game_data == 0x08);
    REQUIRE(off.deaths == 0x94);
    REQUIRE(off.igt_ms == 0xA0);
    REQUIRE(off.boss_fight == 0xC0);
    REQUIRE(off.ng_cycle == 0x120);
    REQUIRE(off.vigor == 0x3C);
    REQUIRE(off.mind == 0x40);
    REQUIRE(off.endurance == 0x44);
    REQUIRE(off.strength == 0x48);
    REQUIRE(off.dexterity == 0x4C);
    REQUIRE(off.intelligence == 0x50);
    REQUIRE(off.faith == 0x54);
    REQUIRE(off.arcane == 0x58);
    REQUIRE(off.level == 0x68);
    REQUIRE(off.runes == 0x6C);
    REQUIRE(off.rune_memory == 0x70);
    REQUIRE(off.inventory == 0x5D0);
    REQUIRE(off.last_grace == 0xB60);
    REQUIRE(off.name == 0x9C);
    REQUIRE(off.position == 0x6C0);
    REQUIRE(off.map_id == 0x6D0);
    REQUIRE(off.scadutree_blessing == 0xFC);
    REQUIRE(off.revered_ash == 0xFD);
    REQUIRE(off.flask_hp == 0x101);
    REQUIRE(off.flask_fp == 0x102);
    REQUIRE(off.equip_game_data == 0x2B0);
    REQUIRE(off.chr_asm == 0x70);
    REQUIRE(off.chr_asm_gaitem_handles == 0x24);
    REQUIRE(off.chr_asm_param_ids == 0x7C);
    REQUIRE(off.csfeman_boss_bars == 0x5BF0);
    REQUIRE(off.csfeman_hud_state == 0x78);
    REQUIRE(off.csfeman_menu_man == 0x18);
    REQUIRE(off.boss_bar_stride == 0x20);
    REQUIRE(off.boss_bar_display_id == 0);
    REQUIRE(off.chr_npc_param == 0x60);
    REQUIRE(off.chr_modules == 0x190);
    REQUIRE(off.stat_hp == 0x138);
}

TEST_CASE("read_from_buffers extracts a live snapshot") {
    const auto& off = current_offsets();
    std::vector<uint8_t> gdm(0xB0, 0);
    std::vector<uint8_t> pgd(0x110, 0);

    const uint32_t deaths = 17;
    const uint32_t igt = 3'600'000;
    const uint32_t level = 42;
    const uint32_t runes = 12345;
    const uint32_t memory = 99999;
    std::memcpy(gdm.data() + off.deaths, &deaths, 4);
    std::memcpy(gdm.data() + off.igt_ms, &igt, 4);
    const uint32_t vigor = 40;
    std::memcpy(pgd.data() + off.vigor, &vigor, 4);
    std::memcpy(pgd.data() + off.level, &level, 4);
    std::memcpy(pgd.data() + off.runes, &runes, 4);
    std::memcpy(pgd.data() + off.rune_memory, &memory, 4);
    const uint8_t scadu = 8;
    const uint8_t ash = 3;
    pgd[off.scadutree_blessing] = scadu;
    pgd[off.revered_ash] = ash;
    pgd[off.flask_hp] = 10;
    pgd[off.flask_fp] = 2;

    const wchar_t name[] = L"Tarnished";
    std::memcpy(pgd.data() + off.name, name, sizeof(name));

    const auto snap = GameDataReader::read_from_buffers(gdm, pgd);
    REQUIRE(snap.character_loaded);
    REQUIRE(snap.character_name == "Tarnished");
    REQUIRE(snap.deaths == 17);
    REQUIRE(snap.level == 42);
    REQUIRE(snap.runes == 12345);
    REQUIRE(snap.rune_memory == 99999);
    REQUIRE(snap.igt_ms == 3'600'000);
    REQUIRE(snap.stats.vigor == 40);
    REQUIRE(snap.scadutree_blessing == 8);
    REQUIRE(snap.revered_ash == 3);
    REQUIRE(snap.flasks.valid);
    REQUIRE(snap.flasks.charges == 12);
}

TEST_CASE("boss flag is only active when the raw value is 1") {
    const auto& off = current_offsets();
    std::vector<uint8_t> gdm(0xD0, 0);
    std::vector<uint8_t> pgd(0x110, 0);
    const uint32_t noise = 0x00000100;
    std::memcpy(gdm.data() + off.boss_fight, &noise, 4);
    const wchar_t name[] = L"Tarnished";
    std::memcpy(pgd.data() + off.name, name, sizeof(name));
    REQUIRE_FALSE(GameDataReader::read_from_buffers(gdm, pgd).in_boss_fight);

    const uint32_t active = 1;
    std::memcpy(gdm.data() + off.boss_fight, &active, 4);
    REQUIRE(GameDataReader::read_from_buffers(gdm, pgd).in_boss_fight);
}

TEST_CASE("session deaths never underflow") {
    REQUIRE(compute_session_deaths(10, 7) == 3);
    REQUIRE(compute_session_deaths(4, 10) == 0);
}

TEST_CASE("ChrAsm buffer yields active weapons and talismans") {
    const auto& off = current_offsets();
    const size_t chr_asm = off.equip_game_data + off.chr_asm;
    const size_t ids = chr_asm + off.chr_asm_param_ids;
    std::vector<uint8_t> pgd(ids + 22 * 4, 0);
    const uint32_t arm = 3;
    const uint32_t left_sel = 0;
    const uint32_t right_sel = 0;
    std::memcpy(pgd.data() + chr_asm + off.chr_asm_arm_style, &arm, 4);
    std::memcpy(pgd.data() + chr_asm + off.chr_asm_left_slot, &left_sel, 4);
    std::memcpy(pgd.data() + chr_asm + off.chr_asm_right_slot, &right_sel, 4);
    const int32_t claymore = 3180010;
    const int32_t empty = -1;
    const int32_t soreseal = 1051;
    std::memcpy(pgd.data() + ids + 1 * 4, &claymore, 4);
    std::memcpy(pgd.data() + ids + 0 * 4, &empty, 4);
    std::memcpy(pgd.data() + ids + 17 * 4, &soreseal, 4);
    const auto loadout = GameDataReader::loadout_from_player_bytes(pgd);
    REQUIRE(loadout.right_id == static_cast<uint32_t>(claymore));
    REQUIRE(loadout.left_id == 0xFFFFFFFFu);
    REQUIRE(loadout.two_handing);
    REQUIRE(loadout.attack_id == static_cast<uint32_t>(claymore));
    REQUIRE(loadout.talisman_ids[0] == 1051);
}

TEST_CASE("one-handed left weapon is the attacking armament when right is unarmed") {
    const auto& off = current_offsets();
    const size_t chr_asm = off.equip_game_data + off.chr_asm;
    const size_t ids = chr_asm + off.chr_asm_param_ids;
    std::vector<uint8_t> pgd(ids + 22 * 4, 0);
    const uint32_t arm = 1;
    const uint32_t left_sel = 0;
    const uint32_t right_sel = 0;
    std::memcpy(pgd.data() + chr_asm + off.chr_asm_arm_style, &arm, 4);
    std::memcpy(pgd.data() + chr_asm + off.chr_asm_left_slot, &left_sel, 4);
    std::memcpy(pgd.data() + chr_asm + off.chr_asm_right_slot, &right_sel, 4);
    const int32_t fang = 8030010;
    const int32_t unarmed = 110000;
    std::memcpy(pgd.data() + ids + 0 * 4, &fang, 4);
    std::memcpy(pgd.data() + ids + 1 * 4, &unarmed, 4);
    const auto loadout = GameDataReader::loadout_from_player_bytes(pgd);
    REQUIRE(loadout.left_id == static_cast<uint32_t>(fang));
    REQUIRE(loadout.right_id == static_cast<uint32_t>(unarmed));
    REQUIRE(loadout.attack_id == static_cast<uint32_t>(fang));
}

TEST_CASE("empty talisman param id falls back to accessory gaitem handle") {
    const auto& off = current_offsets();
    const size_t chr_asm = off.equip_game_data + off.chr_asm;
    const size_t ids = chr_asm + off.chr_asm_param_ids;
    const size_t gaitems = chr_asm + off.chr_asm_gaitem_handles;
    std::vector<uint8_t> pgd(ids + 22 * 4, 0);
    const int32_t empty = -1;
    const uint32_t claw_handle = 0xA0000884;  // Claw Talisman 2180
    std::memcpy(pgd.data() + ids + 17 * 4, &empty, 4);
    std::memcpy(pgd.data() + ids + 18 * 4, &empty, 4);
    std::memcpy(pgd.data() + ids + 19 * 4, &empty, 4);
    std::memcpy(pgd.data() + ids + 20 * 4, &empty, 4);
    std::memcpy(pgd.data() + gaitems + 20 * 4, &claw_handle, 4);
    const auto loadout = GameDataReader::loadout_from_player_bytes(pgd);
    REQUIRE(loadout.talisman_ids[0] == 2180);
}

TEST_CASE("empty accessory slot is filled from unused or covenant ChrAsm index") {
    const auto& off = current_offsets();
    const size_t chr_asm = off.equip_game_data + off.chr_asm;
    const size_t ids = chr_asm + off.chr_asm_param_ids;
    std::vector<uint8_t> pgd(ids + 22 * 4, 0);
    const int32_t empty = -1;
    const int32_t dragoncrest = 4000;
    const int32_t millicent = 1250;
    const int32_t two_hand = 8040;
    const int32_t exultation = 2160;
    for (int i = 0; i < 22; ++i) {
        std::memcpy(pgd.data() + ids + i * 4, &empty, 4);
    }
    std::memcpy(pgd.data() + ids + 17 * 4, &dragoncrest, 4);
    std::memcpy(pgd.data() + ids + 18 * 4, &millicent, 4);
    std::memcpy(pgd.data() + ids + 19 * 4, &two_hand, 4);
    std::memcpy(pgd.data() + ids + 16 * 4, &exultation, 4);
    const auto loadout = GameDataReader::loadout_from_player_bytes(pgd);
    REQUIRE(loadout.talisman_ids[0] == 4000);
    REQUIRE(loadout.talisman_ids[1] == 1250);
    REQUIRE(loadout.talisman_ids[2] == 8040);
    REQUIRE(loadout.talisman_ids[3] == 2160);
}

TEST_CASE("CSFeMan buffer yields occupied boss handles") {
    const auto& off = current_offsets();
    std::vector<uint8_t> fe(off.csfeman_boss_bars + 3 * off.boss_bar_stride, 0);
    const uint64_t handle = 0x1234;
    const uint64_t empty = 0xFFFFFFFFFFFFFFFFULL;
    std::memcpy(fe.data() + off.csfeman_boss_bars + off.boss_bar_handle, &handle, 8);
    std::memcpy(
        fe.data() + off.csfeman_boss_bars + off.boss_bar_stride + off.boss_bar_handle, &empty, 8);
    const auto handles = GameDataReader::boss_handles_from_csfeman_bytes(fe);
    REQUIRE(handles[0] == handle);
    REQUIRE(handles[1] == empty);
    REQUIRE_FALSE(boss_handle_occupied(handles[1]));
}

TEST_CASE("CSFeMan display id fills npc_param without ChrIns") {
    const auto& off = current_offsets();
    std::vector<uint8_t> fe(off.csfeman_boss_bars + off.boss_bar_stride, 0);
    const uint64_t handle = 0x1234;
    const int32_t display = 52200000;
    std::memcpy(fe.data() + off.csfeman_boss_bars + off.boss_bar_handle, &handle, 8);
    std::memcpy(fe.data() + off.csfeman_boss_bars + off.boss_bar_display_id, &display, 4);
    const auto slots = GameDataReader::boss_slots_from_csfeman_bytes(fe);
    REQUIRE(slots[0].handle == handle);
    REQUIRE(slots[0].npc_param == 52200000);
}

TEST_CASE("CSFeMan layout rejects the live false-positive bar bytes") {
    const auto& off = current_offsets();
    std::vector<uint8_t> fe(off.csfeman_boss_bars + 3 * off.boss_bar_stride, 0);
    fe[off.csfeman_hud_state] = 3;
    const int32_t empty_fmg = -1;
    const uint64_t empty_handle = 0xFFFFFFFFFFFFFFFFULL;
    for (int i = 0; i < off.boss_bar_count; ++i) {
        const size_t entry = off.csfeman_boss_bars + static_cast<size_t>(i) * off.boss_bar_stride;
        std::memcpy(fe.data() + entry + off.boss_bar_display_id, &empty_fmg, 4);
        std::memcpy(fe.data() + entry + off.boss_bar_handle, &empty_handle, 8);
    }
    REQUIRE(csfeman_layout_plausible(fe));

    const int32_t garbage_fmg = -1360610935;
    const uint64_t garbage_handle = 9431592613529296614ULL;
    std::memcpy(
        fe.data() + off.csfeman_boss_bars + off.boss_bar_display_id, &garbage_fmg, 4);
    std::memcpy(
        fe.data() + off.csfeman_boss_bars + off.boss_bar_handle, &garbage_handle, 8);
    REQUIRE_FALSE(csfeman_layout_plausible(fe));

    fe[off.csfeman_hud_state] = 4;
    std::memcpy(
        fe.data() + off.csfeman_boss_bars + off.boss_bar_display_id, &empty_fmg, 4);
    std::memcpy(
        fe.data() + off.csfeman_boss_bars + off.boss_bar_handle, &empty_handle, 8);
    REQUIRE_FALSE(csfeman_layout_plausible(fe));
}

TEST_CASE("unique CSFeMan AOB is kept even if the instance pointer is still null") {
    const CsFeManCandidate unique{.slot = 0x14670DE80, .unique_pattern = true};
    const CsFeManCandidate fake{
        .slot = 0x146704288,
        .unique_pattern = false,
        .instance = 0xDEADBEEF,
        .layout_ok = true,
    };
    const CsFeManCandidate candidates[] = {unique, fake};
    REQUIRE(*pick_csfeman_slot(candidates) == unique.slot);
}

TEST_CASE("fallback CSFeMan AOB requires a live plausible instance") {
    const CsFeManCandidate empty_instance{.slot = 0x2000, .instance = 0, .layout_ok = true};
    const CsFeManCandidate bad_layout{
        .slot = 0x3000, .instance = 0x1111, .layout_ok = false};
    const CsFeManCandidate ok{.slot = 0x4000, .instance = 0x2222, .layout_ok = true};
    const CsFeManCandidate first[] = {empty_instance, bad_layout};
    REQUIRE_FALSE(pick_csfeman_slot(first));
    const CsFeManCandidate second[] = {empty_instance, bad_layout, ok};
    REQUIRE(*pick_csfeman_slot(second) == ok.slot);
}
