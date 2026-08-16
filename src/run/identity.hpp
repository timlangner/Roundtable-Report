#pragma once

#include "run/save_headers.hpp"
#include "run/snapshot.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace erstats {

struct SaveLocation {
    std::string steam_id;
    std::string filename;
    std::filesystem::path path;
};

std::string make_run_key(
    std::string_view steam_id,
    std::string_view save_filename,
    int slot_index,
    std::string_view character_name);

std::optional<RunIdentity> match_slot(
    const std::vector<SlotHeader>& headers,
    const LiveSnapshot& live,
    std::string steam_id,
    std::string save_filename);

std::optional<SaveLocation> find_latest_save(const std::filesystem::path& saves_root);
std::optional<RunIdentity> resolve_identity(
    const LiveSnapshot& live,
    const std::filesystem::path& saves_root);

}  // namespace erstats
