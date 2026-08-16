#pragma once

#include <filesystem>
#include <string>

namespace erstats {

enum class DiscordMode {
    Edit,
    New,
};

struct Hotkey {
    bool alt = true;
    bool ctrl = false;
    bool shift = false;
    int vk = 0x4F;  // O
};

struct Config {
    std::string webhook_url;
    DiscordMode discord_mode = DiscordMode::New;
    Hotkey toggle;
    std::string toggle_hotkey = "alt+o";
    bool show_session_deaths = true;
    std::string overlay_anchor = "left_middle";
};

Config default_config();
Config load_config(const std::filesystem::path& path);
Hotkey parse_hotkey(std::string_view spec);
std::string discord_mode_to_string(DiscordMode mode);
DiscordMode discord_mode_from_string(std::string_view value);

}  // namespace erstats
