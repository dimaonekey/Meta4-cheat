#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "color_t.hpp"

#include <string>
#include <vector>
#include <unordered_map>

using namespace ImGui;

inline float gui_sc = 1.f;

class c_gui {

public:

    float m_anim = 0.f;
    int m_tab = 0;

    int m_rage_subtab = 0;
    std::vector<const char*> rage_subtabs = { "General", "Anti aim", "Subtab" };

    color_t accent_color = { 0.7f, 0.7f, 0.7f, 1.f };

    color_t text = { 1.f, 1.f, 1.f, 1.f };
    color_t text_disabled = { 0.51f, 0.52f, 0.56f, 1.f };

    color_t border = { 1.f, 1.f, 1.f, 0.03f };

    color_t frame_inactive = { 0.044f, 0.044f, 0.044f, 1.f };
    color_t frame_active = { 0.083f, 0.083f, 0.083f, 1.f };

    color_t button = { 0.041f, 0.041f, 0.041f, 1.f };
    color_t button_hovered = { 0.061f, 0.061f, 0.061f, 1.f };
    color_t button_active = { 0.081f, 0.081f, 0.081f, 1.f };

    color_t group_box_bg = { 0.039f, 0.039f, 0.039f, 1.f };

    void render_circle_for_horizontal_bar(ImVec2 pos, ImColor color, float alpha);

    inline void group_title(const char* name) {

        SetCursorPosX(GetCursorPosX() + 10.f * gui_sc);
        PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 0.5f));
        Text("%s", name);
        PopStyleColor();
    }

    void group_box(const char* name, ImVec2 size_arg);
    void end_group_box();

    bool tab(const char* icon, const char* label, bool selected);
    bool subtab(const char* label, bool selected, int size, ImDrawFlags flags);

};

inline c_gui gui;
