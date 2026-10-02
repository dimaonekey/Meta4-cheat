// bighead.cpp
// Первый подход работал (визуал) — патчим TransformEntry.scale в массиве
// Проблема была что коллайдер не следовал. 
// Решение: патчим TransformEntry.scale И NativeTransform direct position trick.
// На самом деле в Standoff2 коллайдеры головы — отдельные SphereCollider
// на дочерних объектах. Они тоже следуют за parent Transform scale.
// Поэтому патч TransformEntry.scale ДОЛЖЕН работать для хитбокса тоже.
// Проблема предыдущей версии: проверка "если уже нужный scale — пропускаем"
// срабатывала неправильно т.к. base scale != 1.0 у модели.
// Решение: всегда писать целевой scale без проверки.

#include "bighead.hpp"
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <cstdint>
#include <ctime>
#include <cmath>

namespace {
    static inline bool ok(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static uint64_t now_bh() {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec*1000ull + ts.tv_nsec/1000000ull;
    }

    static uint64_t s_last = 0;

    // sizeof(TransformEntry) = 3 * Vector4 = 48 bytes
    // layout: position[16] rotation[16] scale[16]
    // scale starts at +0x20

    static void patch_player_head(uint64_t p, float mul) {
        // biped_map: player → view(+0x48) → map(+0x48)
        uint64_t view = rpm<uint64_t>(p + oxorany(0x48ULL));
        if (!ok(view)) return;
        uint64_t map = rpm<uint64_t>(view + oxorany(0x48ULL));
        if (!ok(map)) return;

        // Head Transform @ map+0x20 (из txt оффсетов: Head @ 0x20)
        uint64_t head_tr = rpm<uint64_t>(map + oxorany(0x20ULL));
        if (!ok(head_tr)) return;

        // NativeTransform @ head_tr+0x10
        uint64_t nt = rpm<uint64_t>(head_tr + oxorany(0x10ULL));
        if (!ok(nt)) return;

        // TransformData @ nt+0x28, index @ nt+0x30
        uint64_t td  = rpm<uint64_t>(nt + oxorany(0x28ULL));
        int      idx = rpm<int>(nt + oxorany(0x30ULL));
        if (!ok(td) || idx < 0 || idx > 100000) return;

        // TransformArray @ td+0x18
        uint64_t arr = rpm<uint64_t>(td + oxorany(0x18ULL));
        if (!ok(arr)) return;

        // TransformEntry.scale @ arr + idx*48 + 0x20 (Vector4: x,y,z,w)
        uint64_t scale_addr = arr + (uint64_t)idx * 48ULL + 0x20ULL;

        // Пишем scale безусловно каждый кадр (80ms)
        wpm<float>(scale_addr + 0x00ULL, mul);
        wpm<float>(scale_addr + 0x04ULL, mul);
        wpm<float>(scale_addr + 0x08ULL, mul);
        // w оставляем 1.0
        wpm<float>(scale_addr + 0x0CULL, 1.f);
    }
}

void bighead::run() {
    if (!cfg::bighead::enabled) return;

    uint64_t now = now_bh();
    if (now - s_last < 80ULL) return;
    s_last = now;

    uint64_t pm = get_player_manager();
    if (!ok(pm)) return;

    uint64_t lp = rpm<uint64_t>(pm + oxorany(0x70ULL));
    uint64_t pl = rpm<uint64_t>(pm + oxorany(0x28ULL));
    if (!ok(pl)) return;

    int cnt = rpm<int>(pl + oxorany(0x20ULL));
    if (cnt <= 0 || cnt > 64) return;

    uint64_t buf = rpm<uint64_t>(pl + oxorany(0x18ULL));
    if (!ok(buf)) return;

    float mul = std::clamp(cfg::bighead::scale, 1.5f, 8.f);

    for (int i = 0; i < cnt; i++) {
        uint64_t p = rpm<uint64_t>(buf + oxorany(0x30ULL) + (uint64_t)i * oxorany(0x18ULL));
        if (!ok(p) || p == lp) continue;
        if (player::health(p) <= 0) continue;
        patch_player_head(p, mul);
    }
}
