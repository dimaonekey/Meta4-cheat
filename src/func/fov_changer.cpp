// fov_changer.cpp
// Перенесено из ковки: FOV 60/90 (fov0..fov10)
// Ковка: search 1 (DWORD), offset chain → write float @ +352
// FOV диапазон: 40..90 (fov0=40, fov4=60, fov10=90)
// В лефффе: пишем через PlayerCamera → CameraSettings → fieldOfView
// Offsets из offsets.hpp: OFF_PLAYER_MAIN_CAMERA(0xE8), OFF_CAM_SETTINGS(0x28), OFF_CAM_FOV(0x38)

#include "fov_changer.hpp"
#include "../ui/cfg.hpp"
#include "../game/game.hpp"
#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"
#include <cmath>

namespace {
    static constexpr uint64_t kOffMainCamera  = 0xE8;
    static constexpr uint64_t kOffCamSettings = 0x28;
    static constexpr uint64_t kOffCamFov      = 0x38;

    static inline bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static bool read_ptr(uint64_t addr, uint64_t& out) {
        out = 0;
        if (!likely_ptr(addr)) return false;
        if (!mem_read(addr, &out, sizeof(out))) return false;
        return likely_ptr(out);
    }
}

void fov_changer::run() {
    if (!cfg::fov_changer::enabled) return;

    uint64_t pm = get_player_manager();
    if (!likely_ptr(pm)) return;
    uint64_t lp = 0;
    if (!read_ptr(pm + oxorany(0x70ULL), lp)) return;

    uint64_t cam_obj = 0;
    if (!read_ptr(lp + oxorany(kOffMainCamera), cam_obj)) return;
    uint64_t cam_settings = 0;
    if (!read_ptr(cam_obj + oxorany(kOffCamSettings), cam_settings)) return;

    float cur = 0.f;
    if (!mem_read(cam_settings + oxorany(kOffCamFov), &cur, sizeof(cur))) return;
    if (!std::isfinite(cur)) return;

    float target = cfg::fov_changer::value;
    // Клампим: ковка даёт 40..90
    if (target < 40.f) target = 40.f;
    if (target > 90.f) target = 90.f;

    // Пишем только если отличается (избегаем лишних write)
    if (std::fabsf(cur - target) > 0.1f) {
        wpm<float>(cam_settings + oxorany(kOffCamFov), target);
    }
}
