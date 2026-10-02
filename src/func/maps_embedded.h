#pragma once

#include <cstddef>

// Embedded collider maps (.glb), gzip-compressed and stored as base64
// inside the binary. At startup / on first use they are transparently
// extracted to /sdcard/NexuraWare/maps/ if missing there.
namespace embedded_maps {

struct EmbeddedMap {
    const char* fileName;  // e.g. "breeze_colliders.glb"
    const char* b64Data;   // base64 of the gzip-compressed GLB
    size_t      b64Size;   // strlen(b64Data)
    size_t      rawSize;   // size of the decompressed GLB in bytes
};

extern const EmbeddedMap kMaps[];
extern const int         kMapCount;

// Creates dirPath (with parents) if needed and writes every embedded map
// that is missing (or has a wrong size) into dirPath.
// Safe to call repeatedly - existing valid files are skipped.
// Returns the number of files written.
int ExtractAll(const char* dirPath);

// Decompresses map[index] into a heap buffer (free with delete[]).
// Returns the decompressed size, or 0 on failure.
size_t Decompress(int index, unsigned char** outData);

} // namespace embedded_maps
