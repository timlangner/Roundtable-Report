#pragma once

#include <cstdint>

namespace erstats {

// tick_once sleeps 250ms; 8 ticks is about 2s of consecutive unload.
inline constexpr int kTitleUnloadConfirmTicks = 8;

struct SessionPostState {
    bool session_active = false;
    bool session_posted = false;
    int unload_ticks = 0;
};

enum class SessionTickAction {
    None,
    SessionStarted,
    PostTitleReturn,
};

SessionTickAction note_character_presence(SessionPostState& state, bool character_loaded);

struct SessionBaseline {
    uint32_t deaths_at_load = 0;
    bool locked = false;
};

uint32_t stabilize_death_count(uint32_t live_deaths, uint32_t last_known_deaths);
void begin_session_baseline(SessionBaseline& baseline, uint32_t trusted_deaths, bool in_world);
void refresh_session_baseline(SessionBaseline& baseline, uint32_t trusted_deaths, bool in_world);

}  // namespace erstats
