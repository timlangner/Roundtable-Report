#pragma once

#include "config.hpp"
#include "run/snapshot.hpp"

#include <string>

namespace erstats {

struct HudFrame {
    bool visible = true;
    bool character_loaded = false;
    bool show_session_deaths = true;
    std::string anchor = "left_middle";
    uint32_t deaths = 0;
    uint32_t session_deaths = 0;
};

void hud_set_config(const Config& config);
void hud_update(const LiveSnapshot& live);
HudFrame hud_snapshot();
void hud_poll_toggle();
void draw_death_hud();

}  // namespace erstats
