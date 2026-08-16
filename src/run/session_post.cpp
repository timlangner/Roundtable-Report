#include "run/session_post.hpp"

namespace erstats {

SessionTickAction note_character_presence(SessionPostState& state, bool character_loaded) {
    if (character_loaded) {
        state.unload_ticks = 0;
        if (!state.session_active) {
            state.session_active = true;
            state.session_posted = false;
            return SessionTickAction::SessionStarted;
        }
        return SessionTickAction::None;
    }

    if (!state.session_active) {
        state.unload_ticks = 0;
        return SessionTickAction::None;
    }

    if (state.unload_ticks < kTitleUnloadConfirmTicks) {
        ++state.unload_ticks;
    }
    if (state.unload_ticks < kTitleUnloadConfirmTicks) {
        return SessionTickAction::None;
    }

    state.session_active = false;
    state.unload_ticks = 0;
    if (state.session_posted) {
        return SessionTickAction::None;
    }
    return SessionTickAction::PostTitleReturn;
}

uint32_t stabilize_death_count(uint32_t live_deaths, uint32_t last_known_deaths) {
    return last_known_deaths > live_deaths ? last_known_deaths : live_deaths;
}

void begin_session_baseline(SessionBaseline& baseline, uint32_t trusted_deaths, bool in_world) {
    baseline.deaths_at_load = trusted_deaths;
    baseline.locked = in_world || trusted_deaths > 0;
}

void refresh_session_baseline(SessionBaseline& baseline, uint32_t trusted_deaths, bool in_world) {
    if (baseline.locked) {
        return;
    }
    if (in_world || trusted_deaths > 0) {
        baseline.deaths_at_load = trusted_deaths;
        baseline.locked = true;
    }
}

}  // namespace erstats
