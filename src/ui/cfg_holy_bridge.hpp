#pragma once
// cfg_holy_bridge.hpp
// Мост: holy-функции используют cfg:: namespace.
// Вместо inline reference (ODR violation) — inline accessor functions.

#include "cfg_holy.hpp"
#include "imgui.h"

namespace cfg {

namespace wallshot {
    static inline bool&  enabled()       { return cfg_holy::wallshot::enabled; }
    static inline int&   value()         { return cfg_holy::wallshot::value; }
    static inline int&   restore_value() { return cfg_holy::wallshot::restore_value; }
}

namespace inf_ammo {
    static inline bool& enabled() { return cfg_holy::inf_ammo::enabled; }
    static inline int&  value()   { return cfg_holy::inf_ammo::value; }
}

namespace norecoil {
    static inline bool&  enabled()    { return cfg_holy::norecoil::enabled; }
    static inline float& multiplier() { return cfg_holy::norecoil::multiplier; }
}

namespace test {
    static inline bool& invisible() { return cfg_holy::test_h::invisible; }
}

namespace air_jump {
    static inline bool& enabled() { return cfg_holy::air_jump::enabled; }
}

namespace strafe {
    static inline bool& enabled() { return cfg_holy::strafe::enabled; }
}

namespace bunny_hop {
    static inline bool& enabled() { return cfg_holy::bunny_hop::enabled; }
}

namespace crouch_speed {
    static inline bool& enabled() { return cfg_holy::crouch_speed::enabled; }
}

namespace anti_flash {
    static inline bool& enabled() { return cfg_holy::anti_flash::enabled; }
}

namespace anti_smoke {
    static inline bool& enabled() { return cfg_holy::anti_smoke::enabled; }
}

namespace anti_molotov {
    static inline bool& enabled() { return cfg_holy::anti_molotov::enabled; }
}

namespace fov_changer {
    static inline bool&  enabled() { return cfg_holy::fov_changer::enabled; }
    static inline float& value()   { return cfg_holy::fov_changer::value; }
}

namespace sky_color {
    static inline bool&   enabled() { return cfg_holy::sky_color::enabled; }
    static inline ImVec4& color()   { return cfg_holy::sky_color::color; }
}

namespace chams {
    static inline bool& enabled() { return cfg_holy::chams::enabled; }
}

namespace fustknife {
    static inline bool& enabled() { return cfg_holy::fustknife::enabled; }
}

namespace inf_shop {
    static inline bool& enabled() { return cfg_holy::inf_shop::enabled; }
    static inline int&  value()   { return cfg_holy::inf_shop::value; }
}

namespace sigma {
    static inline bool& enabled() { return cfg_holy::sigma::enabled; }
    static inline int&  damage()  { return cfg_holy::sigma::damage; }
}

namespace arms {
    static inline bool&  enabled() { return cfg_holy::arms::enabled; }
    static inline float& pos_x()   { return cfg_holy::arms::pos_x;   }
    static inline float& pos_y()   { return cfg_holy::arms::pos_y;   }
    static inline float& pos_z()   { return cfg_holy::arms::pos_z;   }
}

namespace fire_rate {
    static inline bool& enabled() { return cfg_holy::fire_rate::enabled; }
}

} // namespace cfg
