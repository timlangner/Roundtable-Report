#include "util.hpp"

#include <Windows.h>
#include <ShlObj.h>

#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace erstats {
namespace {

std::mutex g_log_mutex;

std::filesystem::path known_folder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw))) {
        return {};
    }
    std::filesystem::path path(raw);
    CoTaskMemFree(raw);
    return path;
}

void write_log(std::string_view level, std::string_view message) {
    std::lock_guard lock(g_log_mutex);
    const auto dir = mod_data_dir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream out(dir / "mod.log", std::ios::app);
    if (!out) {
        return;
    }
    out << iso8601_now() << " [" << level << "] " << message << "\n";
}

}  // namespace

std::filesystem::path appdata_dir() {
    return known_folder(FOLDERID_RoamingAppData);
}

std::filesystem::path mod_data_dir() {
    return appdata_dir() / "EldenRing_StatsShare";
}

std::filesystem::path elden_ring_saves_root() {
    return appdata_dir() / "EldenRing";
}

std::filesystem::path dll_directory() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&dll_directory),
            &module)) {
        return {};
    }
    wchar_t buffer[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(module, buffer, MAX_PATH);
    if (n == 0) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
}

std::string wide_to_utf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring utf8_to_wide(std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

std::string read_utf16le(const uint8_t* data, size_t max_bytes) {
    if (data == nullptr || max_bytes < 2) {
        return {};
    }
    const size_t wchar_count = max_bytes / 2;
    std::wstring wide;
    wide.reserve(wchar_count);
    for (size_t i = 0; i < wchar_count; ++i) {
        const wchar_t ch = static_cast<wchar_t>(data[i * 2] | (data[i * 2 + 1] << 8));
        if (ch == 0) {
            break;
        }
        wide.push_back(ch);
    }
    return wide_to_utf8(wide);
}

std::string sanitize_filename(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\\':
            case '/':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                out.push_back('_');
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out.empty() ? "unknown" : out;
}

std::string iso8601_now() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_s(&tm, &time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string format_igt(uint32_t milliseconds) {
    const uint32_t total_seconds = milliseconds / 1000;
    const uint32_t hours = total_seconds / 3600;
    const uint32_t minutes = (total_seconds % 3600) / 60;
    const uint32_t seconds = total_seconds % 60;
    std::ostringstream oss;
    oss << hours << ":" << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::setw(2) << std::setfill('0') << seconds;
    return oss.str();
}

std::string format_number(uint32_t value) {
    const std::string raw = std::to_string(value);
    std::string out;
    const size_t n = raw.size();
    for (size_t i = 0; i < n; ++i) {
        out.push_back(raw[i]);
        const size_t remaining = n - i - 1;
        if (remaining > 0 && remaining % 3 == 0) {
            out.push_back(',');
        }
    }
    return out;
}

void log_info(std::string_view message) {
    write_log("info", message);
}

void log_error(std::string_view message) {
    write_log("error", message);
}

std::optional<std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

bool write_text_file(const std::filesystem::path& path, std::string_view contents) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(out);
}

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size <= 0) {
        return {};
    }
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

}  // namespace erstats
