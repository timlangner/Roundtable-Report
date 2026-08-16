#include "shutdown.hpp"

#include "app.hpp"
#include "discord/webhook.hpp"
#include "run/store.hpp"
#include "util.hpp"

#include <Windows.h>
#include <MinHook.h>

#include <mutex>

namespace erstats {
namespace {

using ExitProcess_t = void(WINAPI*)(UINT);
ExitProcess_t g_original_exit = nullptr;
std::mutex g_discord_post_mutex;

bool post_current_run(const char* reason) {
    auto& state = app();
    persist_current();
    RunSnapshot snap;
    {
        std::lock_guard lock(state.mutex);
        snap = state.current;
    }
    if (!snap.identity.valid()) {
        log_info(std::string(reason) + " with no bound run, skipping Discord");
        clear_pending(mod_data_dir());
        return false;
    }
    if (state.config.webhook_url.empty()) {
        log_info("Discord webhook not configured");
        return false;
    }

    log_info(std::string("posting Discord summary (") + reason + ")");
    const auto result = post_or_edit_webhook(state.config, snap);
    {
        std::lock_guard lock(state.mutex);
        state.current.discord_message_id = snap.discord_message_id;
    }
    save_run(mod_data_dir(), snap);
    if (result.ok) {
        clear_pending(mod_data_dir());
        return true;
    }
    log_error("Discord post failed; pending.json kept for next launch");
    return false;
}

void mark_session_posted() {
    auto& state = app();
    std::lock_guard lock(state.mutex);
    state.session.session_posted = true;
    state.session.session_active = false;
}

bool already_posted_this_session() {
    auto& state = app();
    std::lock_guard lock(state.mutex);
    return state.session.session_posted;
}

void do_flush() {
    auto& state = app();
    bool expected = false;
    if (!state.flushed.compare_exchange_strong(expected, true)) {
        return;
    }

    std::lock_guard post_lock(g_discord_post_mutex);
    if (already_posted_this_session()) {
        log_info("Discord already posted for this session, skipping");
        persist_current();
        clear_pending(mod_data_dir());
        return;
    }
    if (post_current_run("exit")) {
        mark_session_posted();
    }
}

void WINAPI hooked_exit_process(UINT code) {
    do_flush();
    if (g_original_exit) {
        g_original_exit(code);
    }
}

}  // namespace

void install_shutdown_hooks() {
    const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    if (kernel == nullptr) {
        return;
    }
    auto* exit_process = reinterpret_cast<void*>(GetProcAddress(kernel, "ExitProcess"));
    if (exit_process == nullptr) {
        return;
    }
    if (MH_CreateHook(exit_process, reinterpret_cast<LPVOID>(&hooked_exit_process),
                      reinterpret_cast<LPVOID*>(&g_original_exit))
        == MH_OK) {
        MH_EnableHook(exit_process);
        log_info("ExitProcess hook installed");
    }
}

void flush_discord() {
    do_flush();
}

void post_session_if_needed(const char* reason) {
    std::lock_guard post_lock(g_discord_post_mutex);
    if (already_posted_this_session()) {
        return;
    }
    if (post_current_run(reason)) {
        mark_session_posted();
    }
}

}  // namespace erstats

extern "C" __declspec(dllexport) void stats_share_shutdown() {
    erstats::flush_discord();
}
