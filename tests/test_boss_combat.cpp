#include <catch2/catch_test_macros.hpp>

#include "game/boss_combat.hpp"
#include "game/item_names.hpp"

#include <string>
#include <vector>

using namespace erstats;

namespace {

BossBarSlot bar(int32_t npc, int32_t hp, int32_t hp_max, uint64_t handle = 1) {
    return BossBarSlot{handle, npc, hp, hp_max};
}

EquippedLoadout claymore_loadout() {
    EquippedLoadout loadout;
    loadout.right_id = 3180000;
    loadout.attack_id = 3180000;
    return loadout;
}

}  // namespace

TEST_CASE("npc names strip place suffixes and map aliases") {
    REQUIRE(npc_display_name(21300014) == "Margit, the Fell Omen");
    REQUIRE(npc_display_name(1).empty());
    REQUIRE(format_encounter_name({21300014}) == "Margit, the Fell Omen");
    REQUIRE(format_encounter_name({35600000, 35700000}) == "Godskin Duo");
    REQUIRE(format_encounter_name({47700165, 47701165}) == "Valiant Gargoyles");
    REQUIRE(format_encounter_name({25000000, 34600000})
            == "Crucible Knight, Leonine Misbegotten");
    REQUIRE(format_encounter_name({1, 21300014}) == "Margit, the Fell Omen");
    REQUIRE(format_encounter_name({1, 2}).empty());
}

TEST_CASE("npc names map nearby param variants and prefer HUD display id") {
    REQUIRE(npc_display_name(52200050) == "Promised Consort Radahn");
    REQUIRE(npc_display_name(21300014) == "Margit, the Fell Omen");
    REQUIRE(prefer_mapped_npc_param(52200000, 0) == 52200000);
    REQUIRE(prefer_mapped_npc_param(-1, 21300014) == 21300014);
    REQUIRE(prefer_mapped_npc_param(52200050, 0) == 52200050);
    REQUIRE(prefer_mapped_npc_param(0, 0) == 0);
}

TEST_CASE("unmapped chr param still latches when HUD display id maps") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 3;
    input.bars = {bar(prefer_mapped_npc_param(52200050, 0), 40, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 1000);
    input.bars.clear();
    input.deaths = 4;
    tick_boss_combat(tracker, input, 2000);
    REQUIRE(tracker.last_boss == "Promised Consort Radahn");
    REQUIRE(tracker.best_by_encounter["Promised Consort Radahn"].right_weapon
            == "Claymore +0");
}

TEST_CASE("weapons and talismans render empty and upgrade suffixes") {
    REQUIRE(format_weapon_name(0).empty());
    REQUIRE(format_weapon_name(110000).empty());
    REQUIRE(format_weapon_name(3180000) == "Claymore +0");
    REQUIRE(format_weapon_name(3180010) == "Claymore +10");
    REQUIRE(format_weapon_name(8030010) == "Bloodhound's Fang +10");
    REQUIRE(format_talisman_name(1051) == "Radagon's Soreseal");
    REQUIRE(format_talisman_name(0x20000FA0) == "Dragoncrest Shield Talisman");
    REQUIRE(format_talisman_name(0xA0000FA0) == "Dragoncrest Shield Talisman");
    REQUIRE(format_talisman_name(0).empty());
    REQUIRE(format_talisman_name(999999) == "Unknown");
    REQUIRE(format_weapons_line("", "") == "No weapons");
    REQUIRE(format_weapons_line("Claymore +0", "") == "Claymore +0 / No weapon");
    REQUIRE(format_weapons_line("", "Jellyfish Shield +0") == "No weapon / Jellyfish Shield +0");
    REQUIRE(format_talismans_line({}) == "No talismans");
    REQUIRE(format_talismans_line({"Radagon's Soreseal", "Erdtree's Favor"})
            == "Radagon's Soreseal, Erdtree's Favor");
}

TEST_CASE("boss bar handles use the FieldIns selector, not the raw qword") {
    REQUIRE_FALSE(boss_handle_occupied(0));
    REQUIRE_FALSE(boss_handle_occupied(0xFFFFFFFFFFFFFFFFULL));
    REQUIRE_FALSE(boss_handle_occupied(0x82E3BF82FFFFFFFFull));
    REQUIRE_FALSE(boss_handle_occupied(9431592613529296614ULL));
    REQUIRE(boss_handle_occupied(0x1234));
    REQUIRE(boss_handle_occupied(0x0000A01E10000005ULL));
}

TEST_CASE("live CSFeMan false-positive bytes are not plausible boss slots") {
    REQUIRE_FALSE(boss_bar_ids_plausible(-1360610935, 9431592613529296614ULL));
    REQUIRE(boss_bar_ids_plausible(-1, 0xFFFFFFFFFFFFFFFFULL));
    REQUIRE(boss_bar_ids_plausible(52200000, 0x0000A01E10000005ULL));
}

TEST_CASE("combined remaining HP never rounds a sliver to 0%") {
    REQUIRE_FALSE(combined_hp_pct({}).has_value());
    REQUIRE(*combined_hp_pct({bar(21300014, 20, 100)}) == 20);
    REQUIRE(*combined_hp_pct({bar(21300014, 0, 100)}) == 0);
    REQUIRE(*combined_hp_pct({bar(21300014, 1, 1000)}) == 1);
    REQUIRE(*combined_hp_pct({bar(35600000, 50, 100), bar(35700000, 50, 100)}) == 50);
}

TEST_CASE("latch commits last boss only on a death inside the grace window") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 3;
    input.bars = {bar(21300014, 20, 100)};
    input.loadout = claymore_loadout();

    tick_boss_combat(tracker, input, 1000);
    REQUIRE(tracker.last_boss.empty());

    input.bars.clear();
    input.deaths = 4;
    tick_boss_combat(tracker, input, 2000);
    REQUIRE(tracker.last_boss == "Margit, the Fell Omen");
    REQUIRE(tracker.last_killed.empty());
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].hp_pct == 20);
}

TEST_CASE("death after a long DLC reload still writes last boss") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1870;
    input.bars = {bar(52200089, 39904, 46134)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);

    input.bars.clear();
    tick_boss_combat(tracker, input, 250);
    input.deaths = 1871;
    tick_boss_combat(tracker, input, 45'000);
    REQUIRE(tracker.last_boss == "Promised Consort Radahn");
}

TEST_CASE("death after the grace window does not write last boss") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1;
    input.bars = {bar(21300014, 40, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);

    input.bars.clear();
    tick_boss_combat(tracker, input, 100);
    input.deaths = 2;
    tick_boss_combat(tracker, input, 100 + kBossLatchGraceMs + 1);
    REQUIRE(tracker.last_boss.empty());
}

TEST_CASE("walking away without a death does not write last boss") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 2;
    input.bars = {bar(21300014, 80, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    tick_boss_combat(tracker, input, 250);
    REQUIRE(tracker.last_boss.empty());
    REQUIRE(tracker.last_killed.empty());
}

TEST_CASE("best try keeps the lowest remaining HP and that death's gear") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1;
    input.bars = {bar(21300014, 20, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.deaths = 2;
    tick_boss_combat(tracker, input, 100);

    input.bars = {bar(21300014, 40, 100)};
    input.loadout.right_id = 8030010;
    input.loadout.attack_id = 8030010;
    tick_boss_combat(tracker, input, 200);
    input.bars.clear();
    input.deaths = 3;
    tick_boss_combat(tracker, input, 300);

    LiveSnapshot live;
    apply_best_to_snapshot(live, tracker);
    REQUIRE(live.last_boss == "Margit, the Fell Omen");
    REQUIRE(live.last_boss_hp_pct == 20);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].right_weapon == "Claymore +0");

    input.bars = {bar(21300014, 15, 100)};
    input.loadout.right_id = 8030010;
    input.loadout.attack_id = 8030010;
    tick_boss_combat(tracker, input, 400);
    input.bars.clear();
    input.deaths = 4;
    tick_boss_combat(tracker, input, 500);
    apply_best_to_snapshot(live, tracker);
    REQUIRE(live.last_boss_hp_pct == 15);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].right_weapon
            == "Bloodhound's Fang +10");
}

TEST_CASE("a later death to another boss keeps the previous encounter's best") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 0;
    input.bars = {bar(21300014, 20, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.deaths = 1;
    tick_boss_combat(tracker, input, 100);

    input.bars = {bar(25000000, 40, 100)};
    tick_boss_combat(tracker, input, 200);
    input.bars.clear();
    input.deaths = 2;
    tick_boss_combat(tracker, input, 300);

    LiveSnapshot live;
    apply_best_to_snapshot(live, tracker);
    REQUIRE(live.last_boss == "Crucible Knight");
    REQUIRE(live.last_boss_hp_pct == 40);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].hp_pct == 20);
}

TEST_CASE("HUD kill sets last killed and never last boss") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 5;
    input.bars = {bar(21300014, 0, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    tick_boss_combat(tracker, input, 250);
    REQUIRE(tracker.last_killed == "Margit, the Fell Omen");
    REQUIRE(tracker.last_boss.empty());
}

TEST_CASE("phase change is not a kill") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1;
    input.bars = {bar(21300014, 0, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars = {bar(25000000, 100, 100)};
    tick_boss_combat(tracker, input, 250);
    REQUIRE(tracker.last_killed.empty());
    input.bars.clear();
    tick_boss_combat(tracker, input, 500);
    REQUIRE(tracker.last_killed.empty());
}

TEST_CASE("flag diff sets last killed when HUD missed the zero frame") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 2;
    tick_boss_combat(tracker, input, 0);
    input.bosses_down = {"Margit, the Fell Omen"};
    tick_boss_combat(tracker, input, 250);
    REQUIRE(tracker.last_killed == "Margit, the Fell Omen");
    REQUIRE(tracker.last_boss.empty());
}

TEST_CASE("HUD encounter name wins when flags also fire") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1;
    input.bars = {bar(35600000, 0, 100), bar(35700000, 0, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.bosses_down = {"Godskin Apostle", "Godskin Noble"};
    tick_boss_combat(tracker, input, 250);
    REQUIRE(tracker.last_killed == "Godskin Duo");
}

TEST_CASE("credited HP drop picks the dealt-damage weapon; ties prefer right hand") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1;
    input.loadout.right_id = 3180010;
    input.loadout.left_id = 8030010;
    input.loadout.attack_id = 3180010;
    input.bars = {bar(21300014, 100, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars = {bar(21300014, 70, 100)};
    tick_boss_combat(tracker, input, 250);
    input.loadout.attack_id = 8030010;
    input.bars = {bar(21300014, 50, 100)};
    tick_boss_combat(tracker, input, 500);
    input.bars.clear();
    input.deaths = 2;
    tick_boss_combat(tracker, input, 750);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].dealt_damage == "Claymore +10");
}

TEST_CASE("HP drop credits left-hand weapon when right hand is unarmed") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 1;
    input.loadout.right_id = 110000;
    input.loadout.left_id = 8030010;
    input.loadout.attack_id = 110000;
    input.bars = {bar(21300014, 100, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars = {bar(21300014, 70, 100)};
    tick_boss_combat(tracker, input, 250);
    input.bars.clear();
    input.deaths = 2;
    tick_boss_combat(tracker, input, 500);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].dealt_damage
            == "Bloodhound's Fang +10");
}

TEST_CASE("no HP drop falls back to left-hand weapon when right hand is empty") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 0;
    input.loadout.right_id = 110000;
    input.loadout.left_id = 8030010;
    input.loadout.attack_id = 110000;
    input.bars = {bar(21300014, 100, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.deaths = 1;
    tick_boss_combat(tracker, input, 100);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].dealt_damage
            == "Bloodhound's Fang +10");
}

TEST_CASE("unmapped talisman id is kept as Unknown on the death record") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 0;
    input.loadout = claymore_loadout();
    input.loadout.talisman_ids = {1051, 999999, 0, 4000};
    input.bars = {bar(21300014, 100, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.deaths = 1;
    tick_boss_combat(tracker, input, 100);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].talismans
            == std::vector<std::string>{
                "Radagon's Soreseal", "Unknown", "Dragoncrest Shield Talisman"});
}

TEST_CASE("no HP drop uses two-hand then right-hand fallback") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 0;
    input.loadout.right_id = 3180000;
    input.loadout.attack_id = 3180000;
    input.loadout.two_handing = true;
    input.bars = {bar(21300014, 100, 100)};
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.deaths = 1;
    tick_boss_combat(tracker, input, 100);
    REQUIRE(tracker.best_by_encounter["Margit, the Fell Omen"].dealt_damage == "Claymore +0");
}

TEST_CASE("first poll with an existing death count does not commit last boss") {
    BossCombatTracker tracker;
    BossCombatInput input;
    input.deaths = 40;
    input.bars = {bar(21300014, 10, 100)};
    input.loadout = claymore_loadout();
    tick_boss_combat(tracker, input, 0);
    REQUIRE(tracker.last_boss.empty());
}

TEST_CASE("restore tracker keeps last boss and last killed across a bind") {
    LiveSnapshot stored;
    stored.last_boss = "Margit, the Fell Omen";
    stored.last_killed = "Soldier of Godrick";
    stored.deaths = 12;
    stored.boss_death_best["Margit, the Fell Omen"] = BossDeathRecord{
        "Margit, the Fell Omen", 20, "Claymore +0", "", "Claymore +0", {}};
    BossCombatTracker tracker;
    restore_tracker_from_snapshot(tracker, stored);
    LiveSnapshot live;
    apply_best_to_snapshot(live, tracker);
    REQUIRE(live.last_boss == "Margit, the Fell Omen");
    REQUIRE(live.last_killed == "Soldier of Godrick");
    REQUIRE_FALSE(live.last_boss_hp_pct.has_value());
}

TEST_CASE("empty loadout keeps previously applied talismans") {
    LiveSnapshot live;
    live.last_boss = "Promised Consort Radahn";
    EquippedLoadout loadout;
    loadout.left_id = 8030010;
    loadout.talisman_ids = {4000, 1250, 8040, 2160};
    apply_current_loadout_to_snapshot(live, loadout);
    REQUIRE(live.last_boss_talismans.size() == 4);

    BossCombatTracker tracker;
    tracker.last_boss = "Promised Consort Radahn";
    tracker.best_by_encounter["Promised Consort Radahn"] = BossDeathRecord{
        "Promised Consort Radahn",
        73,
        "",
        "Bloodhound's Fang +10",
        "No weapons",
        {"Dragoncrest Shield Talisman", "Millicent's Prosthesis", "Two-Handed Sword Talisman"}};
    apply_best_to_snapshot(live, tracker);
    apply_current_loadout_to_snapshot(live, EquippedLoadout{});
    REQUIRE(live.last_boss_talismans.size() == 4);
    REQUIRE(live.last_boss_talismans.back() == "Lord of Blood's Exultation");
}

TEST_CASE("best try on the snapshot is this session only") {
    LiveSnapshot stored;
    stored.last_boss = "Promised Consort Radahn";
    stored.boss_death_best["Promised Consort Radahn"] = BossDeathRecord{
        "Promised Consort Radahn", 73, "", "Bloodhound's Fang +10", "No weapons", {}};
    BossCombatTracker tracker;
    restore_tracker_from_snapshot(tracker, stored);

    BossCombatInput input;
    input.deaths = 1880;
    input.bars = {bar(52200089, 40944, 46134)};
    input.loadout.left_id = 8030010;
    tick_boss_combat(tracker, input, 0);
    input.bars.clear();
    input.deaths = 1881;
    tick_boss_combat(tracker, input, 250);

    LiveSnapshot live;
    apply_best_to_snapshot(live, tracker);
    REQUIRE(live.last_boss == "Promised Consort Radahn");
    REQUIRE(live.last_boss_hp_pct == 89);
    REQUIRE(tracker.best_by_encounter["Promised Consort Radahn"].hp_pct == 73);

    clear_session_boss_best(tracker);
    apply_best_to_snapshot(live, tracker);
    REQUIRE_FALSE(live.last_boss_hp_pct.has_value());
}

TEST_CASE("current loadout overwrites last-boss weapons and talismans") {
    LiveSnapshot stored;
    stored.last_boss = "Margit, the Fell Omen";
    stored.boss_death_best["Margit, the Fell Omen"] = BossDeathRecord{
        "Margit, the Fell Omen",
        20,
        "Claymore +0",
        "",
        "Claymore +0",
        {"Radagon's Soreseal"}};
    BossCombatTracker tracker;
    restore_tracker_from_snapshot(tracker, stored);
    LiveSnapshot live;
    apply_best_to_snapshot(live, tracker);
    EquippedLoadout loadout;
    loadout.right_id = 110000;
    loadout.left_id = 8030010;
    loadout.talisman_ids = {4003, 2081, 1250, 2160};
    apply_current_loadout_to_snapshot(live, loadout);
    REQUIRE_FALSE(live.last_boss_hp_pct.has_value());
    REQUIRE(live.last_boss_right_weapon.empty());
    REQUIRE(live.last_boss_left_weapon == "Bloodhound's Fang +10");
    REQUIRE(live.last_boss_talismans
            == std::vector<std::string>{
                "Dragoncrest Greatshield Talisman",
                "Rotten Winged Sword Insignia",
                "Millicent's Prosthesis",
                "Lord of Blood's Exultation"});
}
