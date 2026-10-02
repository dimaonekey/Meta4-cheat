#include "arms.hpp"
#include "../game/game.hpp"
#include "../game/offsets.hpp"
#include "../ui/cfg_holy_bridge.hpp"
#include "../protect/oxorany.hpp"
#include "../other/vector3.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ARMS — управление позицией viewmodel (рук с оружием) у локального игрока.
//
//  Цепочка: PlayerManager(+0x70) → LocalPlayer → AimAnimController(+0xA0) →
//           viewmodelOffset(+0xE8) Vector3.
//
//  Конфиг: cfg::arms::enabled  + cfg::arms::pos_x / pos_y / pos_z
//  (значения прибавляются к текущей позиции каждый тик — это позволяет
//  "сдвигать" руки относительно их базового положения).
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632ULL;

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static uint64_t get_local_player() {
        uint64_t player_manager = get_player_manager();
        if (!likely_ptr(player_manager)) {
            player_manager = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
        }
        if (!likely_ptr(player_manager)) return 0;
        return rpm<uint64_t>(player_manager + oxorany(OFF_PM_LOCAL_PLAYER));
    }
}

void arms::run() {
    if (!cfg::arms::enabled()) return;

    uint64_t local_player = get_local_player();
    if (!likely_ptr(local_player)) return;

    uint64_t aim_anim_ctrl = rpm<uint64_t>(local_player + oxorany(OFF_PLAYER_AIM_ANIM_CTRL));
    if (!likely_ptr(aim_anim_ctrl)) return;

    Vector3 pos = rpm<Vector3>(aim_anim_ctrl + oxorany(OFF_AAC_VIEWMODEL_OFFSET));

    // Прибавляем настроенные смещения.
    pos.x += cfg::arms::pos_x();
    pos.y += cfg::arms::pos_y();
    pos.z += cfg::arms::pos_z();

    wpm<Vector3>(aim_anim_ctrl + oxorany(OFF_AAC_VIEWMODEL_OFFSET), pos);
}
