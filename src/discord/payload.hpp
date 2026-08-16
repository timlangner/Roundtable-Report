#pragma once

#include "run/snapshot.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace erstats {

struct ParsedWebhook {
    std::wstring host;
    std::wstring path;
    bool https = true;
};

std::string build_webhook_payload(const RunSnapshot& snapshot, bool attach_map = false);
std::optional<ParsedWebhook> parse_webhook_url(std::string_view url);
std::optional<std::string> extract_message_id(std::string_view response_body);

}  // namespace erstats
