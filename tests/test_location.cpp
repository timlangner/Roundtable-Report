#include <catch2/catch_test_macros.hpp>

#include "game/location.hpp"

#include <cmath>
#include <limits>

using namespace erstats;

TEST_CASE("location_from_map_id names legacy dungeons") {
    const uint32_t stormveil = (10u << 24);
    REQUIRE(location_from_map_id(stormveil) == "Stormveil Castle");
    REQUIRE(location_from_map_id(0) == "Unknown");
}

static uint32_t pack_map(uint8_t area, uint8_t x, uint8_t z, uint8_t size = 0) {
    return (static_cast<uint32_t>(area) << 24) | (static_cast<uint32_t>(x) << 16)
        | (static_cast<uint32_t>(z) << 8) | size;
}

TEST_CASE("overworld tiles map to regions") {
    REQUIRE(location_from_map_id(pack_map(60, 42, 36)) == "Limgrave");
    REQUIRE(location_from_map_id(pack_map(60, 10, 9, 2)) == "Limgrave");
    REQUIRE(location_from_map_id(pack_map(60, 47, 38)) == "Caelid");
    REQUIRE(location_from_map_id(pack_map(60, 43, 33)) == "Weeping Peninsula");
    REQUIRE(location_from_map_id(pack_map(12, 2, 0)) == "Siofra River");
    REQUIRE(location_from_map_id(pack_map(12, 1, 0)) == "Ainsel River");
}

TEST_CASE("journey and boss status copy") {
    REQUIRE(format_journey(0) == "NG");
    REQUIRE(format_journey(2) == "NG+2");
    REQUIRE(format_boss_status(true) == "In a boss fight");
    REQUIRE(format_boss_status(false) == "Exploring");
    REQUIRE(boss_flag_active(1));
    REQUIRE_FALSE(boss_flag_active(0));
    REQUIRE_FALSE(boss_flag_active(2));
    REQUIRE_FALSE(boss_flag_active(0x100));
}

TEST_CASE("live memory map ids pack area in the low byte") {
    const uint32_t limgrave = 60u | (42u << 8) | (36u << 16);
    REQUIRE(location_from_map_id(limgrave) == "Limgrave");
    REQUIRE(parse_map_id(limgrave).area == 60);
    REQUIRE(parse_map_id(limgrave).grid_x == 42);
}

TEST_CASE("Church of Elleh small tile pins Limgrave not the ocean center") {
    const uint32_t elleh = pack_map(60, 42, 36);
    REQUIRE(location_from_map_id(elleh) == "Limgrave");

    const auto [gx, gz] = overworld_global_xz(elleh, 0.f, 0.f);
    REQUIRE(gx == 42.f * 256.f);
    REQUIRE(gz == 36.f * 256.f);

    const auto pin = pin_from_world(elleh, 0.f, 0.f, 0.f, location_from_map_id(elleh));
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::LandsBetween);
    REQUIRE(pin.u > 0.35f);
    REQUIRE(pin.u < 0.58f);
    REQUIRE(pin.v > 0.68f);
    REQUIRE(pin.v < 0.90f);
    const bool ocean_center = pin.u > 0.45f && pin.u < 0.55f && pin.v > 0.45f && pin.v < 0.55f;
    REQUIRE_FALSE(ocean_center);
}

TEST_CASE("West Limgrave large tile projects near Church of Elleh") {
    const uint32_t large = pack_map(60, 10, 9, 2);
    const uint32_t elleh = pack_map(60, 42, 36);
    const auto from_large = pin_from_world(large, 0.f, 0.f, 0.f, location_from_map_id(large));
    const auto from_small = pin_from_world(elleh, 0.f, 0.f, 0.f, location_from_map_id(elleh));
    REQUIRE(from_large.atlas == MapAtlas::LandsBetween);
    REQUIRE(from_small.atlas == MapAtlas::LandsBetween);
    REQUIRE(std::abs(from_large.u - from_small.u) < 0.12f);
    REQUIRE(std::abs(from_large.v - from_small.v) < 0.12f);
}

TEST_CASE("Siofra uses the underground atlas") {
    const uint32_t siofra = pack_map(12, 2, 0);
    REQUIRE(location_from_map_id(siofra) == "Siofra River");
    const auto pin = pin_from_world(siofra, 0.f, 0.f, 0.f, location_from_map_id(siofra));
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::Underground);
}

TEST_CASE("Enir-Ilim projects onto the southwest shadow map via conv params") {
    const uint32_t enir = pack_map(20, 1, 0);
    const auto pin = pin_from_world(enir, 0.f, 0.f, 0.f, "Enir-Ilim");
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::Shadow);
    REQUIRE(pin.u > 0.20f);
    REQUIRE(pin.u < 0.35f);
    REQUIRE(pin.v > 0.40f);
    REQUIRE(pin.v < 0.57f);
}

TEST_CASE("Divine Gate Front Staircase matches the in-game map reference") {
    // Logged-off snapshot: m20_01 Enir-Ilim at Divine Gate Front Staircase.
    // Expected UV is the north rim of the dark circular courtyard on
    // realm_of_shadow.png, matched against the in-game map screenshot.
    const uint32_t enir = pack_map(20, 1, 0);
    const auto pin =
        pin_from_world(enir, -205.113f, 298.889f, -183.646f, "Enir-Ilim");
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::Shadow);
    REQUIRE(std::abs(pin.u - 0.243f) < 0.015f);
    REQUIRE(std::abs(pin.v - 0.550f) < 0.015f);
}

TEST_CASE("Viaduct Minor Tower matches the in-game map reference") {
    // Player standing at the Viaduct Minor Tower grace (m61 tile 46,47);
    // expected UV was measured on realm_of_shadow.png from an in-game shot.
    const uint32_t tile = pack_map(61, 46, 47);
    const auto pin = pin_from_world(tile, 140.8f, 0.f, 38.4f, "Rauh Base");
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::Shadow);
    REQUIRE(std::abs(pin.u - 0.423f) < 0.01f);
    REQUIRE(std::abs(pin.v - 0.417f) < 0.01f);
}

TEST_CASE("Mohgwyn Palace Approach matches the in-game map reference") {
    // Player at the Palace Approach Ledge-Road grace (m12_05); expected UV
    // measured on underworld.png from an in-game reference screenshot.
    const uint32_t mohgwyn = pack_map(12, 5, 0);
    const auto pin = pin_from_world(mohgwyn, 1905.3f, 0.f, 1116.9f, "Mohgwyn Palace");
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::Underground);
    REQUIRE(std::abs(pin.u - 0.604f) < 0.01f);
    REQUIRE(std::abs(pin.v - 0.752f) < 0.01f);
}

TEST_CASE("Stormveil projects onto west Limgrave via conv params") {
    const uint32_t stormveil = pack_map(10, 0, 0);
    const auto pin = pin_from_world(stormveil, 0.f, 0.f, 0.f, "Stormveil Castle");
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::LandsBetween);
    REQUIRE(pin.u > 0.26f);
    REQUIRE(pin.u < 0.39f);
    REQUIRE(pin.v > 0.69f);
    REQUIRE(pin.v < 0.81f);
}

TEST_CASE("Deeproot Depths chains conv params to the northeast underground") {
    const uint32_t deeproot = pack_map(12, 3, 0);
    const auto pin =
        pin_from_world(deeproot, 672.648f, 0.f, 228.076f, "Deeproot Depths");
    REQUIRE(pin.valid);
    REQUIRE(pin.atlas == MapAtlas::Underground);
    // Resolves to global tile (49, 54) via m35 -> m11 -> m60.
    REQUIRE(pin.u > 0.48f);
    REQUIRE(pin.u < 0.65f);
    REQUIRE(pin.v > 0.18f);
    REQUIRE(pin.v < 0.34f);
}

TEST_CASE("dungeon local coordinates move the projected pin") {
    const uint32_t stormveil = pack_map(10, 0, 0);
    const auto at_origin = pin_from_world(stormveil, 0.f, 0.f, 0.f, "Stormveil Castle");
    const auto north = pin_from_world(stormveil, 0.f, 0.f, 200.f, "Stormveil Castle");
    REQUIRE(north.v < at_origin.v);
    REQUIRE(std::abs(north.u - at_origin.u) < 0.01f);
}

TEST_CASE("shadow of the erdtree tiles resolve to DLC regions") {
    const uint32_t gravesite = (61u << 24) | (46u << 16) | (42u << 8);
    REQUIRE(location_from_map_id(gravesite) == "Gravesite Plain");
    REQUIRE(is_shadow_realm(gravesite));

    const uint32_t scadu = (61u << 24) | (48u << 16) | (45u << 8);
    REQUIRE(location_from_map_id(scadu) == "Scadu Altus");

    const uint32_t gravesite_big = (61u << 24) | (11u << 16) | (10u << 8) | 2u;
    REQUIRE(location_from_map_id(gravesite_big) == "Gravesite Plain");

    const uint32_t enir = (20u << 24) | (1u << 16);
    REQUIRE(location_from_map_id(enir) == "Enir-Ilim");
    REQUIRE(location_from_map_id(25u << 24) == "Finger Birthing Grounds");
    REQUIRE(location_from_map_id(28u << 24) == "Midra's Manse");
}

TEST_CASE("dlc blessing fields appear only with DLC progress") {
    REQUIRE(shows_dlc_stats(0, 0, 0) == false);
    REQUIRE(shows_dlc_stats(0, 4, 0) == true);
    REQUIRE(shows_dlc_stats((61u << 24) | (46u << 16) | (42u << 8), 0, 0) == true);
}

TEST_CASE("dungeon pins use named centroids") {
    const uint32_t stormveil = (10u << 24);
    const auto castle = pin_from_world(stormveil, 0.f, 0.f, 0.f, "Stormveil Castle");
    REQUIRE(castle.valid);
    REQUIRE(castle.atlas == MapAtlas::LandsBetween);

    const uint32_t belurat = (20u << 24);
    const auto shadow = pin_from_world(belurat, 0.f, 0.f, 0.f, "Belurat, Tower Settlement");
    REQUIRE(shadow.valid);
    REQUIRE(shadow.atlas == MapAtlas::Shadow);

    const uint32_t gravesite = (61u << 24) | (46u << 16) | (42u << 8);
    const auto plain = pin_from_world(gravesite, 128.f, 0.f, 128.f, "Gravesite Plain");
    REQUIRE(plain.valid);
    REQUIRE(plain.atlas == MapAtlas::Shadow);
}

TEST_CASE("missing position falls back to the named pin, not a fake origin") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    // Church of Elleh small tile: with NaN coords we must not project the tile
    // corner as if it were a real position.
    const uint32_t elleh = pack_map(60, 42, 36);
    const auto named = pin_from_world(elleh, nan, nan, nan, "Limgrave");
    REQUIRE(named.valid);
    REQUIRE(named.atlas == MapAtlas::LandsBetween);

    const auto origin = pin_from_world(elleh, 0.f, 0.f, 0.f, "Limgrave");
    const bool differs = std::fabs(named.u - origin.u) > 1e-4f
        || std::fabs(named.v - origin.v) > 1e-4f;
    REQUIRE(differs);
}
