#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace erstats {

std::filesystem::path appdata_dir();
std::filesystem::path mod_data_dir();
std::filesystem::path elden_ring_saves_root();
std::filesystem::path dll_directory();

std::string wide_to_utf8(std::wstring_view value);
std::wstring utf8_to_wide(std::string_view value);
std::string read_utf16le(const uint8_t* data, size_t max_bytes);

std::string sanitize_filename(std::string_view value);
std::string iso8601_now();
std::string format_igt(uint32_t milliseconds);
std::string format_number(uint32_t value);

void log_info(std::string_view message);
void log_error(std::string_view message);

std::optional<std::string> read_text_file(const std::filesystem::path& path);
bool write_text_file(const std::filesystem::path& path, std::string_view contents);
std::vector<uint8_t> read_binary_file(const std::filesystem::path& path);

}  // namespace erstats
