// ─────────────────────────────────────────────────────────────────────────────
//  MOVEMENT HACKS — оптимизированная версия
//  Ключевое изменение: get_mov_ctrl() вызывается ОДИН раз за тик через
//  общий кэш movement_cache::get(), все 6 хаков читают одно и то же значение.
// ─────────────────────────────────────────────────────────────────────────────

#include "dmovement.hpp"
#include "../game/game.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <stdint.h>

namespace {

static constexpr uint64_t kOffMovCtrl       = 0x98;
static constexpr uint64_t kOffTrajPredictor = 0xA8;
static constexpr uint64_t kOffJumpParams    = 0x50;
static constexpr uint64_t kOffJumpSpeed     = 0x10;
static constexpr uint64_t kOffJumpSpeed2    = 0x60;
static constexpr uint64_t kOffThrustData    = 0xB0;
static constexpr uint64_t kOffThrustVec     = 0x68;
static constexpr uint64_t kOffTranslationA  = 0x1C;
static constexpr uint64_t kOffWeaponryState = 0x88;

static constexpr uint32_t kAirJumpVal   = 5u;
static constexpr uint32_t kStrafeVal    = 1232348144u;
static constexpr uint32_t kBnnyValDword = 1081674666u;

static constexpr uint64_t kOffMoveDirectionSpeed    = 0x2C;
static constexpr uint64_t kOffMecaninDirectionSpeed = 0x30;

static inline bool likely_ptr(uint64_t p) {
    return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
}

static inline bool read_ptr(uint64_t addr, uint64_t& out) {
    out = 0;
    if (!likely_ptr(addr)) return false;
    if (!mem_read(addr, &out, sizeof(out))) return false;
    return likely_ptr(out);
}

// ── Кэш MovCtrl за кадр ──────────────────────────────────────────────────
// Все хаки движения вызываются последовательно в одном тике.
// Вместо 6 × (rpm x3) = 18 rpm-вызовов делаем 3 rpm один раз.
struct MovCache {
    uint64_t mc  = 0;  // MovementController
    uint64_t tp  = 0;  // TranslationParams (TrajPredictor)
    uint64_t jp  = 0;  // JumpParams
    uint64_t td  = 0;  // ThrustData
    bool     valid = false;
};

static MovCache s_mov = {};

static bool refresh_mov_cache() {
    s_mov = {};
    uint64_t pm = get_player_manager();
    if (!likely_ptr(pm)) return false;
    uint64_t lp = 0;
    if (!read_ptr(pm + oxorany(0x70ULL), lp)) return false;
    if (!read_ptr(lp + oxorany(kOffMovCtrl), s_mov.mc)) return false;

    read_ptr(s_mov.mc + oxorany(kOffTrajPredictor), s_mov.tp);
    if (s_mov.tp) read_ptr(s_mov.tp + oxorany(kOffJumpParams), s_mov.jp);
    read_ptr(s_mov.mc + oxorany(kOffThrustData), s_mov.td);

    s_mov.valid = true;
    return true;
}

} // namespace

// ── Публичный API: вызывается из main.cpp ПЕРЕД всеми хаками движения ────
void movement_cache_refresh() {
    refresh_mov_cache();
}

void air_jump::run() {
    if (!cfg::air_jump::enabled) return;
    if (!s_mov.valid || !s_mov.jp) return;
    wpm<uint32_t>(s_mov.jp + oxorany(kOffJumpSpeed), kAirJumpVal);
}

void strafe::run() {
    if (!cfg::strafe::enabled) return;
    if (!s_mov.valid || !s_mov.td) return;
    wpm<uint32_t>(s_mov.td + oxorany(kOffThrustVec),      kStrafeVal);
    wpm<uint32_t>(s_mov.td + oxorany(kOffThrustVec + 4),  0u);
    wpm<uint32_t>(s_mov.td + oxorany(kOffThrustVec + 8),  0u);
}

void bunny_hop::run() {
    if (!cfg::bunny_hop::enabled) return;
    if (!s_mov.valid || !s_mov.jp) return;
    wpm<uint32_t>(s_mov.jp + oxorany(kOffJumpSpeed),  kBnnyValDword);
    wpm<uint32_t>(s_mov.jp + oxorany(kOffJumpSpeed2), kBnnyValDword);
}

void crouch_speed::run() {
    if (!cfg::crouch_speed::enabled) return;
    if (!s_mov.valid || !s_mov.tp) return;
    uint64_t crouch_params = 0;
    if (!read_ptr(s_mov.tp + oxorany(0x48ULL), crouch_params)) return;
    // Из f_crouchspeed.hpp: crouchSpeedMultiplier=1.0, crouchSpeed=5.0
    wpm<float>(crouch_params + oxorany(0x10ULL), 1.0f);   // crouchSpeedMultiplier
    wpm<float>(crouch_params + oxorany(0x14ULL), 5.0f);   // crouchSpeed
}

void fast_walk::run() {
    if (!cfg::fast_walk::enabled) return;
    if (!s_mov.valid || !s_mov.tp) return;
    wpm<float>(s_mov.tp + oxorany(kOffMoveDirectionSpeed),    9999.0f);
    wpm<float>(s_mov.tp + oxorany(kOffMecaninDirectionSpeed), 9999.0f);
}

void air_strafe::run() {
    if (!cfg::air_strafe::enabled) return;
    if (!s_mov.valid) return;
    if (s_mov.jp)
        wpm<float>(s_mov.jp + oxorany(kOffJumpSpeed2), 2.0f);
    if (s_mov.td) {
        wpm<float>(s_mov.td + oxorany(kOffTranslationA),       0.f);
        wpm<float>(s_mov.td + oxorany(kOffTranslationA + 0x4), 0.f);
        wpm<float>(s_mov.td + oxorany(kOffTranslationA + 0x8), 0.f);
    }
}

void high_jump::run() {
    if (!cfg::high_jump::enabled) return;
    if (!s_mov.valid || !s_mov.jp) return;
    // Lua писала float-значение height (1–50) по тому же JumpParams + 0x10.
    // air_jump пишет uint32_t = 5, high_jump перекрывает его float'ом.
    wpm<float>(s_mov.jp + oxorany(kOffJumpSpeed),  cfg::high_jump::height);
    wpm<float>(s_mov.jp + oxorany(kOffJumpSpeed2), cfg::high_jump::height);
}

void teleport_hack::run(uint64_t weaponry) {
    if (!cfg::movement::teleport::enabled) return;
    if (!likely_ptr(weaponry)) return;

    static bool    was_active = false;
    static uint8_t orig_state = 0;

    if (cfg::movement::teleport::active) {
        uint8_t st = rpm<uint8_t>(weaponry + oxorany(kOffWeaponryState));
        if (!was_active) { orig_state = st; was_active = true; }
        uint8_t v = 10;
        wpm<uint8_t>(weaponry + oxorany(kOffWeaponryState), v);
    } else if (was_active) {
        wpm<uint8_t>(weaponry + oxorany(kOffWeaponryState), orig_state);
        was_active = false;
    }
}
