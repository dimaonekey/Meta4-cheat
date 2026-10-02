#pragma once
#include <cstdint>
void movement_cache_refresh();
namespace air_jump     { void run(); }
namespace strafe       { void run(); }
namespace bunny_hop    { void run(); }
namespace crouch_speed { void run(); }
namespace fast_walk    { void run(); }
namespace air_strafe   { void run(); }
namespace teleport_hack { void run(uint64_t weaponry); }
namespace high_jump     { void run(); }
