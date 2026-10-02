// ─────────────────────────────────────────────────────────────────────────────
//  WEAPON / RAGE HACKS — порт из NexuraWare в SDK neverlose2.
//  wallshot / norecoil / inf_ammo / inf_shop / fustknife / sigma / fire_rate
//  Все цепочки: PlayerManager(+0x70) → LocalPlayer → Weaponry(0x88) →
//  CurrentWeapon(0xA0) → WeaponParameters(0xA8).
//  Работают через rpm/wpm (external, process_vm_readv/writev).
// ─────────────────────────────────────────────────────────────────────────────

#include "weapon.hpp"
#include "../game/game.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <algorithm>
#include <vector>
#include <cmath>

namespace {

// ── Общие оффсеты цепочки оружия ────────────────────────────────────────
static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632ULL;
static constexpr uint64_t kOffPlayerManagerLocalPlayer  = 0x70;
static constexpr uint64_t kOffPlayerWeaponryController  = 0x88;
static constexpr uint64_t kOffWeaponryCurrentWeapon     = 0xA0;
static constexpr uint64_t kOffWeaponWeaponParameters    = 0xA8;
static constexpr uint64_t kOffWeaponParametersAmmunition = 0x130;
static constexpr uint64_t kOffWeaponParametersId        = 0x18;

struct SafeIntMem { int salt; int value; };
struct NullableSafeIntMem { uint8_t has_value; uint8_t pad[3]; SafeIntMem value; };
struct SafeFloatInline { int salt; float value; };

static inline bool likely_ptr(uint64_t p) {
    return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
}

static inline bool read_ptr(uint64_t addr, uint64_t& out) {
    out = 0;
    if (!likely_ptr(addr)) return false;
    if (!mem_read(addr, &out, sizeof(out))) return false;
    return likely_ptr(out);
}

static uint64_t get_player_manager_safe() {
    uint64_t pm = get_player_manager();
    if (!likely_ptr(pm)) pm = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
    return pm;
}

static uint64_t get_local_player() {
    uint64_t pm = get_player_manager_safe();
    if (!likely_ptr(pm)) return 0;
    return rpm<uint64_t>(pm + oxorany(kOffPlayerManagerLocalPlayer));
}

static uint64_t get_weapon_controller(uint64_t lp) {
    if (!likely_ptr(lp)) return 0;
    uint64_t weaponry = rpm<uint64_t>(lp + oxorany(kOffPlayerWeaponryController));
    if (!likely_ptr(weaponry)) return 0;
    return rpm<uint64_t>(weaponry + oxorany(kOffWeaponryCurrentWeapon));
}

static uint64_t get_weapon_params(uint64_t wc) {
    if (!likely_ptr(wc)) return 0;
    return rpm<uint64_t>(wc + oxorany(kOffWeaponWeaponParameters));
}

// Проверка «это ствол» по полю Ammunition (mag/cap 0..30000)
static bool weapon_is_probably_gun(uint64_t weapon_params) {
    if (!likely_ptr(weapon_params)) return false;
    uint64_t ammo = rpm<uint64_t>(weapon_params + oxorany(kOffWeaponParametersAmmunition));
    if (!likely_ptr(ammo)) return false;
    short mag = 0, cap = 0;
    if (!mem_read(ammo + oxorany(0x10), &mag, sizeof(mag))) return false;
    if (!mem_read(ammo + oxorany(0x12), &cap, sizeof(cap))) return false;
    return mag >= 0 && mag <= 30000 && cap >= 0 && cap <= 30000;
}

static bool write_nullable_safe_int(uint64_t addr, int value) {
    if (!likely_ptr(addr)) return false;
    NullableSafeIntMem v{};
    if (!mem_read(addr, &v, sizeof(v))) return false;
    v.has_value = 1;
    v.value.salt = 0;
    v.value.value = value;
    return mem_write(addr, &v, sizeof(v));
}

static bool write_safe_float_inline_value(uint64_t addr, float value) {
    SafeFloatInline s{};
    if (!likely_ptr(addr)) return false;
    if (!mem_read(addr, &s, sizeof(s))) return false;
    if (!std::isfinite(s.value) || s.value < -1000.f || s.value > 1000.f) return false;
    s.value = value;
    return mem_write(addr, &s, sizeof(s));
}

static bool disable_nullable_safe(uint64_t addr) {
    if (!likely_ptr(addr)) return false;
    return wpm<uint8_t>(addr, static_cast<uint8_t>(0));
}

static void add_unique(std::vector<uint64_t>& out, uint64_t p) {
    if (!likely_ptr(p)) return;
    for (uint64_t v : out) if (v == p) return;
    out.push_back(p);
}

static std::vector<uint64_t> collect_weapon_params() {
    std::vector<uint64_t> out;
    uint64_t local = get_local_player();
    if (!likely_ptr(local)) return out;

    uint64_t weaponry = rpm<uint64_t>(local + oxorany(kOffPlayerWeaponryController));
    if (!likely_ptr(weaponry)) return out;

    uint64_t wc = rpm<uint64_t>(weaponry + oxorany(kOffWeaponryCurrentWeapon));
    if (!likely_ptr(wc)) return out;

    uint64_t params = rpm<uint64_t>(wc + oxorany(kOffWeaponWeaponParameters));
    if (likely_ptr(params)) add_unique(out, params);
    return out;
}

// ═════════════════════════════════════════════════════════════════════════
//  WALLSHOT — пробитие (GunParameters.penetrationPower)
// ═════════════════════════════════════════════════════════════════════════
static constexpr uint64_t kOffGunParametersPenetrationPower     = 0x1A4;
static constexpr uint64_t kOffGunParametersPenetrationPowerSafe = 0x264;

void wallshot_run_impl() {
    static bool prev = false;
    bool enabled = cfg::wallshot::enabled;
    if (!enabled && !prev) return;

    int value = enabled
        ? std::clamp(cfg::wallshot::value, 150, 2147483646)
        : cfg::wallshot::restore_value;

    auto params = collect_weapon_params();
    for (uint64_t p : params) {
        wpm<int>(p + oxorany(kOffGunParametersPenetrationPower), value);
        write_nullable_safe_int(p + oxorany(kOffGunParametersPenetrationPowerSafe), value);
    }
    prev = enabled;
}

// ═════════════════════════════════════════════════════════════════════════
//  INF AMMO / INF SHOP — патроны (Ammunition mag/cap + SafeInt зеркала)
// ═════════════════════════════════════════════════════════════════════════
static constexpr uint64_t kOffAmmoMagazineCapacity     = 0x10;
static constexpr uint64_t kOffAmmoCapacity             = 0x12;
static constexpr uint64_t kOffAmmoMagazineCapacitySafe = 0x14;
static constexpr uint64_t kOffAmmoCapacitySafe         = 0x20;

static void patch_ammo(uint64_t params, short value) {
    uint64_t ammo = rpm<uint64_t>(params + oxorany(kOffWeaponParametersAmmunition));
    if (!likely_ptr(ammo)) return;
    wpm<short>(ammo + oxorany(kOffAmmoMagazineCapacity), value);
    wpm<short>(ammo + oxorany(kOffAmmoCapacity), value);
    write_nullable_safe_int(ammo + oxorany(kOffAmmoMagazineCapacitySafe), value);
    write_nullable_safe_int(ammo + oxorany(kOffAmmoCapacitySafe), value);
}

static void ammo_run_impl(bool enabled, int cfg_value) {
    if (!enabled) return;
    int clamped = std::clamp(cfg_value, 100, 30000);
    short value = static_cast<short>(clamped);
    auto params = collect_weapon_params();
    for (uint64_t p : params) patch_ammo(p, value);
}

// ═════════════════════════════════════════════════════════════════════════
//  FAST KNIFE — обнуление таймера удара ножа (weapon id = 70)
// ═════════════════════════════════════════════════════════════════════════
static constexpr uint64_t kOffWeaponControllerFireTime = 0x120;
static constexpr int      kKnifeId = 70;

void fustknife_run_impl() {
    if (!cfg::fustknife::enabled) return;
    uint64_t lp = get_local_player();
    if (!likely_ptr(lp)) return;
    uint64_t wc = get_weapon_controller(lp);
    if (!likely_ptr(wc)) return;
    uint64_t params = get_weapon_params(wc);
    if (!likely_ptr(params)) return;
    if (rpm<int>(params + oxorany(kOffWeaponParametersId)) != kKnifeId) return;
    wpm<float>(wc + oxorany(kOffWeaponControllerFireTime), 0.0f);
}

// ═════════════════════════════════════════════════════════════════════════
//  SIGMA — One Hit Kill (DamageData зоны голова/торс/руки/ноги)
// ═════════════════════════════════════════════════════════════════════════
static constexpr uint64_t kOffWeaponParametersDamage = 0x140;
static constexpr uint64_t kOffDamageZone1 = 0x28;
static constexpr uint64_t kOffDamageZone2 = 0x34;
static constexpr uint64_t kOffDamageZone3 = 0x40;
static constexpr uint64_t kOffDamageZone4 = 0x4C;
static constexpr uint64_t kSafeIntValueOff = 0x4;

void sigma_run_impl() {
    if (!cfg::sigma::enabled) return;
    uint64_t lp = get_local_player();
    if (!likely_ptr(lp)) return;
    uint64_t wc = get_weapon_controller(lp);
    if (!likely_ptr(wc)) return;
    uint64_t params = get_weapon_params(wc);
    if (!likely_ptr(params)) return;

    int weapon_id = rpm<int>(params + oxorany(kOffWeaponParametersId));
    if (weapon_id == kKnifeId) return;
    if (!weapon_is_probably_gun(params)) return;

    uint64_t damage = rpm<uint64_t>(params + oxorany(kOffWeaponParametersDamage));
    if (!likely_ptr(damage)) return;

    int dmg = std::clamp(cfg::sigma::damage, 1, 1000);
    wpm<int>(damage + oxorany(kOffDamageZone1 + kSafeIntValueOff), dmg);
    wpm<int>(damage + oxorany(kOffDamageZone2 + kSafeIntValueOff), dmg);
    wpm<int>(damage + oxorany(kOffDamageZone3 + kSafeIntValueOff), dmg);
    wpm<int>(damage + oxorany(kOffDamageZone4 + kSafeIntValueOff), dmg);
}

// ═════════════════════════════════════════════════════════════════════════
//  FIRE RATE — обнуление fireDuration (WeaponController + 0x108).
//  Цепочка: PlayerManager(+0x70) → LocalPlayer → Weaponry(+0x88) →
//  CurrentWeapon(+0xA0) → WeaponParameters(+0xA8). Если WeaponParameters+0x18
//  в диапазоне [1..69] — пишем 0.0f в fireDuration каждый тик. Иначе
//  пропускаем (нож/граната — пишем через fustknife/sigma, не трогаем).
// ═════════════════════════════════════════════════════════════════════════
static constexpr uint64_t kOffWcFireDuration = 0x108;

void fire_rate_run_impl() {
    if (!cfg::fire_rate::enabled) return;
    if (!cfg::fire_rate::enabled) return;

    // Защита от двойного захода: weapon::run_all() уже синхронизирован с main.
    static uint64_t s_last_local = 0;
    static uint64_t s_last_wc    = 0;
    static bool     s_is_gun     = false;

    uint64_t local = get_local_player();
    if (!likely_ptr(local)) { s_last_wc = 0; s_is_gun = false; return; }

    uint64_t wc = get_weapon_controller(local);
    if (!likely_ptr(wc)) { s_last_wc = 0; s_is_gun = false; return; }

    if (wc != s_last_wc || local != s_last_local) {
        s_last_wc    = wc;
        s_last_local = local;
        s_is_gun     = false;
        uint64_t params = get_weapon_params(wc);
        if (!likely_ptr(params)) return;
        int id = rpm<int>(params + oxorany(kOffWeaponParametersId));
        if (id >= 1 && id < 70) s_is_gun = true;
    }

    if (!s_is_gun) return;

    uint64_t params = get_weapon_params(wc);
    if (!likely_ptr(params)) { s_last_wc = 0; s_is_gun = false; return; }

    // Каждый раз перепроверяем id на случай смены оружия в этом кадре.
    int id = rpm<int>(params + oxorany(kOffWeaponParametersId));
    if (id < 1 || id >= 70) {
        s_is_gun = false;
        return;
    }

    // fireDuration хранится как SafeFloatInline (salt:int @0, value:float @4),
    // но игра читает «сырое» float-поле в таймере — wpm<float> по 0x108.
    SafeFloatInline s{};
    if (!mem_read(wc + oxorany(kOffWcFireDuration), &s, sizeof(s))) return;
    if (!std::isfinite(s.value) || s.value < 0.f || s.value > 5.f) return;
    s.value = 0.f;
    bool ok = mem_write(wc + oxorany(kOffWcFireDuration), &s, sizeof(s));
    (void)ok;
}

// ═════════════════════════════════════════════════════════════════════════
//  NORECOIL — рекойл/точность
// ═════════════════════════════════════════════════════════════════════════
static constexpr uint64_t kOffGunAccuracyData            = 0x228;
static constexpr uint64_t kOffGunRecoilControl           = 0x160;
static constexpr uint64_t kOffGunSafeRadius              = 0x1EC;
static constexpr uint64_t kOffGunSafeMinRandom           = 0x1F4;
static constexpr uint64_t kOffGunSafeMaxRandom           = 0x1FC;
static constexpr uint64_t kOffGunSafeExtra               = 0x204;

static constexpr uint64_t kOffGunParametersRecoilControl           = 0x150;
static constexpr uint64_t kOffGunParametersRecoilParameters        = 0x158;
static constexpr uint64_t kOffGunParametersRecoilMultOnCrouch      = 0x178;
static constexpr uint64_t kOffGunParametersRecoilAimMult           = 0x180;
static constexpr uint64_t kOffGunParametersRecoilAimMultOnCrouch   = 0x188;
static constexpr uint64_t kOffGunParametersRecoilMultOnCrouchSafe  = 0x1F8;
static constexpr uint64_t kOffGunParametersRecoilAimMultSafe       = 0x210;
static constexpr uint64_t kOffGunParametersRecoilAimMultOnCrouchSafe = 0x228;
static constexpr uint64_t kOffGunParametersRecoilControlSafe       = 0x240;

static bool patch_accuracy(uint64_t weapon) {
    uint64_t accuracy = 0;
    if (!read_ptr(weapon + oxorany(kOffGunAccuracyData), accuracy)) return false;
    float a = 0.f, b = 0.f;
    if (!mem_read(accuracy + oxorany(0x10), &a, sizeof(a))) return false;
    if (!mem_read(accuracy + oxorany(0x14), &b, sizeof(b))) return false;
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    bool ok = true;
    ok &= wpm<float>(accuracy + oxorany(0x10), 0.f);
    ok &= wpm<float>(accuracy + oxorany(0x14), 0.f);
    ok &= wpm<float>(accuracy + oxorany(0x18), 0.f);
    return ok;
}

static bool patch_recoil_control(uint64_t weapon, float mult) {
    uint64_t rc = 0;
    if (!read_ptr(weapon + oxorany(kOffGunRecoilControl), rc)) return false;
    bool ok = false;
    ok |= write_safe_float_inline_value(rc + oxorany(0x50), mult);
    ok |= write_safe_float_inline_value(rc + oxorany(0x58), 0.f);
    ok |= write_safe_float_inline_value(rc + oxorany(0x60), 0.f);
    return ok;
}

static bool patch_recoil_params(uint64_t params) {
    uint64_t rp = 0;
    if (!read_ptr(params + oxorany(kOffGunParametersRecoilParameters), rp)) return false;
    bool ok = true;
    ok &= wpm<float>(rp + oxorany(0x10), 0.f);
    ok &= wpm<float>(rp + oxorany(0x14), 0.f);
    ok &= wpm<float>(rp + oxorany(0x48), 0.f);
    ok &= wpm<float>(rp + oxorany(0x4C), 0.f);
    return ok;
}

static bool patch_gun_parameters(uint64_t params, float mult) {
    if (!likely_ptr(params)) return false;
    bool ok = false;
    ok |= wpm<int>(params + oxorany(kOffGunParametersRecoilControl), 10000);
    ok |= wpm<float>(params + oxorany(kOffGunParametersRecoilMultOnCrouch), mult);
    ok |= wpm<float>(params + oxorany(kOffGunParametersRecoilAimMult), mult);
    ok |= wpm<float>(params + oxorany(kOffGunParametersRecoilAimMultOnCrouch), mult);
    ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilMultOnCrouchSafe));
    ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilAimMultSafe));
    ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilAimMultOnCrouchSafe));
    ok |= disable_nullable_safe(params + oxorany(kOffGunParametersRecoilControlSafe));
    ok |= patch_recoil_params(params);
    return ok;
}

void norecoil_run_impl() {
    if (!cfg::norecoil::enabled) return;
    uint64_t lp = get_local_player();
    if (!likely_ptr(lp)) return;
    uint64_t wc = get_weapon_controller(lp);
    if (!likely_ptr(wc)) return;
    uint64_t params = get_weapon_params(wc);
    if (!likely_ptr(params)) return;
    if (!weapon_is_probably_gun(params)) return;

    float mult = std::clamp(cfg::norecoil::multiplier, 0.0f, 0.50f);
    patch_accuracy(wc);
    patch_recoil_control(wc, mult);
    patch_gun_parameters(params, mult);
    write_safe_float_inline_value(wc + oxorany(kOffGunSafeRadius), mult);
    write_safe_float_inline_value(wc + oxorany(kOffGunSafeMinRandom), 0.f);
    write_safe_float_inline_value(wc + oxorany(kOffGunSafeMaxRandom), 0.f);
    write_safe_float_inline_value(wc + oxorany(kOffGunSafeExtra), 0.f);
}

} // namespace

void wallshot::run()  { wallshot_run_impl(); }
void norecoil::run()  { norecoil_run_impl(); }
void inf_ammo::run()  { ammo_run_impl(cfg::inf_ammo::enabled, cfg::inf_ammo::value); }
void inf_shop::run()  { ammo_run_impl(cfg::inf_shop::enabled, cfg::inf_shop::value); }
void fustknife::run() { fustknife_run_impl(); }
void sigma::run()     { sigma_run_impl(); }
void fire_rate::run() { fire_rate_run_impl(); }

void weapon::run_all() {
    wallshot::run();
    norecoil::run();
    inf_ammo::run();
    inf_shop::run();
    fustknife::run();
    sigma::run();
    fire_rate::run();
}
