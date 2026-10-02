#include "bar.hpp"

namespace ui::bar {
    void set_game_alpha(float a) {
        if (a < 0.f) a = 0.f;
        if (a > 1.f) a = 1.f;
        g_game_alpha = a;
    }
    float game_alpha() { return g_game_alpha; }
    void render() {}
}
