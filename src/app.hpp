#pragma once

#include "config.hpp"
#include "game/gamedata.hpp"
#include "run/session_post.hpp"
#include "run/snapshot.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace erstats {

struct AppState {
    Config config;
    GameDataReader game;
    RunSnapshot current;
    SessionPostState session;
    SessionBaseline baseline;
    uint64_t session_started_ms = 0;
    std::string last_boss_fought;
    std::string loaded_run_key;
    std::mutex mutex;
    std::atomic<bool> running{true};
    std::atomic<bool> flushed{false};
};

AppState& app();
void persist_current();
void tick_once();

}  // namespace erstats
