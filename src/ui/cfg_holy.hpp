#pragma once
// cfg_holy.hpp — конфиги для функций из holycheatlo
// отдельно от основного cfg.hpp чтобы не конфликтовать

#include "imgui.h"

namespace cfg_holy {

namespace wallshot {
    inline bool  enabled       = false;
    inline int   value         = 2147483646;
    inline int   restore_value = 150;
}

namespace inf_ammo {
    inline bool  enabled = false;
    inline int   value   = 10000;
}

namespace norecoil {
    inline bool  enabled    = false;
    inline float multiplier = 0.0f;
}

namespace test_h {
    inline bool  invisible = false;
}

// ─── НОВЫЕ ФИЧИ (перенесены из ковки) ───────────────────────────────────────

namespace air_jump {
    inline bool enabled = false;
}

namespace strafe {
    inline bool enabled = false;
}

namespace bunny_hop {
    inline bool enabled = false;
}

namespace crouch_speed {
    inline bool enabled = false;
}

namespace anti_flash {
    inline bool enabled = false;
}

namespace anti_smoke {
    inline bool enabled = false;
}

namespace anti_molotov {
    inline bool enabled = false;
}

namespace fov_changer {
    inline bool  enabled = false;
    inline float value   = 60.f;  // 60 = fov4 из ковки
}

namespace sky_color {
    inline bool   enabled = false;
    inline ImVec4 color   = ImVec4(0.05f, 0.05f, 0.10f, 1.0f);
}

namespace chams {
    inline bool enabled = false;
}

// ─── НОВЫЕ ФИЧИ ────────────────────────────────────────────────────────────

namespace fustknife {
    inline bool enabled = false;
}

namespace inf_shop {
    inline bool enabled = false;
    inline int  value   = 30000;
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
namespace fire_rate {
    inline bool enabled = false;
}

} // namespace cfg_holy
