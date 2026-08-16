#include <catch2/catch_test_macros.hpp>

#include "discord/payload.hpp"
#include "util.hpp"

using namespace erstats;

static RunSnapshot sample_snapshot() {
    RunSnapshot snap;
    snap.identity.steam_id = "7656";
    snap.identity.save_filename = "Challenge.sl2";
    snap.identity.slot_index = 2;
    snap.identity.character_name = "Wolf";
    snap.live.character_loaded = true;
    snap.live.character_name = "Wolf";
    snap.live.deaths = 14;
    snap.live.session_deaths = 3;
    snap.live.level = 50;
    snap.live.runes = 12000;
    snap.live.rune_memory = 80000;
    snap.live.igt_ms = 3661000;
    snap.live.session_ms = 2700000;
    snap.live.ng_cycle = 1;
    snap.live.in_boss_fight = true;
    snap.live.location = "Stormveil Castle";
    snap.live.stats = {40, 18, 25, 30, 20, 9, 8, 10};
    snap.live.flasks = {12, 8, true};
    snap.live.last_grace = "Godrick the Grafted";
    snap.live.last_boss = "Godrick the Grafted";
    snap.live.bosses_down = {"Margit, the Fell Omen", "Godrick the Grafted"};
    snap.updated_at = "2026-08-16T01:00:00Z";
    return snap;
}

TEST_CASE("webhook payload contains per-run stats and no secret url") {
    const auto json = build_webhook_payload(sample_snapshot());
    REQUIRE(json.find("Game Profile: Wolf") != std::string::npos);
    REQUIRE(json.find("\"author\"") == std::string::npos);
    REQUIRE(json.find("In a boss fight at Stormveil Castle") != std::string::npos);
    REQUIRE(json.find("\"Location\"") == std::string::npos);
    REQUIRE(json.find("Last Grace Visited") != std::string::npos);
    REQUIRE(json.find("NG+1") != std::string::npos);
    REQUIRE(json.find("14") != std::string::npos);
    REQUIRE(json.find("\"Session\"") != std::string::npos);
    REQUIRE(json.find("\"Time\"") != std::string::npos);
    REQUIRE(json.find("Session Time") != std::string::npos);
    REQUIRE(json.find("Lifetime Runes") != std::string::npos);
    REQUIRE(json.find("80,000") != std::string::npos);
    REQUIRE(json.find("12 flasks +8") != std::string::npos);
    REQUIRE(json.find("Vigor 40") != std::string::npos);
    REQUIRE(json.find("Last Site of Grace") == std::string::npos);
    REQUIRE(json.find("Godrick the Grafted") != std::string::npos);
    REQUIRE(json.find("Bosses") != std::string::npos);
    REQUIRE(json.find("\xE2\x80\x94") == std::string::npos);
    REQUIRE(json.find("\xE2\x80\x93") == std::string::npos);
    REQUIRE(json.find("Challenge.sl2") == std::string::npos);
    REQUIRE(json.find("Slot") == std::string::npos);
    REQUIRE(json.find("7656/Challenge.sl2/slot2/Wolf") == std::string::npos);
    REQUIRE(json.find("webhook") == std::string::npos);
    REQUIRE(json.find("https://") == std::string::npos);
}

TEST_CASE("parse_webhook_url splits host and path") {
    const auto parsed = parse_webhook_url("https://discord.com/api/webhooks/1/abc");
    REQUIRE(parsed);
    REQUIRE(parsed->https);
    REQUIRE(parsed->host == L"discord.com");
    REQUIRE(parsed->path == L"/api/webhooks/1/abc");
    REQUIRE_FALSE(parse_webhook_url("not-a-url"));
}

TEST_CASE("extract_message_id reads Discord JSON") {
    REQUIRE(extract_message_id(R"({"id":"42","content":"x"})") == "42");
    REQUIRE_FALSE(extract_message_id("not-json"));
}

TEST_CASE("format_igt renders hours") {
    REQUIRE(format_igt(3661000) == "1:01:01");
}

TEST_CASE("webhook payload includes DLC blessings when present") {
    auto snap = sample_snapshot();
    snap.live.scadutree_blessing = 12;
    snap.live.revered_ash = 7;
    const auto json = build_webhook_payload(snap);
    REQUIRE(json.find("Scadutree Blessing") != std::string::npos);
    REQUIRE(json.find("Revered Ash") != std::string::npos);
    REQUIRE(json.find("12") != std::string::npos);
    REQUIRE(json.find("7") != std::string::npos);
    REQUIRE(json.find("\xE2\x80\x94") == std::string::npos);
}

TEST_CASE("webhook payload hides DLC blessings in the Lands Between") {
    const auto json = build_webhook_payload(sample_snapshot());
    REQUIRE(json.find("Scadutree Blessing") == std::string::npos);
    REQUIRE(json.find("Revered Ash") == std::string::npos);
}

TEST_CASE("last grace row falls back to the location name") {
    auto snap = sample_snapshot();
    snap.live.last_grace.clear();
    const auto json = build_webhook_payload(snap);
    REQUIRE(json.find("Last Grace Visited") != std::string::npos);
    REQUIRE(json.find("Stormveil Castle") != std::string::npos);
}

TEST_CASE("webhook payload can attach a location map") {
    const auto json = build_webhook_payload(sample_snapshot(), true);
    REQUIRE(json.find("attachment://location.png") != std::string::npos);
    REQUIRE(json.find("location.png") != std::string::npos);
    REQUIRE(json.find("https://") == std::string::npos);
}
