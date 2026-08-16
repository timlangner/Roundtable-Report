#include "run/store.hpp"

#include "util.hpp"

#include <nlohmann/json.hpp>

namespace erstats {
namespace {

nlohmann::json to_json(const RunSnapshot& snapshot) {
    return nlohmann::json{
        {"run_key", snapshot.identity.key()},
        {"steam_id", snapshot.identity.steam_id},
        {"save_filename", snapshot.identity.save_filename},
        {"slot_index", snapshot.identity.slot_index},
        {"character_name", snapshot.identity.character_name},
        {"deaths", snapshot.live.deaths},
        {"session_deaths", snapshot.live.session_deaths},
        {"level", snapshot.live.level},
        {"runes", snapshot.live.runes},
        {"rune_memory", snapshot.live.rune_memory},
        {"igt_ms", snapshot.live.igt_ms},
        {"session_ms", snapshot.live.session_ms},
        {"ng_cycle", snapshot.live.ng_cycle},
        {"map_id", snapshot.live.map_id},
        {"x", snapshot.live.x},
        {"y", snapshot.live.y},
        {"z", snapshot.live.z},
        {"has_position", snapshot.live.has_position},
        {"in_boss_fight", snapshot.live.in_boss_fight},
        {"scadutree_blessing", snapshot.live.scadutree_blessing},
        {"revered_ash", snapshot.live.revered_ash},
        {"vigor", snapshot.live.stats.vigor},
        {"mind", snapshot.live.stats.mind},
        {"endurance", snapshot.live.stats.endurance},
        {"strength", snapshot.live.stats.strength},
        {"dexterity", snapshot.live.stats.dexterity},
        {"intelligence", snapshot.live.stats.intelligence},
        {"faith", snapshot.live.stats.faith},
        {"arcane", snapshot.live.stats.arcane},
        {"flask_charges", snapshot.live.flasks.charges},
        {"flask_tier", snapshot.live.flasks.tier},
        {"flasks_valid", snapshot.live.flasks.valid},
        {"last_grace_id", snapshot.live.last_grace_id},
        {"last_grace", snapshot.live.last_grace},
        {"last_boss", snapshot.live.last_boss},
        {"bosses_down", snapshot.live.bosses_down},
        {"location", snapshot.live.location},
        {"character_loaded", snapshot.live.character_loaded},
        {"discord_message_id", snapshot.discord_message_id},
        {"updated_at", snapshot.updated_at},
    };
}

}  // namespace

std::filesystem::path run_store_path(const std::filesystem::path& root, std::string_view run_key) {
    return root / "runs" / (sanitize_filename(run_key) + ".json");
}

std::filesystem::path pending_path(const std::filesystem::path& root) {
    return root / "pending.json";
}

std::string snapshot_to_json(const RunSnapshot& snapshot) {
    return to_json(snapshot).dump(2);
}

std::optional<RunSnapshot> snapshot_from_json(std::string_view json) {
    try {
        const auto j = nlohmann::json::parse(json);
        RunSnapshot snap;
        snap.identity.steam_id = j.value("steam_id", "");
        snap.identity.save_filename = j.value("save_filename", "");
        snap.identity.slot_index = j.value("slot_index", -1);
        snap.identity.character_name = j.value("character_name", "");
        snap.live.character_name = snap.identity.character_name;
        snap.live.deaths = j.value("deaths", 0u);
        snap.live.session_deaths = j.value("session_deaths", 0u);
        snap.live.level = j.value("level", 0u);
        snap.live.runes = j.value("runes", 0u);
        snap.live.rune_memory = j.value("rune_memory", 0u);
        snap.live.igt_ms = j.value("igt_ms", 0u);
        snap.live.session_ms = j.value("session_ms", 0u);
        snap.live.ng_cycle = j.value("ng_cycle", 0u);
        snap.live.map_id = j.value("map_id", 0u);
        snap.live.x = j.value("x", 0.f);
        snap.live.y = j.value("y", 0.f);
        snap.live.z = j.value("z", 0.f);
        snap.live.has_position = j.value("has_position", false);
        snap.live.in_boss_fight = j.value("in_boss_fight", false);
        snap.live.scadutree_blessing =
            static_cast<uint8_t>(j.value("scadutree_blessing", 0));
        snap.live.revered_ash = static_cast<uint8_t>(j.value("revered_ash", 0));
        snap.live.stats.vigor = j.value("vigor", 0u);
        snap.live.stats.mind = j.value("mind", 0u);
        snap.live.stats.endurance = j.value("endurance", 0u);
        snap.live.stats.strength = j.value("strength", 0u);
        snap.live.stats.dexterity = j.value("dexterity", 0u);
        snap.live.stats.intelligence = j.value("intelligence", 0u);
        snap.live.stats.faith = j.value("faith", 0u);
        snap.live.stats.arcane = j.value("arcane", 0u);
        snap.live.flasks.charges = static_cast<uint8_t>(j.value("flask_charges", 0));
        snap.live.flasks.tier = static_cast<uint8_t>(j.value("flask_tier", 0));
        snap.live.flasks.valid = j.value("flasks_valid", false);
        snap.live.last_grace_id = j.value("last_grace_id", 0u);
        snap.live.last_grace = j.value("last_grace", "");
        snap.live.last_boss = j.value("last_boss", "");
        if (j.contains("bosses_down") && j["bosses_down"].is_array()) {
            snap.live.bosses_down = j["bosses_down"].get<std::vector<std::string>>();
        }
        snap.live.location = j.value("location", "Unknown");
        snap.live.character_loaded = j.value("character_loaded", true);
        snap.discord_message_id = j.value("discord_message_id", "");
        snap.updated_at = j.value("updated_at", "");
        return snap;
    } catch (...) {
        return std::nullopt;
    }
}

bool save_run(const std::filesystem::path& root, const RunSnapshot& snapshot) {
    return write_text_file(run_store_path(root, snapshot.identity.key()), snapshot_to_json(snapshot));
}

std::optional<RunSnapshot> load_run(const std::filesystem::path& root, std::string_view run_key) {
    const auto text = read_text_file(run_store_path(root, run_key));
    if (!text) {
        return std::nullopt;
    }
    return snapshot_from_json(*text);
}

bool write_pending(const std::filesystem::path& root, const RunSnapshot& snapshot) {
    return write_text_file(pending_path(root), snapshot_to_json(snapshot));
}

std::optional<RunSnapshot> load_pending(const std::filesystem::path& root) {
    const auto text = read_text_file(pending_path(root));
    if (!text) {
        return std::nullopt;
    }
    return snapshot_from_json(*text);
}

bool clear_pending(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::remove(pending_path(root), ec);
    return !ec || !std::filesystem::exists(pending_path(root));
}

}  // namespace erstats
