#include "config.hpp"

#include "util.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>

namespace erstats {

Config default_config() {
    return {};
}

DiscordMode discord_mode_from_string(std::string_view value) {
    std::string lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower == "edit" ? DiscordMode::Edit : DiscordMode::New;
}

std::string discord_mode_to_string(DiscordMode mode) {
    return mode == DiscordMode::New ? "new" : "edit";
}

Hotkey parse_hotkey(std::string_view spec) {
    Hotkey key;
    key.alt = false;
    key.ctrl = false;
    key.shift = false;
    key.vk = 0;

    std::string lower(spec);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    size_t start = 0;
    while (start < lower.size()) {
        const size_t plus = lower.find('+', start);
        const std::string part = lower.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        if (part == "alt") {
            key.alt = true;
        } else if (part == "ctrl" || part == "control") {
            key.ctrl = true;
        } else if (part == "shift") {
            key.shift = true;
        } else if (part.size() == 1 && part[0] >= 'a' && part[0] <= 'z') {
            key.vk = static_cast<int>(std::toupper(static_cast<unsigned char>(part[0])));
        } else if (part == "f8") {
            key.vk = 0x77;
        }
        if (plus == std::string::npos) {
            break;
        }
        start = plus + 1;
    }
    if (key.vk == 0) {
        return Hotkey{};
    }
    return key;
}

Config load_config(const std::filesystem::path& path) {
    Config cfg = default_config();
    if (!std::filesystem::exists(path)) {
        log_info("config not found, using defaults");
        return cfg;
    }

    try {
        const auto table = toml::parse_file(path.string());
        if (const auto* discord = table["discord"].as_table()) {
            if (const auto url = (*discord)["webhook_url"].value<std::string>()) {
                cfg.webhook_url = *url;
            }
            if (const auto mode = (*discord)["mode"].value<std::string>()) {
                cfg.discord_mode = discord_mode_from_string(*mode);
            }
        }
        if (const auto* overlay = table["overlay"].as_table()) {
            if (const auto hotkey = (*overlay)["toggle_hotkey"].value<std::string>()) {
                cfg.toggle_hotkey = *hotkey;
                cfg.toggle = parse_hotkey(*hotkey);
            } else if (const auto key = (*overlay)["toggle_vk"].value<int64_t>()) {
                cfg.toggle = Hotkey{};
                cfg.toggle.alt = false;
                cfg.toggle.vk = static_cast<int>(*key);
                cfg.toggle_hotkey = "vk";
            }
            if (const auto show = (*overlay)["show_session_deaths"].value<bool>()) {
                cfg.show_session_deaths = *show;
            }
            if (const auto anchor = (*overlay)["anchor"].value<std::string>()) {
                cfg.overlay_anchor = *anchor;
            }
        }
    } catch (const std::exception& ex) {
        log_error(std::string("failed to parse config: ") + ex.what());
    }
    return cfg;
}

}  // namespace erstats
