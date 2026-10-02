// ─────────────────────────────────────────────────────────────────────────────
//  CHAMS — Material Chams (Memory Write)
//  Порт логики из chams1 в C++ под normal5 SDK.
//  Memory type: QWORD (uint64_t) чтение адресов, DWORD (int/uint32_t) запись ID.
//  Записываем 0 в Material ID чтобы форсировать белый/дефолт материал.
// ─────────────────────────────────────────────────────────────────────────────

#include "matchams.hpp"
#include "../game/game.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"

namespace chams {

    void handle() {
        if (!cfg::matchams::enabled) return;

        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager) return;

        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // QWORD
        if (!LocalPlayer) return;

        int LocalTeam = rpm<uint8_t>(LocalPlayer + oxorany(0x79));             // BYTE

        uint64_t PlayerList   = rpm<uint64_t>(PlayerManager + oxorany(0x28)); // QWORD
        if (!PlayerList) return;

        int      PlayerCount  = rpm<int>(PlayerList + oxorany(0x20));          // DWORD
        if (PlayerCount <= 0 || PlayerCount > 128) return;

        uint64_t ListBuffer   = rpm<uint64_t>(PlayerList + oxorany(0x18));     // QWORD
        if (!ListBuffer) return;

        for (int i = 0; i < PlayerCount; i++) {
            // QWORD — указатель на игрока
            uint64_t Player = rpm<uint64_t>(ListBuffer + oxorany(0x30) + oxorany(0x18) * i);
            if (!Player || Player == LocalPlayer) continue;

            int PlayerTeam = rpm<uint8_t>(Player + oxorany(0x79));             // BYTE

            // Если включён team check — пропускаем союзников
            if (cfg::matchams::team_check && PlayerTeam == LocalTeam) continue;

            if (player::health(Player) <= 0) continue;

            // QWORD — CharacterLodGroup (offset 0x128)
            uint64_t CharacterLodGroup = rpm<uint64_t>(Player + oxorany(0x128));
            if (!CharacterLodGroup) continue;

            // QWORD — SkinnedMeshRenderer (offset 0x30)
            uint64_t SkinnedMeshRenderer = rpm<uint64_t>(CharacterLodGroup + oxorany(0x30));
            if (!SkinnedMeshRenderer) continue;

            // QWORD — Step (offset 0x10)
            uint64_t Step = rpm<uint64_t>(SkinnedMeshRenderer + oxorany(0x10));
            if (!Step) continue;

            // QWORD — Material ID ptr (offset 0x140)
            uint64_t IdPtr = rpm<uint64_t>(Step + oxorany(0x140));
            if (!IdPtr) continue;

            // DWORD (int) — запись 0 форсирует дефолтный/белый material (Material Chams)
            wpm<int>(IdPtr, 0);
        }
    }

} // namespace chams
