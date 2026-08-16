#include "run/identity.hpp"

#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace erstats {
namespace {

bool is_steam_id_folder(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](unsigned char ch) {
        return std::isdigit(ch);
    });
}

bool is_save_file(const std::filesystem::path& path) {
    const auto ext = path.extension().wstring();
    return ext == L".sl2" || ext == L".co2" || ext == L".mod" || ext == L".sl2.bak";
}

}  // namespace

std::string RunIdentity::key() const {
    return make_run_key(steam_id, save_filename, slot_index, character_name);
}

bool RunIdentity::valid() const {
    return !steam_id.empty() && !save_filename.empty() && !character_name.empty() && slot_index >= 0;
}

std::string make_run_key(
    std::string_view steam_id,
    std::string_view save_filename,
    int slot_index,
    std::string_view character_name) {
    std::ostringstream oss;
    oss << steam_id << "/" << save_filename << "/slot" << slot_index << "/" << character_name;
    return oss.str();
}

std::optional<RunIdentity> match_slot(
    const std::vector<SlotHeader>& headers,
    const LiveSnapshot& live,
    std::string steam_id,
    std::string save_filename) {
    if (!live.character_loaded || live.character_name.empty()) {
        return std::nullopt;
    }

    const uint32_t live_seconds = live.igt_ms / 1000;
    const SlotHeader* best = nullptr;
    int best_score = -1;
    uint32_t best_delta = std::numeric_limits<uint32_t>::max();

    for (const auto& slot : headers) {
        if (!slot.active || slot.name != live.character_name) {
            continue;
        }
        int score = 1;
        uint32_t delta = std::numeric_limits<uint32_t>::max();
        if (slot.level == live.level) {
            score = 2;
            const uint32_t d = slot.playtime_seconds > live_seconds
                ? slot.playtime_seconds - live_seconds
                : live_seconds - slot.playtime_seconds;
            delta = d;
            if (d <= 30) {
                score = 3;
            }
        }
        if (score > best_score || (score == best_score && delta < best_delta)) {
            best = &slot;
            best_score = score;
            best_delta = delta;
        }
    }

    if (best == nullptr) {
        return std::nullopt;
    }

    RunIdentity id;
    id.steam_id = std::move(steam_id);
    id.save_filename = std::move(save_filename);
    id.slot_index = best->index;
    id.character_name = live.character_name;
    return id;
}

std::optional<SaveLocation> find_latest_save(const std::filesystem::path& saves_root) {
    std::error_code ec;
    if (!std::filesystem::exists(saves_root, ec)) {
        return std::nullopt;
    }

    std::optional<SaveLocation> best;
    std::filesystem::file_time_type best_time{};
    bool have_time = false;

    for (const auto& dir : std::filesystem::directory_iterator(saves_root, ec)) {
        if (!dir.is_directory()) {
            continue;
        }
        const std::string steam_id = dir.path().filename().string();
        if (!is_steam_id_folder(steam_id)) {
            continue;
        }
        for (const auto& file : std::filesystem::directory_iterator(dir.path(), ec)) {
            if (!file.is_regular_file() || !is_save_file(file.path())) {
                continue;
            }
            const auto time = file.last_write_time(ec);
            if (ec) {
                continue;
            }
            if (!have_time || time > best_time) {
                have_time = true;
                best_time = time;
                best = SaveLocation{steam_id, file.path().filename().string(), file.path()};
            }
        }
    }
    return best;
}

std::optional<RunIdentity> resolve_identity(
    const LiveSnapshot& live,
    const std::filesystem::path& saves_root) {
    const auto save = find_latest_save(saves_root);
    if (!save) {
        return std::nullopt;
    }
    auto bytes = maybe_decrypt_sl2(read_binary_file(save->path));
    const auto headers = parse_save_headers(bytes);
    if (!headers) {
        return std::nullopt;
    }
    return match_slot(*headers, live, save->steam_id, save->filename);
}

}  // namespace erstats
