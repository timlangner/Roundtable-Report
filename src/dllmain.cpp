#include "app.hpp"

#include "discord/webhook.hpp"
#include "overlay/dx12_hook.hpp"
#include "overlay/hud.hpp"
#include "game/progress.hpp"
#include "run/identity.hpp"
#include "run/store.hpp"
#include "shutdown.hpp"
#include "util.hpp"

#include <Windows.h>
#include <MinHook.h>

#include <filesystem>

namespace erstats {
namespace {

AppState g_app;

void retry_pending() {
    const auto root = mod_data_dir();
    auto pending = load_pending(root);
    if (!pending || !pending->identity.valid()) {
        return;
    }
    if (g_app.config.webhook_url.empty()) {
        return;
    }
    log_info("posting pending Discord summary from a previous session");
    const auto result = post_or_edit_webhook(g_app.config, *pending);
    if (result.ok) {
        save_run(root, *pending);
        clear_pending(root);
    } else {
        log_error("pending Discord post failed");
    }
}

}  // namespace

AppState& app() {
    return g_app;
}

void persist_current() {
    std::lock_guard lock(g_app.mutex);
    if (!g_app.current.identity.valid()) {
        return;
    }
    g_app.current.updated_at = iso8601_now();
    const auto root = mod_data_dir();
    save_run(root, g_app.current);
    write_pending(root, g_app.current);
}

void tick_once() {
    if (!g_app.game.ready() || !g_app.game.world_ready()) {
        g_app.game.locate();
        if (!g_app.game.ready()) {
            return;
        }
    }

    LiveSnapshot live = g_app.game.read();
    std::optional<RunIdentity> identity;
    if (live.character_loaded) {
        identity = resolve_identity(live, elden_ring_saves_root());
    }

    SessionTickAction session_action = SessionTickAction::None;
    {
        std::lock_guard lock(g_app.mutex);
        if (identity && identity->valid()) {
            const std::string key = identity->key();
            const bool same_run = key == g_app.loaded_run_key;
            if (same_run) {
                live.deaths = stabilize_death_count(live.deaths, g_app.current.live.deaths);
            }
            const bool in_world = live.map_id != 0 || live.has_position;
            if (!same_run) {
                g_app.loaded_run_key = key;
                g_app.baseline = {};
                if (const auto stored = load_run(mod_data_dir(), key)) {
                    g_app.current.discord_message_id = stored->discord_message_id;
                } else {
                    g_app.current.discord_message_id.clear();
                }
                log_info("bound run " + key);
            }
            if (live.map_id == 0 && g_app.current.live.map_id != 0) {
                live.map_id = g_app.current.live.map_id;
                live.location = g_app.current.live.location;
                live.x = g_app.current.live.x;
                live.y = g_app.current.live.y;
                live.z = g_app.current.live.z;
                live.has_position = g_app.current.live.has_position;
            } else if (!live.has_position && g_app.current.live.has_position
                       && live.map_id == g_app.current.live.map_id) {
                // Position read failed but we are still on the same map; keep
                // the last known coordinates so the pin stays accurate.
                live.x = g_app.current.live.x;
                live.y = g_app.current.live.y;
                live.z = g_app.current.live.z;
                live.has_position = true;
            }
            if (live.last_grace.empty() && !g_app.current.live.last_grace.empty()) {
                live.last_grace = g_app.current.live.last_grace;
                live.last_grace_id = g_app.current.live.last_grace_id;
            }
            if (same_run && live.ng_cycle == g_app.current.live.ng_cycle) {
                // Union with the previous poll: flag reads can transiently
                // miss bosses (notably DLC flags in separate tree nodes).
                live.bosses_down =
                    merge_bosses_down(g_app.current.live.bosses_down, live.bosses_down);
            }
            if (!live.flasks.valid && g_app.current.live.flasks.valid) {
                live.flasks = g_app.current.live.flasks;
            }
            if (!stats_look_valid(live.stats) && stats_look_valid(g_app.current.live.stats)) {
                live.stats = g_app.current.live.stats;
            }
            g_app.current.identity = *identity;
            g_app.current.live = live;
            session_action = note_character_presence(g_app.session, true);
            if (session_action == SessionTickAction::SessionStarted) {
                begin_session_baseline(g_app.baseline, live.deaths, in_world);
                g_app.session_started_ms = GetTickCount64();
                g_app.last_boss_fought.clear();
            } else {
                refresh_session_baseline(g_app.baseline, live.deaths, in_world);
            }
            if (g_app.session_started_ms != 0) {
                live.session_ms = static_cast<uint32_t>(
                    GetTickCount64() - g_app.session_started_ms);
            }
            if (live.in_boss_fight) {
                auto hint = last_boss_from_location(live.location);
                if (hint.empty()) {
                    hint = live.location;
                }
                if (!hint.empty() && hint != "Unknown") {
                    g_app.last_boss_fought = hint;
                }
            }
            live.last_boss = g_app.last_boss_fought;
            live.session_deaths = compute_session_deaths(live.deaths, g_app.baseline.deaths_at_load);
            g_app.current.live.session_deaths = live.session_deaths;
            g_app.current.live.session_ms = live.session_ms;
            g_app.current.live.last_boss = live.last_boss;
        } else {
            live.character_loaded = false;
            g_app.current.live.character_loaded = false;
            session_action = note_character_presence(g_app.session, false);
        }
    }

    hud_update(live);
    if (session_action == SessionTickAction::PostTitleReturn) {
        post_session_if_needed("title menu");
        return;
    }
    if (!live.character_loaded) {
        return;
    }

    static uint32_t last_persisted_deaths = 0xFFFFFFFFu;
    static ULONGLONG last_persist_ms = 0;
    const ULONGLONG now = GetTickCount64();
    const bool death_changed = live.deaths != last_persisted_deaths;
    if (death_changed || now - last_persist_ms >= 3000) {
        persist_current();
        last_persisted_deaths = live.deaths;
        last_persist_ms = now;
    }
}

}  // namespace erstats

namespace {

DWORD WINAPI init_thread(LPVOID) {
    using namespace erstats;
    log_info("StatsShare starting");

    auto config_path = dll_directory() / "EldenRing_StatsShare.toml";
    if (!std::filesystem::exists(config_path)) {
        config_path = dll_directory().parent_path() / "EldenRing_StatsShare.toml";
    }
    app().config = load_config(config_path);
    hud_set_config(app().config);

    std::error_code ec;
    std::filesystem::create_directories(mod_data_dir() / "runs", ec);

    retry_pending();

    if (MH_Initialize() != MH_OK) {
        log_error("MinHook init failed");
        return 1;
    }

    install_overlay();
    install_shutdown_hooks();

    int locate_attempts = 0;
    while (app().running.load()) {
        tick_once();
        if (!app().game.ready() && locate_attempts < 120) {
            ++locate_attempts;
        }
        Sleep(250);
    }
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        erstats::app().running = false;
        stats_share_shutdown();
        erstats::shutdown_overlay();
        MH_Uninitialize();
    }
    return TRUE;
}
