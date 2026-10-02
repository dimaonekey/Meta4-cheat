#pragma once

#include <imgui.h>
#include <vector>
#include <random>
#include "../ui/cfg.hpp"
#include "../game/math.hpp"


struct RainParticle
{
    Vector3 world_pos;
    Vector3 velocity;

    float life;
    float length;
};


inline std::vector<RainParticle> rain_particles;


inline std::mt19937 rain_rng(std::random_device{}());


inline float rain_rand(float a, float b)
{
    std::uniform_real_distribution<float> d(a,b);
    return d(rain_rng);
}



inline void RainEffect(const matrix& view, const Vector3& center)
{
    if(!cfg::esp::rain)
        return;


    ImDrawList* draw = ImGui::GetBackgroundDrawList();


    static bool initialized = false;


    if(!initialized)
    {

        for(int i = 0; i < 220; i++)
        {

            RainParticle p;


            // область дождя вокруг игрока
            p.world_pos = Vector3(
                center.x + rain_rand(-35.f,35.f),
                center.y + rain_rand(5.f,35.f),
                center.z + rain_rand(-35.f,35.f)
            );


            // падение вниз + немного ветра
            p.velocity = Vector3(
                rain_rand(-0.01f,0.01f),
                rain_rand(-0.7f,-0.45f),
                rain_rand(-0.01f,0.01f)
            );


            p.life = rain_rand(1.f,3.f);


            // длинные капли
            p.length = rain_rand(0.5f,1.5f);


            rain_particles.push_back(p);

        }


        initialized = true;
    }



    float dt = ImGui::GetIO().DeltaTime;



    for(auto& r : rain_particles)
    {

        r.world_pos.x += r.velocity.x;
        r.world_pos.y += r.velocity.y;
        r.world_pos.z += r.velocity.z;


        r.life -= dt;



        // заново сверху
        if(r.life <= 0.f || r.world_pos.y < center.y - 5.f)
        {

            r.world_pos = Vector3(
                center.x + rain_rand(-35.f,35.f),
                center.y + rain_rand(20.f,40.f),
                center.z + rain_rand(-35.f,35.f)
            );


            r.life = rain_rand(1.f,3.f);
        }



        ImVec2 start;


        if(!world_to_screen(
            r.world_pos,
            view,
            start))
            continue;



        // конец капли в мире
        Vector3 end_world(
            r.world_pos.x,
            r.world_pos.y + r.length,
            r.world_pos.z
        );


        ImVec2 end;


        if(!world_to_screen(
            end_world,
            view,
            end))
            continue;



        float alpha =
            std::clamp(
                r.life / 3.f,
                0.f,
                1.f);



        ImU32 col = IM_COL32(
            (int)(cfg::esp::rain_col.x * 255.f),
            (int)(cfg::esp::rain_col.y * 255.f),
            (int)(cfg::esp::rain_col.z * 255.f),
            (int)(cfg::esp::rain_col.w * 90.f * alpha)
        );



        // мягкая прозрачная тень
        draw->AddLine(
            start,
            end,
            IM_COL32(255,255,255,20),
            2.f
        );


        // сама тонкая капля
        draw->AddLine(
            start,
            end,
            col,
            1.f
        );

    }
}