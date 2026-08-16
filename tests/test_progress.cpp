#include <catch2/catch_test_macros.hpp>

#include "game/progress.hpp"

using namespace erstats;

TEST_CASE("flask item ids decode charges and sacred tear tier") {
    const auto flasks = flasks_from_inventory({
        {1001 + 8 * 2, 10},
        {1051 + 8 * 2, 2},
    });
    REQUIRE(flasks.valid);
    REQUIRE(flasks.charges == 12);
    REQUIRE(flasks.tier == 8);
    REQUIRE(format_flasks(flasks) == "12 flasks +8");
}

TEST_CASE("empty and filled flasks add together") {
    const auto flasks = flasks_from_inventory({
        {1000, 1},
        {1001, 3},
        {1050, 0},
        {1051, 0},
    });
    REQUIRE(flasks.charges == 4);
    REQUIRE(flasks.tier == 0);
    REQUIRE(format_flasks(flasks) == "4 flasks +0");
}

TEST_CASE("goods ids ignore category bits in the high byte") {
    REQUIRE(goods_item_id(0x400003E9) == 1001);
    const auto flasks = flasks_from_inventory({{0x400003E9 + 8 * 2, 6}});
    REQUIRE(flasks.charges == 6);
    REQUIRE(flasks.tier == 8);
}

TEST_CASE("unknown inventory does not invent flasks") {
    REQUIRE_FALSE(flasks_from_inventory({{9000, 4}}).valid);
    REQUIRE(format_flasks({}) == "Unknown");
}

TEST_CASE("flask allocation is the rested total not remaining uses") {
    const auto flasks = flasks_from_allocation(10, 2, 8);
    REQUIRE(flasks.valid);
    REQUIRE(flasks.charges == 12);
    REQUIRE(flasks.tier == 8);
    REQUIRE(format_flasks(flasks) == "12 flasks +8");
}

TEST_CASE("flask allocation rejects empty or oversized values") {
    REQUIRE_FALSE(flasks_from_allocation(0, 0, 0).valid);
    REQUIRE_FALSE(flasks_from_allocation(15, 0, 0).valid);
    REQUIRE_FALSE(flasks_from_allocation(8, 8, 0).valid);
}

TEST_CASE("allocated flasks win over remaining inventory uses") {
    const auto remaining = flasks_from_inventory({{1001 + 8 * 2, 3}, {1051 + 8 * 2, 1}});
    const auto allocated = flasks_from_allocation(10, 2, 0);
    const auto flasks = prefer_allocated_flasks(allocated, remaining);
    REQUIRE(flasks.charges == 12);
    REQUIRE(flasks.tier == 8);
}

TEST_CASE("attributes render as a compact character sheet") {
    CharacterStats stats;
    stats.vigor = 40;
    stats.mind = 18;
    stats.endurance = 25;
    stats.strength = 30;
    stats.dexterity = 20;
    stats.intelligence = 9;
    stats.faith = 8;
    stats.arcane = 10;
    REQUIRE(stats_look_valid(stats));
    const auto text = format_attributes(stats);
    REQUIRE(text.find("Vigor 40") != std::string::npos);
    REQUIRE(text.find("Mind 18") != std::string::npos);
    REQUIRE(text.find("Endurance 25") != std::string::npos);
    REQUIRE(text.find("Strength 30") != std::string::npos);
    REQUIRE(text.find("Dexterity 20") != std::string::npos);
    REQUIRE(text.find("Intelligence 9") != std::string::npos);
    REQUIRE(text.find("Faith 8") != std::string::npos);
    REQUIRE(text.find("Arcane 10") != std::string::npos);
    REQUIRE(text.find("\xE2\x80\x94") == std::string::npos);
}

TEST_CASE("zeroed stats are not shown as a real build") {
    REQUIRE_FALSE(stats_look_valid({}));
    REQUIRE_FALSE(plausible_stat(0));
    REQUIRE_FALSE(plausible_stat(100));
}

TEST_CASE("grace ids resolve to site names") {
    REQUIRE(grace_name_from_id(76101) == "The First Step");
    REQUIRE(grace_name_from_id(76100) == "Church of Elleh");
    REQUIRE(grace_name_from_id(71400) == "Raya Lucaria Grand Library");
    REQUIRE(grace_name_from_id(0).empty());
    REQUIRE(grace_name_from_id(1).empty());
}

TEST_CASE("boss list merge keeps bosses missed by a flaky poll") {
    const std::vector<std::string> previous = {
        "Godrick the Grafted", "Messmer the Impaler", "Bayle the Dread"};
    const std::vector<std::string> current = {"Margit, the Fell Omen", "Godrick the Grafted"};
    const auto merged = merge_bosses_down(previous, current);
    // Canonical journey order, DLC bosses retained even though the current
    // poll failed to read their flags.
    const std::vector<std::string> expected = {
        "Margit, the Fell Omen", "Godrick the Grafted", "Messmer the Impaler", "Bayle the Dread"};
    REQUIRE(merged == expected);
}

TEST_CASE("boss list merge handles empty sides") {
    const std::vector<std::string> bosses = {"Godrick the Grafted"};
    REQUIRE(merge_bosses_down({}, bosses) == bosses);
    REQUIRE(merge_bosses_down(bosses, {}) == bosses);
    REQUIRE(merge_bosses_down({}, {}).empty());
}

TEST_CASE("bonfire warp ids resolve to grace names") {
    REQUIRE(grace_name_from_bonfire_id(1042362950) == "Church of Elleh");
    REQUIRE(grace_name_from_bonfire_id(2047472950) == "Viaduct Minor Tower");
    REQUIRE(grace_name_from_bonfire_id(10002950) == "Godrick the Grafted");
    REQUIRE(grace_name_from_bonfire_id(0).empty());
    REQUIRE(grace_name_from_bonfire_id(123).empty());
}

TEST_CASE("journey bosses include remembrance and DLC legends") {
    const auto& bosses = journey_bosses();
    REQUIRE(bosses.size() >= 20);
    bool godrick = false;
    bool messmer = false;
    for (const auto& boss : bosses) {
        if (boss.flag == 10000800 && std::string(boss.name) == "Godrick the Grafted") {
            godrick = true;
        }
        if (boss.flag == 21010800 && std::string(boss.name).find("Messmer") != std::string::npos) {
            messmer = true;
        }
    }
    REQUIRE(godrick);
    REQUIRE(messmer);
}

TEST_CASE("boss list stays readable in a Discord field") {
    REQUIRE(format_bosses_down({}).empty());
    REQUIRE(format_bosses_down({"Godrick the Grafted"}) == "Godrick the Grafted");
    const auto many = format_bosses_down({
        "Godrick the Grafted",
        "Rennala, Queen of the Full Moon",
        "Starscourge Radahn",
        "Rykard, Lord of Blasphemy",
    });
    REQUIRE(many.find("Godrick the Grafted") != std::string::npos);
    REQUIRE(many.find("Starscourge Radahn") != std::string::npos);
    REQUIRE(many.find("\xE2\x80\x94") == std::string::npos);
}

TEST_CASE("last boss hint uses the current region when the fight is unique") {
    REQUIRE(last_boss_from_location("Academy of Raya Lucaria") == "Rennala, Queen of the Full Moon");
    REQUIRE(last_boss_from_location("Stormveil Castle") == "Stormveil Castle");
    REQUIRE(last_boss_from_location("Unknown").empty());
}

TEST_CASE("event flag bit matches FromSoftware packing") {
    REQUIRE(event_flag_bit(0, 0x80));
    REQUIRE_FALSE(event_flag_bit(0, 0x01));
    REQUIRE(event_flag_bit(7, 0x01));
}
