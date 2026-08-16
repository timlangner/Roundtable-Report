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
