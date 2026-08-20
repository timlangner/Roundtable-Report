#include <catch2/catch_test_macros.hpp>

#include "run/store.hpp"

#include <filesystem>

using namespace erstats;

TEST_CASE("snapshot json roundtrip keeps run identity") {
    RunSnapshot snap;
    snap.identity.steam_id = "99";
    snap.identity.save_filename = "ER0000.sl2";
    snap.identity.slot_index = 4;
    snap.identity.character_name = "Ranni";
    snap.live.deaths = 8;
    snap.live.session_deaths = 1;
    snap.live.level = 20;
    snap.live.x = 128.f;
    snap.live.z = 64.f;
    snap.live.has_position = true;
    snap.live.scadutree_blessing = 5;
    snap.live.revered_ash = 2;
    snap.live.session_ms = 120000;
    snap.live.stats.vigor = 20;
    snap.live.flasks = {8, 4, true};
    snap.live.last_grace = "The First Step";
    snap.live.last_boss = "Soldier of Godrick";
    snap.live.last_killed = "Soldier of Godrick";
    snap.live.last_boss_hp_pct = 20;
    snap.live.last_boss_right_weapon = "Claymore +0";
    snap.live.last_boss_dealt_damage = "Claymore +0";
    snap.live.last_boss_talismans = {"Radagon's Soreseal"};
    snap.live.boss_death_best["Soldier of Godrick"] = BossDeathRecord{
        "Soldier of Godrick", 20, "Claymore +0", "", "Claymore +0", {"Radagon's Soreseal"}};
    snap.live.bosses_down = {"Soldier of Godrick"};
    snap.discord_message_id = "msg-1";
    snap.updated_at = "2026-08-16T00:00:00Z";

    const auto parsed = snapshot_from_json(snapshot_to_json(snap));
    REQUIRE(parsed);
    REQUIRE(parsed->identity.key() == "99/ER0000.sl2/slot4/Ranni");
    REQUIRE(parsed->live.deaths == 8);
    REQUIRE(parsed->live.has_position);
    REQUIRE(parsed->live.x == 128.f);
    REQUIRE(parsed->live.scadutree_blessing == 5);
    REQUIRE(parsed->live.revered_ash == 2);
    REQUIRE(parsed->live.session_ms == 120000);
    REQUIRE(parsed->live.stats.vigor == 20);
    REQUIRE(parsed->live.flasks.charges == 8);
    REQUIRE(parsed->live.last_grace == "The First Step");
    REQUIRE(parsed->live.last_boss == "Soldier of Godrick");
    REQUIRE(parsed->live.last_killed == "Soldier of Godrick");
    REQUIRE(parsed->live.last_boss_hp_pct == 20);
    REQUIRE(parsed->live.last_boss_right_weapon == "Claymore +0");
    REQUIRE(parsed->live.last_boss_talismans.size() == 1);
    REQUIRE(parsed->live.boss_death_best.at("Soldier of Godrick").hp_pct == 20);
    REQUIRE(parsed->live.bosses_down.size() == 1);
    REQUIRE(parsed->discord_message_id == "msg-1");
}

TEST_CASE("store writes per-run files and pending.json") {
    const auto root = std::filesystem::temp_directory_path() / "erstats_store_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    RunSnapshot snap;
    snap.identity.steam_id = "1";
    snap.identity.save_filename = "ER0000.sl2";
    snap.identity.slot_index = 0;
    snap.identity.character_name = "Wolf";
    snap.live.deaths = 2;

    REQUIRE(save_run(root, snap));
    REQUIRE(write_pending(root, snap));

    const auto loaded = load_run(root, snap.identity.key());
    REQUIRE(loaded);
    REQUIRE(loaded->live.deaths == 2);

    const auto pending = load_pending(root);
    REQUIRE(pending);
    REQUIRE(pending->identity.character_name == "Wolf");
    REQUIRE(clear_pending(root));
    REQUIRE_FALSE(load_pending(root));

    std::filesystem::remove_all(root, ec);
}

TEST_CASE("run store path sanitizes reserved characters") {
    const auto path = run_store_path("C:/tmp", "1/ER0000.sl2/slot0/Wolf");
    REQUIRE(path.filename().string().find('/') == std::string::npos);
    REQUIRE(path.filename().string().find("Wolf") != std::string::npos);
}
