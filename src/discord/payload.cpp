#include "discord/payload.hpp"

#include "game/item_names.hpp"
#include "game/location.hpp"
#include "game/progress.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>

namespace erstats {

std::string embed_description(const LiveSnapshot& live);

namespace {

nlohmann::json make_field(std::string name, std::string value, bool inline_field = true) {
    return {
        {"name", std::move(name)},
        {"value", std::move(value)},
        {"inline", inline_field},
    };
}

void add_field(nlohmann::json& fields, std::string name, std::string value, bool inline_field = true) {
    if (value.empty()) {
        return;
    }
    fields.push_back(make_field(std::move(name), std::move(value), inline_field));
}

nlohmann::json profile_embed(const RunSnapshot& snapshot, bool attach_map) {
    nlohmann::json fields = nlohmann::json::array();
    const auto session = snapshot.live.session_deaths == 0
        ? format_number(0)
        : "+" + format_number(snapshot.live.session_deaths);

    std::string last_grace = snapshot.live.last_grace;
    if (last_grace.empty()) {
        last_grace = snapshot.live.location.empty() ? "Unknown" : snapshot.live.location;
    }
    add_field(fields, "Last Grace Visited", std::move(last_grace), false);
    add_field(fields, "Deaths", format_number(snapshot.live.deaths));
    add_field(fields, "Session", session);
    add_field(fields, "Session Time", format_igt(snapshot.live.session_ms));
    add_field(fields, "Level", format_number(snapshot.live.level));
    add_field(fields, "Journey", format_journey(snapshot.live.ng_cycle));
    add_field(fields, "Time", format_igt(snapshot.live.igt_ms));
    add_field(fields, "Runes", format_number(snapshot.live.runes));
    add_field(fields, "Lifetime Runes", format_number(snapshot.live.rune_memory));
    if (snapshot.live.flasks.valid) {
        add_field(fields, "Flasks", format_flasks(snapshot.live.flasks));
    }
    if (shows_dlc_stats(
            snapshot.live.map_id, snapshot.live.scadutree_blessing, snapshot.live.revered_ash)) {
        add_field(fields, "Scadutree Blessing", format_number(snapshot.live.scadutree_blessing));
        add_field(fields, "Revered Ash", format_number(snapshot.live.revered_ash));
    }

    const std::string name = snapshot.identity.character_name.empty()
        ? "Tarnished"
        : snapshot.identity.character_name;
    nlohmann::json embed = {
        {"title", "Game Profile: " + name},
        {"description", embed_description(snapshot.live)},
        {"color", snapshot.live.in_boss_fight ? 0x8B1E1E : 0xC3A456},
        {"fields", std::move(fields)},
    };
    if (!snapshot.updated_at.empty()) {
        embed["timestamp"] = snapshot.updated_at;
    }
    if (attach_map) {
        embed["image"] = {{"url", "attachment://location.png"}};
    }
    return embed;
}

nlohmann::json last_boss_embed(const LiveSnapshot& live) {
    nlohmann::json fields = nlohmann::json::array();
    if (live.last_boss_hp_pct) {
        add_field(fields, "Best try", format_number(*live.last_boss_hp_pct) + "% HP left");
    }
    add_field(
        fields,
        "Weapons",
        format_weapons_line(live.last_boss_right_weapon, live.last_boss_left_weapon),
        false);
    add_field(fields, "Talismans", format_talismans_line(live.last_boss_talismans), false);
    return {
        {"title", "Last boss"},
        {"description", live.last_boss},
        {"color", 0x8B1E1E},
        {"fields", std::move(fields)},
    };
}

nlohmann::json build_journey_embed(const LiveSnapshot& live) {
    nlohmann::json fields = nlohmann::json::array();
    add_field(fields, "Last killed", live.last_killed, false);
    if (stats_look_valid(live.stats)) {
        add_field(fields, "Attributes", format_attributes(live.stats), false);
    }
    if (const auto bosses = format_bosses_down(live.bosses_down); !bosses.empty()) {
        add_field(fields, "Bosses this journey", bosses, false);
    }
    if (fields.empty()) {
        return nlohmann::json{};
    }
    return {
        {"title", "Build & journey"},
        {"color", 0x5C4A32},
        {"fields", std::move(fields)},
    };
}

}  // namespace

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
    nlohmann::json embeds = nlohmann::json::array();
    embeds.push_back(profile_embed(snapshot, attach_map));
    if (!snapshot.live.last_boss.empty()) {
        embeds.push_back(last_boss_embed(snapshot.live));
    }
    if (auto journey = build_journey_embed(snapshot.live); !journey.is_null() && !journey.empty()) {
        embeds.push_back(std::move(journey));
    }

    nlohmann::json payload = {{"embeds", std::move(embeds)}};
    if (attach_map) {
        payload["attachments"] = nlohmann::json::array({
            {{"id", 0}, {"filename", "location.png"}},
        });
    }
    return payload.dump();
}

}  // namespace erstats
