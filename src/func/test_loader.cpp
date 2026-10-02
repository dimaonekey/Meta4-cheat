#include "test_loader.hpp"
#include "../game/game.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"

namespace {
    static constexpr uint64_t kOffPlayerManagerStaticLegacy = 132435632;
    static constexpr uint64_t kOffPlayerManagerLocalPlayer = 0x70;
    static constexpr uint64_t kOffPlayerWeaponryController = 0x88;
    static constexpr uint64_t kOffPlayerWeaponryControllerAlt = 0x68;
    static constexpr uint64_t kOffWeaponryInvisibleState = 0x88;

    static bool likely_ptr(uint64_t p) {
        return p > 0x10000ull && p < 0x0000FFFFFFFFFFFFull;
    }

    static bool read_ptr(uint64_t addr, uint64_t& out) {
        out = 0;
        if (!likely_ptr(addr)) return false;
        if (!mem_read(addr, &out, sizeof(out))) return false;
        return likely_ptr(out);
    }

    static bool get_local_player(uint64_t& out_local) {
        out_local = 0;

        uint64_t player_manager = get_player_manager();
        if (!likely_ptr(player_manager)) {
            player_manager = get_static<uint64_t>(oxorany(kOffPlayerManagerStaticLegacy));
        }
        if (!likely_ptr(player_manager)) return false;

        return read_ptr(player_manager + oxorany(kOffPlayerManagerLocalPlayer), out_local);
    }

    static bool get_weaponry(uint64_t& out_weaponry) {
        out_weaponry = 0;

        uint64_t local = 0;
        if (!get_local_player(local)) return false;

        if (!read_ptr(local + oxorany(kOffPlayerWeaponryController), out_weaponry)) {
            if (!read_ptr(local + oxorany(kOffPlayerWeaponryControllerAlt), out_weaponry)) return false;
        }
        return likely_ptr(out_weaponry);
    }
}

void test_loader::run() {
    if (!cfg::test::invisible) return;

    uint64_t weaponry = 0;
    if (!get_weaponry(weaponry)) return;

    uint8_t slot = rpm<uint8_t>(weaponry + oxorany(kOffWeaponryInvisibleState));
    if (slot > 20) return;
    wpm<uint8_t>(weaponry + oxorany(kOffWeaponryInvisibleState), 10);
}
