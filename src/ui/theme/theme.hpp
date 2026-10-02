#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <unordered_map>

extern float g_sw;
extern float g_sh;

namespace ui {

namespace clr {
    inline ImVec4 bg           = ImVec4(10/255.f,  12/255.f,  16/255.f,  1.f);
    inline ImVec4 bg_two       = ImVec4(14/255.f,  17/255.f,  24/255.f,  1.f);
    inline ImVec4 sidebar      = ImVec4(8/255.f,   9/255.f,   13/255.f,  1.f);
    inline ImVec4 panel        = ImVec4(16/255.f,  20/255.f,  28/255.f,  1.f);
    inline ImVec4 widget       = ImVec4(20/255.f,  26/255.f,  38/255.f,  1.f);

    inline ImVec4 accent       = ImVec4(45/255.f,  125/255.f, 245/255.f, 1.f);
    inline ImVec4 accent_light = ImVec4(75/255.f,  155/255.f, 255/255.f, 1.f);
    inline ImVec4 accent_dark  = ImVec4(25/255.f,  85/255.f,  190/255.f, 1.f);

    inline ImVec4 text         = ImVec4(245/255.f, 248/255.f, 255/255.f, 1.f);
    inline ImVec4 text_light   = ImVec4(255/255.f, 255/255.f, 255/255.f, 1.f);
    inline ImVec4 text_dim     = ImVec4(100/255.f, 115/255.f, 140/255.f, 1.f);

    inline ImVec4 border       = ImVec4(45/255.f,  125/255.f, 245/255.f, 0.18f);
    inline ImVec4 border_light = ImVec4(75/255.f,  155/255.f, 255/255.f, 0.35f);
    inline ImVec4 border_dark  = ImVec4(0/255.f,   0/255.f,   0/255.f,   1.f);
    inline ImVec4 subtab_bg    = ImVec4(12/255.f,  15/255.f,  22/255.f,  1.f);
}

namespace style {
    inline std::unordered_map<std::string, float>   anims;
    inline std::unordered_map<std::string, ImVec4>  anim_colors;
    inline float        content_w     = 0.f;
    inline float        content_alpha = 1.f;
    inline bool         popup_open    = false;
    inline std::string  active_popup  = "";
    inline constexpr float S = 2.5f;

    void   tick();
    float  anim(const std::string& id, float tgt, float spd = 12.f);
    ImVec4 anim_col(const std::string& id, const ImVec4& tgt, float spd = 12.f);
    ImU32  col(const ImVec4& c, float a = 1.f);
    bool   popup();
    void   close();
    void   popups();
}

namespace theme {

    inline ImU32 wm_bg() {
        return IM_COL32(10, 13, 18, 235);
    }

    inline ImU32 wm_text() {
        return IM_COL32(235, 242, 255, 240);
    }

    inline ImU32 sep() {
        return IM_COL32(45, 125, 245, 120);
    }

    inline ImU32 logo() {
        return IM_COL32(75, 155, 255, 255);
    }

}

}
