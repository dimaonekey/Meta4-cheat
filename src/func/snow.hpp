#pragma once

#include <imgui.h>
#include <vector>
#include <random>
#include "../ui/cfg.hpp"
#include "../game/math.hpp"

struct SnowParticle
{
    Vector3 world_pos;
    Vector3 velocity;

    float life;
    float size;
};


inline std::vector<SnowParticle> snow_particles;

inline std::mt19937 snow_rng(std::random_device{}());


inline float snow_rand(float a, float b)
{
    std::uniform_real_distribution<float> d(a, b);
    return d(snow_rng);
}



inline void SnowEffect(const matrix& view, const Vector3& center)
{
    if (!cfg::esp::snow)
        return;


    ImDrawList* draw = ImGui::GetBackgroundDrawList();


    static bool initialized = false;


    if (!initialized)
    {
        for (int i = 0; i < 150; i++)
        {
            SnowParticle p;


            // создаём снег вокруг точки в мире
            p.world_pos = Vector3(
                center.x + snow_rand(-25.f, 25.f),
                center.y + snow_rand(5.f, 30.f),
                center.z + snow_rand(-25.f, 25.f)
            );


            p.velocity = Vector3(
                snow_rand(-0.02f,0.02f),
                snow_rand(-0.15f,-0.05f),
                snow_rand(-0.02f,0.02f)
            );


            p.life = snow_rand(2.f,6.f);
            p.size = snow_rand(1.5f,3.f);


            snow_particles.push_back(p);
        }


        initialized = true;
    }



    float dt = ImGui::GetIO().DeltaTime;



    for(auto& p : snow_particles)
    {

        // движение в мире
        p.world_pos.x += p.velocity.x;
        p.world_pos.y += p.velocity.y;
        p.world_pos.z += p.velocity.z;


        p.life -= dt;



        // обновляем если умерла
        if(p.life <= 0.f)
        {
            p.world_pos = Vector3(
                center.x + snow_rand(-25.f,25.f),
                center.y + snow_rand(10.f,30.f),
                center.z + snow_rand(-25.f,25.f)
            );


            p.velocity.y = snow_rand(-0.15f,-0.05f);

            p.life = snow_rand(2.f,6.f);
        }



        ImVec2 screen;


        // если частица вне камеры не рисуем
        if(!world_to_screen(
            p.world_pos,
            view,
            screen))
            continue;



        float alpha =
            std::clamp(
                p.life / 6.f,
                0.f,
                1.f);



        draw->AddCircleFilled(
            screen,
            p.size * 2.f,

            ImColor(
                cfg::esp::snow_col.x,
                cfg::esp::snow_col.y,
                cfg::esp::snow_col.z,
                0.15f * alpha
            )
        );


        draw->AddCircleFilled(
            screen,
            p.size,

            ImColor(
                cfg::esp::snow_col.x,
                cfg::esp::snow_col.y,
                cfg::esp::snow_col.z,
                alpha
            )
        );
    }
}