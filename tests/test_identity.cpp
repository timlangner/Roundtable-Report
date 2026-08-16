#include <catch2/catch_test_macros.hpp>

#include "run/identity.hpp"
#include "run/save_headers.hpp"
#include "util.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace erstats;

TEST_CASE("run key is steam/file/slot/name") {
    REQUIRE(make_run_key("7656", "ER0000.sl2", 3, "Wolf") == "7656/ER0000.sl2/slot3/Wolf");
}

TEST_CASE("match_slot binds the loaded character, not every slot") {
    const auto bytes = make_synthetic_save({
        SlotHeader{true, 0, "Wolf", 12, 100},
        SlotHeader{true, 1, "Blaidd", 80, 8000},
    });
    const auto headers = parse_save_headers(bytes);
    REQUIRE(headers);

    LiveSnapshot live;
    live.character_loaded = true;
    live.character_name = "Blaidd";
    live.level = 80;
    live.igt_ms = 8000 * 1000;

    const auto id = match_slot(*headers, live, "7656119", "ER0000.sl2");
    REQUIRE(id);
    REQUIRE(id->slot_index == 1);
    REQUIRE(id->character_name == "Blaidd");
    REQUIRE(id->key() == "7656119/ER0000.sl2/slot1/Blaidd");
}

TEST_CASE("match_slot does not return a different character") {
    const auto bytes = make_synthetic_save({
        SlotHeader{true, 0, "Wolf", 12, 100},
        SlotHeader{true, 1, "Blaidd", 80, 8000},
    });
    const auto headers = parse_save_headers(bytes);

    LiveSnapshot live;
    live.character_loaded = true;
    live.character_name = "Wolf";
    live.level = 12;
    live.igt_ms = 100000;

    const auto id = match_slot(*headers, live, "1", "ER0000.co2");
    REQUIRE(id);
    REQUIRE(id->slot_index == 0);
    REQUIRE(id->save_filename == "ER0000.co2");
}

TEST_CASE("match_slot ignores empty title-screen snapshots") {
    const auto bytes = make_synthetic_save({SlotHeader{true, 0, "Wolf", 12, 100}});
    const auto headers = parse_save_headers(bytes);
    LiveSnapshot live;
    REQUIRE_FALSE(match_slot(*headers, live, "1", "ER0000.sl2"));
}

TEST_CASE("find_latest_save prefers the newest sl2 or co2") {
    const auto root = std::filesystem::temp_directory_path() / "erstats_identity_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto steam = root / "76561198000000000";
    std::filesystem::create_directories(steam);

    const auto older = steam / "ER0000.sl2";
    const auto newer = steam / "ER0000.co2";
    {
        std::ofstream(older) << "old";
        std::ofstream(newer) << "new";
    }
    std::filesystem::last_write_time(older, std::filesystem::file_time_type::clock::now() - std::chrono::seconds(60));
    std::filesystem::last_write_time(newer, std::filesystem::file_time_type::clock::now());

    const auto found = find_latest_save(root);
    REQUIRE(found);
    REQUIRE(found->steam_id == "76561198000000000");
    REQUIRE(found->filename == "ER0000.co2");
    std::filesystem::remove_all(root, ec);
}
