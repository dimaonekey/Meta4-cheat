#pragma once
#include "imgui.h"

namespace cfg {
    namespace esp {
        inline bool box = false;
        inline ImVec4 box_col = ImVec4(1.f, 1.f, 1.f, 1.f);
        inline int box_type = 0;
        inline float box_rounding = 0.f;

        inline bool name = false;
        inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);

        inline bool health = false;
        inline ImVec4 health_col = ImVec4(1.f, 1.f, 1.f, 1.f);

        inline bool distance = false;
        inline ImVec4 distance_col = ImVec4(1.f, 1.f, 1.f, 1.f); // ← добавлено
        
        inline bool device_text = false;
        inline ImVec4 device_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        inline ImVec4 device_textgradient_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        // Новые поля ESP
        inline bool fill = false;
        inline ImVec4 fill_col = ImVec4(1.f, 1.f, 1.f, 1.f);

        inline bool box3d = false;
        inline ImVec4 box3d_col = ImVec4(1.f, 1.f, 1.f, 1.f);

        inline bool fill3d = false;
        inline ImVec4 fill3d_col = ImVec4(1.f, 1.f, 1.f, 1.f);

        inline bool line = false;
        inline ImVec4 line_col = ImVec4(1.f, 1.f, 1.f, 1.f);
        
        inline bool armor = false;
        inline ImVec4 armor_col = ImVec4(1.f, 1.f, 1.f, 1.f);
        
        inline ImVec4 fill3d_base_col = ImVec4(1.f, 1.f, 1.f, 1.f);
        
        inline bool ping = false;
        inline ImVec4 ping_col = ImVec4 (1.f, 1.f, 1.f, 1.f);
        inline ImVec4 pinggradient_col = ImVec4 (1.f, 1.f, 1.f, 1.f);
        
        inline bool esp_arrows = false;
        inline bool esp_arrows_rgb = false;
        inline float esp_arrows_rgb_speed = 1.0f;
        inline ImVec4 esp_arrows_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        inline float esp_arrows_size = 18.0f;      // размер стрелок
        inline float esp_arrows_distance = 120.0f; // радиус от прицела
        inline float esp_arrows_glow = 5.0f;
        inline bool esp_arrows_in_animated = false;
        inline bool esp_arrows_show_distance = true;
        
        inline bool weapon = false;
        inline ImVec4 weapon_col = ImVec4 (1.f, 1.f, 1.f, 1.f);
        inline ImVec4 weapongradient_col = ImVec4 (1.f, 1.f, 1.f, 1.f);
               
        inline int fill3d_type = 0; // 0 base, 1 glass
        // Gradient colors (second color for gradient)
inline ImVec4 boxgradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 fillgradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 box3dgradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 fill3dgradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 armorgradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 namegradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 distancegradient_col = { 1.f, 1.f, 1.f, 1.f };
inline ImVec4 healthgradient_col = { 1.f, 1.f, 1.f, 1.f };

inline bool trail = false;
inline bool trail_enemy = true;
inline bool trail_self = false;

inline float trail_dist = 10.f;   // 1–10
inline float trail_thick = 2.5f;  // 1–10

inline ImVec4 trail_col = {1.f, 1.f, 1.f, 1.f};
inline ImVec4 trail_colgradient = {1.f, 1.f, 1.f, 1.f};

inline bool death = false;

inline float death_time = 0.5f;

inline float death_count = 25.f;

inline ImVec4 death_col = {
    1.f,
    1.f,
    1.f,
    1.f
};

inline int death_type = 0;

    inline bool snow = false;
    inline ImVec4 snow_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    
    inline bool rain = false;
    inline ImVec4 rain_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline bool marker = false;

inline float marker_time = 1.0f;

inline ImVec4 marker_col = {
    1.f,
    1.f,
    1.f,
    1.f
};
inline bool   skeleton_rgb       = false;
    inline float  skeleton_thickness = 1.2f;
    inline bool   skeleton        = false;
    inline ImVec4 skeleton_col       = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 skeleton_gradient_col       = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline bool head_circle = false;
        inline bool china = false;

        inline ImVec4 china1 = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        inline ImVec4 china2 = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    namespace aim {
    inline bool enabled = false;
    inline bool visible_check = false;
    inline bool triggerbot = false;
    inline float trigger_delay = 0.05f;
    inline float trigger_range = 1000.0f;
    inline bool trigger_visible_only = true;
    inline int trigger_bone_mask = 3;
    inline bool show_fov = false;
    inline float fov = 90.f;
    inline float fov_thickness = 1.0f;
    inline ImVec4 fov_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline float smooth = 5.f;
    inline int bone = 0;
    inline bool   fov_draw     = false;
    inline ImVec4 fov_color    = { 1.f, 1.f, 1.f, 1.f }; // RGBA 0..1
    inline int    fov_segments = 64;

    // -- NEW: fov check toggle --
    inline bool   fov_check    = false;
    // true  → стандарт: цель только в экранном фрустуме, px-дистанция
    // false → back-hemisphere: угловой тест, работает на 360°, спиной тоже

    // -- NEW: back camera --
    inline bool   back_camera       = false; // биндишь на кнопку
    inline float  back_camera_speed = 0.f;
}
    
namespace wallshot {
    inline bool enabled = false;
    inline int value = 2147483646;
    inline int restore_value = 150;
}

    namespace fast_walk {
        inline bool enabled = false;
    }
    
    namespace air_strafe {
        inline bool enabled = false;
    }
    
    namespace movement {
    namespace teleport {
        inline bool enabled = false;
        inline bool active  = false;
    }
    }

namespace inf_ammo {
    inline bool enabled = false;
    inline int value = 999999;
}

namespace norecoil {
    inline bool enabled = false;
    inline float multiplier = 0.0f;
}

namespace test {
    inline bool invisible = false;
}

namespace fov_changer {
    inline bool  enabled = false;
    inline float value   = 60.f;  // 60 = fov4 из ковки
}

namespace strafe {
    inline bool enabled = false;
}

namespace bunny_hop {
    inline bool enabled = false;
}

namespace air_jump {
    inline bool enabled = false;
}

namespace chams {
    inline bool   enabled      = false;
    inline int    type         = 0;          // 0=flat 1=crystal 2=outline 3=outline+fill
    inline float  transparency = 1.f;
    inline float  outline_w    = 2.f;        // px, types 2 & 3
    inline float  fill_alpha   = 0.28f;      // fill opacity for type 3
    inline float glow_scale = 1.0f; // [0.5, 3.0]
    inline ImVec4 col          = ImVec4(1.f, 1.f, 1.f,  1.f); // top/solid colour
    inline ImVec4 col_gradient = ImVec4(1.f, 1.f, 1.f, 1.f); // bottom colour
}

// ─── НОВЫЕ ФИЧИ ────────────────────────────────────────────────────────────

namespace chams2 {
inline bool enabled = false;
}

namespace matchams {
    inline bool enabled    = false;
    inline bool team_check = true;
}

namespace misc {
    inline bool autowin       = false;
}

namespace world {
    inline bool   collider_enabled  = false;
    inline int    collider_map_id   = 0;
    inline ImVec4 collider_color    = ImVec4(1.f, 1.f, 0.f, 1.f);
    inline float  collider_alpha    = 0.6f;
    inline float  collider_max_dist = 80.f;
}

namespace fustknife {
    inline bool enabled = false;
}

namespace inf_shop {
    inline bool enabled = false;
    inline int  value   = 30000;
}

namespace crouch_speed {
    inline bool enabled = false;
}

namespace sigma {
    inline bool enabled = false;
    inline int  damage  = 130;   // дефолт из исходного снипа
}

// ─── ARMS (viewmodel position offset) ──────────────────────────────────────
namespace arms {
    inline bool  enabled = false;
    inline float pos_x   = 0.0f;
    inline float pos_y   = 0.0f;
    inline float pos_z   = 0.0f;
}

// ─── FIRE RATE (zero fireDuration each tick) ───────────────────────────────
namespace visuals {
    inline int esp_font = 0;           // 0 - Standart (esp.h), 1 - Comfortaa, 2 - Comicsans, 3 - Minecraft, 4 - Pixeloperator, 5 - Verdana
    inline float esp_font_size = 18.0f; // размер шрифта
    inline bool esp_font_outline = true; // обводка текста
    inline float esp_font_outline_thickness = 1.0f; // толщина обводки
}

namespace bighead {
    inline bool enabled = false;
    inline float scale = 1.5f;
}

namespace fire_rate {
    inline bool enabled = false;
}

namespace high_jump {
    inline bool  enabled = false;
    inline float height   = 5.0f;   // диапазон 1.0 – 50.0 (из lua: [1;50])
}

}