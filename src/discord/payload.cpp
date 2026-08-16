#include "discord/payload.hpp"

#include "game/location.hpp"
#include "game/progress.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>

namespace erstats {

std::optional<ParsedWebhook> parse_webhook_url(std::string_view url) {
    if (url.find("http") != 0) {
        return std::nullopt;
    }
    const bool https = url.rfind("https://", 0) == 0;
    const size_t scheme = https ? 8 : 7;
    const auto rest = url.substr(scheme);
    const auto slash = rest.find('/');
    if (slash == std::string_view::npos) {
        return std::nullopt;
    }
    ParsedWebhook parsed;
    parsed.https = https;
    parsed.host = utf8_to_wide(std::string(rest.substr(0, slash)));
    parsed.path = utf8_to_wide(std::string(rest.substr(slash)));
    if (parsed.host.empty() || parsed.path.empty()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<std::string> extract_message_id(std::string_view response_body) {
    if (response_body.empty()) {
        return std::nullopt;
    }
    try {
        const auto json = nlohmann::json::parse(response_body);
        if (json.contains("id") && json["id"].is_string()) {
            return json["id"].get<std::string>();
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::string embed_description(const LiveSnapshot& live) {
    const std::string location = live.location.empty() ? "Unknown" : live.location;
    if (live.in_boss_fight) {
        return location == "Unknown" ? "In a boss fight" : "In a boss fight at " + location;
    }
    return location == "Unknown" ? "Exploring" : "Exploring " + location;
}

std::string build_webhook_payload(const RunSnapshot& snapshot, bool attach_map) {
    nlohmann::json fields = nlohmann::json::array();
    const auto add = [&](std::string name, std::string value, bool inline_field = true) {
        if (value.empty()) {
            return;
        }
        fields.push_back({
            {"name", std::move(name)},
            {"value", std::move(value)},
            {"inline", inline_field},
        });
    };

    const auto session = snapshot.live.session_deaths == 0
        ? format_number(0)
        : "+" + format_number(snapshot.live.session_deaths);

    std::string last_grace = snapshot.live.last_grace;
    if (last_grace.empty()) {
        last_grace = snapshot.live.location.empty() ? "Unknown" : snapshot.live.location;
    }
    add("Last Grace Visited", std::move(last_grace), false);
    add("Deaths", format_number(snapshot.live.deaths));
    add("Session", session);
    add("Session Time", format_igt(snapshot.live.session_ms));
    add("Level", format_number(snapshot.live.level));
    add("Journey", format_journey(snapshot.live.ng_cycle));
    add("Time", format_igt(snapshot.live.igt_ms));
    add("Runes", format_number(snapshot.live.runes));
    add("Lifetime Runes", format_number(snapshot.live.rune_memory));
    if (snapshot.live.flasks.valid) {
        add("Flasks", format_flasks(snapshot.live.flasks));
    }
    if (shows_dlc_stats(
            snapshot.live.map_id, snapshot.live.scadutree_blessing, snapshot.live.revered_ash)) {
        add("Scadutree Blessing", format_number(snapshot.live.scadutree_blessing));
        add("Revered Ash", format_number(snapshot.live.revered_ash));
    }
    if (stats_look_valid(snapshot.live.stats)) {
        add("Attributes", format_attributes(snapshot.live.stats), false);
    }
    if (const auto bosses = format_bosses_down(snapshot.live.bosses_down); !bosses.empty()) {
        add("Bosses this journey", bosses, false);
    }
    if (!snapshot.live.last_boss.empty()) {
        add("Last boss", snapshot.live.last_boss, false);
    }

    const std::string name = snapshot.identity.character_name.empty()
        ? "Tarnished"
        : snapshot.identity.character_name;
    const std::string title = "Game Profile: " + name;

    nlohmann::json embed = {
        {"title", title},
        {"description", embed_description(snapshot.live)},
        {"color", snapshot.live.in_boss_fight ? 0x8B1E1E : 0xC3A456},
        {"fields", fields},
    };
    if (!snapshot.updated_at.empty()) {
        embed["timestamp"] = snapshot.updated_at;
    }
    if (attach_map) {
        embed["image"] = {{"url", "attachment://location.png"}};
    }

    nlohmann::json payload = {{"embeds", nlohmann::json::array({embed})}};
    if (attach_map) {
        payload["attachments"] = nlohmann::json::array({
            {{"id", 0}, {"filename", "location.png"}},
        });
    }
    return payload.dump();
}

}  // namespace erstats
