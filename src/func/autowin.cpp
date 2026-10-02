// ─────────────────────────────────────────────────────────────────────────────
//  AUTO WIN — визуально обнуляем HP врагов + таймеры раунда
//  Порт f_autowin.hpp под normal7 SDK (rpm/wpm + oxorany).
//  Если ты хост — изменения таймеров синхронизируются всем в комнате.
// ─────────────────────────────────────────────────────────────────────────────

#include "autowin.hpp"
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <string>

namespace {
    static inline bool valid_addr(uint64_t a) {
        return a > 0x10000ull && a < 0x0000FFFFFFFFFFFFull;
    }

    static void zero_round_timers() {
        if (proc::lib == 0) return;

        uint64_t pm = get_player_manager();
        if (!valid_addr(pm)) return;

        uint64_t gc = rpm<uint64_t>(pm + oxorany(0x18ULL));
        if (!valid_addr(gc)) return;

        // DefuseController offsets — пишем напрямую без oxorany (переменная offset)
        const uint64_t defuse_offs[] = { 0x2E4ULL, 0x2E8ULL, 0x2ECULL, 0x2FCULL };
        for (auto off : defuse_offs)
            wpm<uint32_t>(gc + off, 0u);

        uint64_t settings = rpm<uint64_t>(gc + oxorany(0xD0ULL));
        if (!valid_addr(settings)) return;
        wpm<uint32_t>(settings + oxorany(0x1CULL), 0u);
        wpm<uint32_t>(settings + oxorany(0x20ULL), 0u);
        wpm<uint32_t>(settings + oxorany(0x24ULL), 0u);
    }

    static void kill_enemies() {
        uint64_t pm = get_player_manager();
        if (!valid_addr(pm)) return;

        uint64_t lp = rpm<uint64_t>(pm + oxorany(0x70ULL));
        if (!valid_addr(lp)) return;

        int local_team = (int)rpm<uint8_t>(lp + oxorany(0x79ULL));

        uint64_t player_list = rpm<uint64_t>(pm + oxorany(0x28ULL));
        if (!valid_addr(player_list)) return;
        int count = rpm<int>(player_list + oxorany(0x20ULL));
        if (count <= 0 || count > 64) return;
        uint64_t list_buf = rpm<uint64_t>(player_list + oxorany(0x18ULL));
        if (!valid_addr(list_buf)) return;

        for (int i = 0; i < count; i++) {
            // oxorany только на константах, i — переменная, умножаем напрямую
            uint64_t e = rpm<uint64_t>(list_buf + oxorany(0x30ULL) + 0x18ULL * (uint64_t)i);
            if (!valid_addr(e) || e == lp) continue;
            if ((int)rpm<uint8_t>(e + oxorany(0x79ULL)) == local_team) continue;
            if (player::health(e) <= 0) continue;

            uint64_t photon = player::photon_ptr(e);
            if (!valid_addr(photon)) continue;
            uint64_t props_reg = rpm<uint64_t>(photon + oxorany(0x38ULL));
            if (!valid_addr(props_reg)) continue;
            int pcount = rpm<int>(props_reg + oxorany(0x20ULL));
            if (pcount <= 0 || pcount > 128) continue;
            uint64_t plist = rpm<uint64_t>(props_reg + oxorany(0x18ULL));
            if (!valid_addr(plist)) continue;

            for (int j = 0; j < pcount; j++) {
                uint64_t key = rpm<uint64_t>(plist + oxorany(0x28ULL) + 0x18ULL * (uint64_t)j);
                uint64_t val = rpm<uint64_t>(plist + oxorany(0x30ULL) + 0x18ULL * (uint64_t)j);
                if (!valid_addr(key) || !valid_addr(val)) continue;
                std::string ks = rpm<read_string>(key).as_utf8();
                if (ks.find("health") != std::string::npos) {
                    wpm<int>(val + oxorany(0x10ULL), 0);
                    break;
                }
            }
        }
    }
}

namespace autowin {

void run() {
    if (!cfg::misc::autowin) return;
    kill_enemies();
    zero_round_timers();
}

} // namespace autowin
