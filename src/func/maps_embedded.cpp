#include "maps_embedded.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <new>
#include <sys/stat.h>
#include <zlib.h>

#ifdef __ANDROID__
#include <android/log.h>
#define EM_LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "maps_embedded", __VA_ARGS__)
#else
#include <cstdio>
#define EM_LOG(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)
#endif

// Generated table: kMaps[] / kMapCount
#include "maps_embedded.inc"

namespace {

inline int B64Val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1; // padding / whitespace
}

// base64 -> gzip inflate -> raw GLB bytes
unsigned char* DecodeGunzip(const embedded_maps::EmbeddedMap& m, size_t* outSize) {
    *outSize = 0;
    if (!m.b64Data || m.b64Size == 0 || m.rawSize == 0) return nullptr;

    const size_t gzCap = (m.b64Size / 4) * 3 + 4;
    unsigned char* gz = new (std::nothrow) unsigned char[gzCap];
    if (!gz) return nullptr;

    size_t gzLen = 0;
    unsigned int acc = 0;
    int accBits = 0;
    for (size_t i = 0; i < m.b64Size; ++i) {
        int v = B64Val(m.b64Data[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned int)v;
        accBits += 6;
        if (accBits >= 8) {
            accBits -= 8;
            gz[gzLen++] = (unsigned char)((acc >> accBits) & 0xFF);
        }
    }

    unsigned char* raw = new (std::nothrow) unsigned char[m.rawSize];
    if (!raw) { delete[] gz; return nullptr; }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in   = gz;
    strm.avail_in  = (uInt)gzLen;
    strm.next_out  = raw;
    strm.avail_out = (uInt)m.rawSize;

    if (inflateInit2(&strm, 47) != Z_OK) { // 32+15 = auto gzip/zlib
        delete[] gz; delete[] raw;
        return nullptr;
    }
    int ret = inflate(&strm, Z_FINISH);
    const size_t produced = strm.total_out;
    inflateEnd(&strm);
    delete[] gz;

    if (ret != Z_STREAM_END || produced != m.rawSize) {
        EM_LOG("[-] maps_embedded: inflate failed for %s (ret=%d, %zu/%zu)",
               m.fileName, ret, produced, m.rawSize);
        delete[] raw;
        return nullptr;
    }
    *outSize = produced;
    return raw;
}

} // namespace

int embedded_maps::ExtractAll(const char* dirPath) {
    if (!dirPath || !dirPath[0]) return 0;

    // mkdir -p
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dirPath);
    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);

    int written = 0;
    for (int i = 0; i < kMapCount; ++i) {
        const EmbeddedMap& m = kMaps[i];

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dirPath, m.fileName);

        struct stat st;
        if (stat(path, &st) == 0 && (size_t)st.st_size == m.rawSize)
            continue; // already extracted, nothing to do

        size_t rawSize = 0;
        unsigned char* raw = DecodeGunzip(m, &rawSize);
        if (!raw || rawSize != m.rawSize) {
            EM_LOG("[-] maps_embedded: decode failed for %s", m.fileName);
            delete[] raw;
            continue;
        }

        char tmpPath[520];
        snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
        FILE* f = fopen(tmpPath, "wb");
        if (!f) {
            EM_LOG("[-] maps_embedded: fopen %s failed errno=%d", tmpPath, errno);
            delete[] raw;
            continue;
        }
        size_t w = fwrite(raw, 1, rawSize, f);
        fclose(f);
        delete[] raw;

        if (w != rawSize) {
            EM_LOG("[-] maps_embedded: short write for %s", path);
            remove(tmpPath);
            continue;
        }
        if (rename(tmpPath, path) != 0) {
            EM_LOG("[-] maps_embedded: rename to %s failed errno=%d", path, errno);
            remove(tmpPath);
            continue;
        }
        ++written;
        EM_LOG("[+] maps_embedded: extracted %s (%zu bytes)", path, rawSize);
    }
    return written;
}

size_t embedded_maps::Decompress(int index, unsigned char** outData) {
    if (!outData || index < 0 || index >= kMapCount) return 0;
    *outData = nullptr;
    size_t sz = 0;
    unsigned char* raw = DecodeGunzip(kMaps[index], &sz);
    if (!raw) return 0;
    *outData = raw;
    return sz;
}
