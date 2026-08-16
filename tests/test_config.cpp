#include <catch2/catch_test_macros.hpp>

#include "config.hpp"

using namespace erstats;

TEST_CASE("parse_hotkey understands alt+o") {
    const auto key = parse_hotkey("Alt+O");
    REQUIRE(key.alt);
    REQUIRE_FALSE(key.ctrl);
    REQUIRE_FALSE(key.shift);
    REQUIRE(key.vk == 0x4F);
}

TEST_CASE("default overlay is left middle and alt+o") {
    const auto cfg = default_config();
    REQUIRE(cfg.overlay_anchor == "left_middle");
    REQUIRE(cfg.toggle_hotkey == "alt+o");
    REQUIRE(cfg.toggle.alt);
    REQUIRE(cfg.toggle.vk == 0x4F);
    REQUIRE(cfg.discord_mode == DiscordMode::New);
}
