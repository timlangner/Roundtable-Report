#include <catch2/catch_test_macros.hpp>

#include "discord/map_image.hpp"
#include "run/snapshot.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include "stb_image.h"

using namespace erstats;

TEST_CASE("location map png starts with a png signature") {
    RunSnapshot snap;
    snap.live.map_id = (60u << 24) | (42u << 16) | (36u << 8);
    snap.live.x = 0.f;
    snap.live.z = 0.f;
    snap.live.has_position = true;
    snap.live.location = "Limgrave";

    const auto png = render_location_map(snap);
    REQUIRE(png);
    REQUIRE(png->size() > 32);
    REQUIRE((*png)[0] == 137);
    REQUIRE((*png)[1] == 80);
    REQUIRE((*png)[2] == 78);
    REQUIRE((*png)[3] == 71);
}

TEST_CASE("unknown location without a map id yields no image") {
    RunSnapshot snap;
    REQUIRE_FALSE(render_location_map(snap));
}

static uint32_t png_be32(const std::vector<uint8_t>& png, size_t offset) {
    return (static_cast<uint32_t>(png[offset]) << 24)
        | (static_cast<uint32_t>(png[offset + 1]) << 16)
        | (static_cast<uint32_t>(png[offset + 2]) << 8)
        | static_cast<uint32_t>(png[offset + 3]);
}

static void require_photo_png(const std::optional<std::vector<uint8_t>>& png) {
    REQUIRE(png);
    REQUIRE(png->size() > 24);
    REQUIRE(png_be32(*png, 16) == 800);
    REQUIRE(png_be32(*png, 20) == 500);
}

TEST_CASE("lands between photo map is cropped to 800x500") {
    RunSnapshot snap;
    snap.live.map_id = (60u << 24) | (42u << 16) | (36u << 8);
    snap.live.x = 0.f;
    snap.live.z = 0.f;
    snap.live.has_position = true;
    snap.live.location = "Limgrave";

    require_photo_png(render_location_map(snap));
}

TEST_CASE("photo map uses the canvas width as its pixel stride") {
    RunSnapshot snap;
    snap.live.map_id = (60u << 24) | (42u << 16) | (36u << 8);
    snap.live.x = 0.f;
    snap.live.z = 0.f;
    snap.live.has_position = true;
    snap.live.location = "Limgrave";

    const auto png = render_location_map(snap);
    require_photo_png(png);

    int w = 0;
    int h = 0;
    int comp = 0;
    unsigned char* pixels = stbi_load_from_memory(
        png->data(), static_cast<int>(png->size()), &w, &h, &comp, 4);
    REQUIRE(pixels != nullptr);
    REQUIRE(w == 800);
    REQUIRE(h == 500);

    const auto at = [&](int x, int y) -> const unsigned char* {
        return pixels + (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
    };
    const unsigned char* bottom = at(400, 498);
    const unsigned char* mid = at(400, 250);
    REQUIRE(bottom[0] == 195);
    REQUIRE(bottom[1] == 164);
    REQUIRE(bottom[2] == 86);
    const bool mid_is_parchment = mid[0] == 18 && mid[1] == 14 && mid[2] == 10;
    REQUIRE_FALSE(mid_is_parchment);

    stbi_image_free(pixels);
}

TEST_CASE("underworld photo map is used for Siofra") {
    RunSnapshot snap;
    snap.live.map_id = (12u << 24) | (2u << 16);
    snap.live.location = "Siofra River";
    require_photo_png(render_location_map(snap));
}

TEST_CASE("realm of shadow photo map is used for Gravesite Plain") {
    RunSnapshot snap;
    snap.live.map_id = (61u << 24) | (46u << 16) | (42u << 8);
    snap.live.x = 128.f;
    snap.live.z = 128.f;
    snap.live.has_position = true;
    snap.live.location = "Gravesite Plain";
    require_photo_png(render_location_map(snap));
}

// Hidden calibration aid: writes sample map crops for known locations so the
// pin placement can be checked by eye. Run with: stats_share_tests "[render]"
TEST_CASE("write sample renders for visual pin verification", "[.][render]") {
    struct Sample {
        const char* file;
        uint32_t map_id;
        float x;
        float z;
        const char* location;
    };
    const Sample samples[] = {
        {"render_elleh.png", (60u << 24) | (42u << 16) | (36u << 8), -30.f, -60.f, "Limgrave"},
        {"render_enir_ilim.png", (20u << 24) | (1u << 16), 0.f, 0.f, "Enir-Ilim"},
        {"render_divine_gate.png", (20u << 24) | (1u << 16), -205.113f, -183.646f, "Enir-Ilim"},
        {"render_stormveil.png", (10u << 24), 0.f, 0.f, "Stormveil Castle"},
        {"render_leyndell.png", (11u << 24), -90.f, 66.5f, "Leyndell, Royal Capital"},
        {"render_shadow_keep.png", (21u << 24), 0.f, 0.f, "Shadow Keep"},
        {"render_siofra.png", (12u << 24) | (2u << 16), 1549.9f, 1910.8f, "Siofra River"},
        {"render_belurat.png", (20u << 24), 0.f, 0.f, "Belurat, Tower Settlement"},
        {"render_viaduct.png", (61u << 24) | (46u << 16) | (47u << 8), 140.8f, 38.4f, "Rauh Base"},
        {"render_mohgwyn.png", (12u << 24) | (5u << 16), 1905.3f, 1116.9f, "Mohgwyn Palace"},
    };
    for (const auto& sample : samples) {
        RunSnapshot snap;
        snap.live.map_id = sample.map_id;
        snap.live.x = sample.x;
        snap.live.z = sample.z;
        snap.live.has_position = true;
        snap.live.location = sample.location;
        const auto png = render_location_map(snap);
        REQUIRE(png);
        std::FILE* out = std::fopen(sample.file, "wb");
        REQUIRE(out != nullptr);
        std::fwrite(png->data(), 1, png->size(), out);
        std::fclose(out);
    }
}
