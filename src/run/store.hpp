#pragma once

#include "run/snapshot.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace erstats {

std::filesystem::path run_store_path(const std::filesystem::path& root, std::string_view run_key);
std::filesystem::path pending_path(const std::filesystem::path& root);

std::string snapshot_to_json(const RunSnapshot& snapshot);
std::optional<RunSnapshot> snapshot_from_json(std::string_view json);

bool save_run(const std::filesystem::path& root, const RunSnapshot& snapshot);
std::optional<RunSnapshot> load_run(const std::filesystem::path& root, std::string_view run_key);

bool write_pending(const std::filesystem::path& root, const RunSnapshot& snapshot);
std::optional<RunSnapshot> load_pending(const std::filesystem::path& root);
bool clear_pending(const std::filesystem::path& root);

}  // namespace erstats
