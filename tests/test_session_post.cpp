#include "run/session_post.hpp"
#include "run/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

using erstats::begin_session_baseline;
using erstats::compute_session_deaths;
using erstats::kTitleUnloadConfirmTicks;
using erstats::note_character_presence;
using erstats::refresh_session_baseline;
using erstats::SessionBaseline;
using erstats::SessionPostState;
using erstats::SessionTickAction;
using erstats::stabilize_death_count;

TEST_CASE("title return posts after confirmed unload") {
    SessionPostState state;
    REQUIRE(note_character_presence(state, true) == SessionTickAction::SessionStarted);
    REQUIRE(state.session_active);
    REQUIRE_FALSE(state.session_posted);

    REQUIRE(note_character_presence(state, true) == SessionTickAction::None);

    for (int i = 0; i < kTitleUnloadConfirmTicks - 1; ++i) {
        REQUIRE(note_character_presence(state, false) == SessionTickAction::None);
        REQUIRE(state.session_active);
    }
    REQUIRE(note_character_presence(state, false) == SessionTickAction::PostTitleReturn);
    REQUIRE_FALSE(state.session_active);
}

TEST_CASE("title then quit without a new load does not post again") {
    SessionPostState state;
    REQUIRE(note_character_presence(state, true) == SessionTickAction::SessionStarted);

    for (int i = 0; i < kTitleUnloadConfirmTicks - 1; ++i) {
        note_character_presence(state, false);
    }
    REQUIRE(note_character_presence(state, false) == SessionTickAction::PostTitleReturn);
    state.session_posted = true;

    REQUIRE(note_character_presence(state, false) == SessionTickAction::None);
    REQUIRE_FALSE(state.session_active);
    REQUIRE(state.session_posted);
}

TEST_CASE("loading another profile after title starts a new postable session") {
    SessionPostState state;
    note_character_presence(state, true);
    for (int i = 0; i < kTitleUnloadConfirmTicks; ++i) {
        note_character_presence(state, false);
    }
    state.session_posted = true;

    REQUIRE(note_character_presence(state, true) == SessionTickAction::SessionStarted);
    REQUIRE(state.session_active);
    REQUIRE_FALSE(state.session_posted);

    for (int i = 0; i < kTitleUnloadConfirmTicks - 1; ++i) {
        REQUIRE(note_character_presence(state, false) == SessionTickAction::None);
    }
    REQUIRE(note_character_presence(state, false) == SessionTickAction::PostTitleReturn);
}

TEST_CASE("brief unload during play does not post") {
    SessionPostState state;
    note_character_presence(state, true);
    REQUIRE(note_character_presence(state, false) == SessionTickAction::None);
    REQUIRE(note_character_presence(state, true) == SessionTickAction::None);
    REQUIRE(state.session_active);
    REQUIRE_FALSE(state.session_posted);
}

TEST_CASE("never loaded stays silent on title") {
    SessionPostState state;
    REQUIRE(note_character_presence(state, false) == SessionTickAction::None);
    REQUIRE(note_character_presence(state, false) == SessionTickAction::None);
    REQUIRE_FALSE(state.session_active);
}

TEST_CASE("exit post during title debounce suppresses the later title post") {
    SessionPostState state;
    note_character_presence(state, true);
    note_character_presence(state, false);
    state.session_posted = true;

    for (int i = 0; i < kTitleUnloadConfirmTicks; ++i) {
        REQUIRE(note_character_presence(state, false) != SessionTickAction::PostTitleReturn);
    }
    REQUIRE_FALSE(state.session_active);
}

TEST_CASE("title reload does not treat lifetime deaths as session deaths") {
    SessionBaseline baseline;
    const uint32_t last_known = 1784;
    uint32_t live = stabilize_death_count(0, last_known);
    REQUIRE(live == 1784);
    begin_session_baseline(baseline, live, false);
    REQUIRE(compute_session_deaths(live, baseline.deaths_at_load) == 0);

    live = stabilize_death_count(1784, live);
    refresh_session_baseline(baseline, live, true);
    REQUIRE(compute_session_deaths(live, baseline.deaths_at_load) == 0);
}

TEST_CASE("zero death read before the world loads is not the session baseline") {
    SessionBaseline baseline;
    uint32_t live = stabilize_death_count(0, 0);
    begin_session_baseline(baseline, live, false);
    REQUIRE_FALSE(baseline.locked);

    live = stabilize_death_count(1784, 0);
    refresh_session_baseline(baseline, live, true);
    REQUIRE(baseline.deaths_at_load == 1784);
    REQUIRE(compute_session_deaths(1784, baseline.deaths_at_load) == 0);
}

TEST_CASE("new character with zero deaths then a death counts as session") {
    SessionBaseline baseline;
    begin_session_baseline(baseline, 0, true);
    REQUIRE(baseline.locked);
    REQUIRE(baseline.deaths_at_load == 0);
    refresh_session_baseline(baseline, 1, true);
    REQUIRE(compute_session_deaths(1, baseline.deaths_at_load) == 1);
}
