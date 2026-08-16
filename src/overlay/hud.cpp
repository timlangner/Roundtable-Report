#include "overlay/hud.hpp"

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <mutex>

namespace erstats {
namespace {

std::mutex g_mutex;
HudFrame g_frame;
Hotkey g_toggle{};
bool g_toggle_was_down = false;

constexpr ImU32 kInk = IM_COL32(8, 5, 3, 230);
constexpr ImU32 kWash = IM_COL32(10, 7, 5, 196);
constexpr ImU32 kWashFade = IM_COL32(10, 7, 5, 0);
constexpr ImU32 kGold = IM_COL32(232, 213, 163, 255);
constexpr ImU32 kGoldMid = IM_COL32(196, 164, 86, 245);
constexpr ImU32 kGoldDim = IM_COL32(168, 138, 72, 230);
constexpr ImU32 kStave = IM_COL32(210, 176, 96, 235);

bool key_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool hotkey_down(const Hotkey& hotkey) {
    if (hotkey.vk == 0 || !key_down(hotkey.vk)) {
        return false;
    }
    if (hotkey.alt && !key_down(VK_MENU)) {
        return false;
    }
    if (hotkey.ctrl && !key_down(VK_CONTROL)) {
        return false;
    }
    if (hotkey.shift && !key_down(VK_SHIFT)) {
        return false;
    }
    return true;
}

bool is_left_anchor(std::string_view anchor) {
    return anchor == "left_middle" || anchor == "middle_left" || anchor == "top_left"
        || anchor == "bottom_left";
}

ImVec2 anchor_pos(const HudFrame& frame, const ImGuiViewport* viewport) {
    ImVec2 pos = viewport->WorkPos;
    const float inset = 36.0f;
    if (frame.anchor == "left_middle" || frame.anchor == "middle_left") {
        pos.x += inset;
        pos.y += viewport->WorkSize.y * 0.5f;
    } else if (frame.anchor == "right_middle" || frame.anchor == "middle_right") {
        pos.x += viewport->WorkSize.x - inset;
        pos.y += viewport->WorkSize.y * 0.5f;
    } else if (frame.anchor == "top_left") {
        pos.x += inset;
        pos.y += inset;
    } else if (frame.anchor == "top_right") {
        pos.x += viewport->WorkSize.x - inset;
        pos.y += inset;
    } else if (frame.anchor == "bottom_left") {
        pos.x += inset;
        pos.y += viewport->WorkSize.y - inset;
    } else {
        pos.x += viewport->WorkSize.x - inset;
        pos.y += viewport->WorkSize.y - inset;
    }
    return pos;
}

ImVec2 anchor_pivot(const HudFrame& frame) {
    if (frame.anchor == "left_middle" || frame.anchor == "middle_left") {
        return ImVec2(0.0f, 0.5f);
    }
    if (frame.anchor == "right_middle" || frame.anchor == "middle_right") {
        return ImVec2(1.0f, 0.5f);
    }
    const bool left = is_left_anchor(frame.anchor);
    const bool top = frame.anchor == "top_left" || frame.anchor == "top_right";
    return ImVec2(left ? 0.0f : 1.0f, top ? 0.0f : 1.0f);
}

ImFont* label_font() {
    ImGuiIO& io = ImGui::GetIO();
    return io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
}

ImFont* count_font() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.size() > 1) {
        return io.Fonts->Fonts[1];
    }
    return label_font();
}

float spaced_width(ImFont* font, float size, const char* text, float tracking) {
    float width = 0.0f;
    for (const char* p = text; *p != '\0'; ++p) {
        char glyph[2] = {*p, '\0'};
        width += font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph).x;
        if (p[1] != '\0') {
            width += tracking;
        }
    }
    return width;
}

void add_text(
    ImDrawList* draw, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text) {
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            if (x == 0 && y == 0) {
                continue;
            }
            draw->AddText(
                font, size, ImVec2(pos.x + static_cast<float>(x), pos.y + static_cast<float>(y)),
                kInk, text);
        }
    }
    draw->AddText(font, size, ImVec2(pos.x + 1.0f, pos.y + 2.0f), IM_COL32(0, 0, 0, 140), text);
    draw->AddText(font, size, pos, color, text);
}

void add_spaced_text(
    ImDrawList* draw,
    ImFont* font,
    float size,
    ImVec2 pos,
    ImU32 color,
    const char* text,
    float tracking) {
    float x = pos.x;
    for (const char* p = text; *p != '\0'; ++p) {
        char glyph[2] = {*p, '\0'};
        add_text(draw, font, size, ImVec2(x, pos.y), color, glyph);
        x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph).x;
        if (p[1] != '\0') {
            x += tracking;
        }
    }
}

void draw_stave(ImDrawList* draw, float x, float y0, float y1) {
    const ImVec2 tip(x, y0);
    draw->AddTriangleFilled(
        ImVec2(tip.x, tip.y - 5.0f), ImVec2(tip.x - 4.5f, tip.y + 3.0f),
        ImVec2(tip.x + 4.5f, tip.y + 3.0f), kStave);
    draw->AddLine(ImVec2(x, y0 + 4.0f), ImVec2(x, y1 - 4.0f), kStave, 2.0f);
    draw->AddCircleFilled(ImVec2(x, y1), 3.0f, kStave, 8);
}

void draw_plaque(const HudFrame& frame) {
    char deaths[16]{};
    char session[16]{};
    std::snprintf(deaths, sizeof(deaths), "%u", frame.deaths);
    std::snprintf(session, sizeof(session), "+%u", frame.session_deaths);

    const bool show_session = frame.show_session_deaths && frame.session_deaths > 0;
    ImFont* body = label_font();
    ImFont* display = count_font();
    if (body == nullptr || display == nullptr) {
        return;
    }

    const float label_size = 13.0f;
    const float count_size = 34.0f;
    const float session_size = 13.0f;
    const float tracking = 2.4f;
    const char* label = "DEATHS";

    const float label_w = spaced_width(body, label_size, label, tracking);
    const ImVec2 count_ext = display->CalcTextSizeA(count_size, FLT_MAX, 0.0f, deaths);
    const ImVec2 session_ext =
        show_session ? body->CalcTextSizeA(session_size, FLT_MAX, 0.0f, session) : ImVec2();

    const float text_w = (std::max)({label_w, count_ext.x, session_ext.x});
    const float stave_x = 10.0f;
    const float text_x = 24.0f;
    const float pad_top = 12.0f;
    const float pad_bottom = 12.0f;
    const float fade = 28.0f;
    const float width = text_x + text_w + fade;
    float cursor = pad_top;
    const float label_y = cursor;
    cursor += label_size + 4.0f;
    const float count_y = cursor;
    cursor += count_ext.y;
    float session_y = 0.0f;
    if (show_session) {
        cursor += 6.0f;
        session_y = cursor;
        cursor += session_ext.y;
    }
    cursor += pad_bottom;
    const float height = cursor;

    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(
        "##er_stats_deaths",
        nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoBackground);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 max(origin.x + width, origin.y + height);

    draw->AddRectFilledMultiColor(origin, max, kWash, kWashFade, kWashFade, kWash);
    draw_stave(draw, origin.x + stave_x, origin.y + 8.0f, max.y - 8.0f);

    add_spaced_text(
        draw, body, label_size, ImVec2(origin.x + text_x, origin.y + label_y), kGoldDim, label,
        tracking);
    add_text(
        draw, display, count_size, ImVec2(origin.x + text_x, origin.y + count_y), kGold, deaths);
    if (show_session) {
        add_text(
            draw, body, session_size, ImVec2(origin.x + text_x, origin.y + session_y), kGoldMid,
            session);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

}  // namespace

void hud_set_config(const Config& config) {
    std::lock_guard lock(g_mutex);
    g_toggle = config.toggle;
    g_frame.show_session_deaths = config.show_session_deaths;
    g_frame.anchor = config.overlay_anchor;
}

void hud_update(const LiveSnapshot& live) {
    std::lock_guard lock(g_mutex);
    g_frame.character_loaded = live.character_loaded;
    g_frame.deaths = live.deaths;
    g_frame.session_deaths = live.session_deaths;
}

HudFrame hud_snapshot() {
    std::lock_guard lock(g_mutex);
    return g_frame;
}

void hud_poll_toggle() {
    const bool down = hotkey_down(g_toggle);
    if (down && !g_toggle_was_down) {
        std::lock_guard lock(g_mutex);
        g_frame.visible = !g_frame.visible;
    }
    g_toggle_was_down = down;
}

void draw_death_hud() {
    hud_poll_toggle();
    const HudFrame frame = hud_snapshot();
    if (!frame.visible || !frame.character_loaded) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(anchor_pos(frame, viewport), ImGuiCond_Always, anchor_pivot(frame));
    draw_plaque(frame);
}

}  // namespace erstats
