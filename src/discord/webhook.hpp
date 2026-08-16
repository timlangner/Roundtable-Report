#pragma once

#include "config.hpp"
#include "discord/payload.hpp"
#include "run/snapshot.hpp"

#include <string>

namespace erstats {

struct WebhookResult {
    bool ok = false;
    int status = 0;
    std::string message_id;
    std::string error;
};

WebhookResult post_or_edit_webhook(const Config& config, RunSnapshot& snapshot);

}  // namespace erstats
