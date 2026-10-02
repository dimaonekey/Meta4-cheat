#pragma once

#include "../other/memory.hpp"
#include "../other/vector3.h"
#include "../protect/oxorany.hpp"
#include <stdint.h>

struct matrix {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
};

namespace offsets {
    inline uint64_t player_manager = oxorany(180740496);  // updated from Expodium
    inline uint64_t inventory_manager = oxorany(0x814E7B0);
    
    namespace player {
        inline uint64_t player_character_view = oxorany(0x48);
        inline uint64_t aim_controller = oxorany(0x80);
        inline uint64_t weaponry_controller = oxorany(0x88);
        inline uint64_t movement_controller = oxorany(0x98);
        inline uint64_t occlusion_controller = oxorany(0xB8);  // updated: 0xB0 -> 0xB8
        inline uint64_t main_camera = oxorany(0xE8);           // updated: 0xE0 -> 0xE8
        inline uint64_t team = oxorany(0x79);
        inline uint64_t photon_player = oxorany(0x160);        // updated: 0x158 -> 0x160
    }

    namespace view {
        inline uint64_t biped_map = oxorany(0x48);
    }

    namespace movement {
        inline uint64_t translation_data = oxorany(0xB0);
    }

    namespace aim {
        inline uint64_t aiming_data = oxorany(0x90);
    }

    struct TMatrix {
        Vector4 position;
        Quaternion rotation;
        Vector4 scale;
    };

    namespace transform {
        inline uint64_t internal_transform = oxorany(0x10);
    }
}

template<typename T = uint64_t>
inline T get_static(uint64_t type_offset) noexcept {
    if (proc::lib == 0) return 0;
    uint64_t cls = rpm<uint64_t>(proc::lib + type_offset);
    if (!cls) return 0;
    uint64_t obj = rpm<uint64_t>(cls + oxorany(0x90));   // updated: 0x100 -> 0x60
    if (!obj) return 0;
    uint64_t fields = rpm<uint64_t>(obj + oxorany(0x10)); // updated: 0x130 -> 0x10
    if (!fields) return 0;
    return static_cast<T>(fields);  // direct return как в Expodium
}

inline uint64_t get_player_manager() noexcept {
    return get_static(offsets::player_manager);
}

