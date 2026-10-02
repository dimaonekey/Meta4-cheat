#include "chams.hpp"
#include "../game/game.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include <vector>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

namespace {
    // Белый чамс: search = 1073741859 → replace = 1073741910
    static constexpr uint32_t SEARCH_VALUE = 1073741859;
    static constexpr uint32_t REPLACE_VALUE = 1073741910;

    struct BackupEntry {
        uint64_t address;
        uint32_t original_value;
    };
    static std::vector<BackupEntry> backup;
    static std::mutex backup_mutex;

    static std::atomic<bool> worker_active{false};
    static std::thread worker_thread;

    static int get_pid() {
        DIR* dir = opendir("/proc");
        if (!dir) return -1;
        struct dirent* entry;
        char path[64], cmdline[256];
        while ((entry = readdir(dir))) {
            int id = atoi(entry->d_name);
            if (id <= 0) continue;
            snprintf(path, sizeof(path), "/proc/%d/cmdline", id);
            FILE* fp = fopen(path, "r");
            if (fp) {
                memset(cmdline, 0, sizeof(cmdline));
                fread(cmdline, 1, sizeof(cmdline) - 1, fp);
                fclose(fp);
                if (strstr(cmdline, "com.axlebolt.standoff2")) {
                    closedir(dir);
                    return id;
                }
            }
        }
        closedir(dir);
        return -1;
    }

    // Применить замену (добавляет новые адреса в бэкап)
    static void apply_patch(int pid) {
        char mapsPath[64];
        snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", pid);
        FILE* maps = fopen(mapsPath, "r");
        if (!maps) return;

        const size_t CHUNK = 4 * 1024 * 1024;
        uint8_t* buf = (uint8_t*)malloc(CHUNK);
        if (!buf) { fclose(maps); return; }

        std::vector<BackupEntry> local_backup;

        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (!strchr(line, '\n')) {
                int c; while ((c = fgetc(maps)) != '\n' && c != EOF) {}
            }

            uint64_t rs, re;
            char perms[8] = {};
            if (sscanf(line, "%llx-%llx %7s",
                       (unsigned long long*)&rs,
                       (unsigned long long*)&re,
                       perms) < 3) continue;

            if (!strstr(perms, "rw")) continue;

            uint64_t regionSize = re - rs;
            if (regionSize < sizeof(uint32_t)) continue;

            for (uint64_t off = 0; off < regionSize; off += CHUNK) {
                size_t toRead = regionSize - off;
                if (toRead > CHUNK) toRead = CHUNK;
                if (toRead < sizeof(uint32_t)) break;

                struct iovec local = { buf, toRead };
                struct iovec remote = { (void*)(rs + off), toRead };
                ssize_t rd = syscall(SYS_process_vm_readv, pid, &local, 1, &remote, 1, 0);
                if (rd != (ssize_t)toRead) continue;

                for (size_t i = 0; i + sizeof(uint32_t) <= toRead; i += sizeof(uint32_t)) {
                    uint32_t value;
                    memcpy(&value, buf + i, sizeof(uint32_t));
                    if (value == SEARCH_VALUE) {
                        uint64_t addr = rs + off + i;
                        // Проверяем, не сохранён ли уже адрес
                        bool exists = false;
                        for (const auto& b : backup) {
                            if (b.address == addr) { exists = true; break; }
                        }
                        if (!exists) {
                            local_backup.push_back({addr, value});
                        }
                        // Заменяем
                        uint32_t new_val = REPLACE_VALUE;
                        struct iovec wlocal = { &new_val, sizeof(uint32_t) };
                        struct iovec wremote = { (void*)addr, sizeof(uint32_t) };
                        syscall(SYS_process_vm_writev, pid, &wlocal, 1, &wremote, 1, 0);
                    }
                }
            }
        }

        fclose(maps);
        free(buf);

        if (!local_backup.empty()) {
            std::lock_guard<std::mutex> lock(backup_mutex);
            backup.insert(backup.end(), local_backup.begin(), local_backup.end());
        }
    }

    // Восстановить оригиналы
    static void restore_backup(int pid) {
        std::lock_guard<std::mutex> lock(backup_mutex);
        for (const auto& entry : backup) {
            uint32_t orig = entry.original_value;
            struct iovec wlocal = { &orig, sizeof(uint32_t) };
            struct iovec wremote = { (void*)entry.address, sizeof(uint32_t) };
            syscall(SYS_process_vm_writev, pid, &wlocal, 1, &wremote, 1, 0);
        }
        backup.clear();
    }

    // Рабочий поток: каждые 5 секунд применяет или восстанавливает
    static void worker_loop() {
        while (worker_active.load()) {
            int pid = get_pid();
            if (pid > 0) {
                if (cfg::chams2::enabled) {
                    apply_patch(pid);
                } else {
                    restore_backup(pid);
                }
            }
            // Сон 5 секунд
            for (int i = 0; i < 50 && worker_active.load(); i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        // При завершении восстанавливаем
        int pid = get_pid();
        if (pid > 0) restore_backup(pid);
    }

    static void start_worker() {
        if (worker_active.load()) return;
        worker_active = true;
        worker_thread = std::thread(worker_loop);
    }

    static void stop_worker() {
        if (!worker_active.load()) return;
        worker_active = false;
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
        std::lock_guard<std::mutex> lock(backup_mutex);
        backup.clear();
    }
}

void chams::run() {
    if (cfg::chams2::enabled) {
        start_worker();
    } else {
        stop_worker();
    }
}