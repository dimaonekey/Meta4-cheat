#pragma once

#include "maptypes.h"
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>

struct ImDrawList;

// Forward declarations for internal types
namespace map_service_internal {
    struct Vector3;
    struct Quaternion;
}

using MapVector3    = map_service_internal::Vector3;
using MapQuaternion = map_service_internal::Quaternion;

struct MapTriangle {
    MapVector3  v1, v2, v3;
    std::string materialName;
};

struct BakedTriangle {
    MapVector3  v1, v2, v3;
    MapVector3  min, max;
    std::string materialName;
};

struct MapConfig {
    MapVector3 mapScale    = MapVector3(1, 1, 1);
    MapVector3 mapRotation = MapVector3(0, 180.0f, 0);
    MapVector3 mapOffset   = MapVector3(0, 0, 0);
    bool flipX = false;
    bool flipY = false;
    bool flipZ = true;
};

struct matrix;

namespace map_service {

bool LoadMap(const std::string& filename);
bool LoadMapFromMemory(const unsigned char* data, size_t size);
void Render(ImDrawList* draw_list, const matrix& vm, float screen_w, float screen_h,
            const std::string* filterMaterialName = nullptr);
void Update();
float RaycastWallExcluding(MapVector3 origin, MapVector3 dir, float maxDist,
                            const std::vector<std::string>& excludeSubstrings);
float RaycastWallWithMaterial(MapVector3 origin, MapVector3 dir, float maxDist,
                               std::string* outMaterial);
float RaycastClosestWithName(MapVector3 origin, MapVector3 dir, float maxDist,
                              std::string* outMaterial);
float RaycastWallClosest(MapVector3 origin, MapVector3 dir, float maxDist);
float RaycastWallClosestWithNormal(MapVector3 origin, MapVector3 dir, float maxDist, MapVector3* outNormal);
bool  GetColliderAABB(const char* nameSubstring, MapVector3* outMin, MapVector3* outMax);
float RaycastColliderByName(MapVector3 origin, MapVector3 dir, float maxDist,
                             const char* nameSubstring);
void  RenderColliderHighlight(ImDrawList* draw_list, const matrix& vm,
                               float screen_w, float screen_h,
                               const char* nameSubstring, unsigned int color);
static constexpr int EMBEDDED_MAP_COUNT = 24;
extern const char* g_collider_map_names[];
extern const char* g_collider_map_display_names[];
extern int         g_collider_map_count;
extern int         g_collider_map_index;
void LoadColliderMapByIndex(int index);
extern bool         g_enabled;
extern unsigned int g_color;      
extern bool         g_loaded;
extern MapConfig    g_config;
extern MapVector3   g_cam_pos;    
extern float        g_max_dist;   
} 
