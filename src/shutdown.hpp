#pragma once

namespace erstats {

void install_shutdown_hooks();
void flush_discord();
void post_session_if_needed(const char* reason);

}  // namespace erstats

extern "C" __declspec(dllexport) void stats_share_shutdown();
