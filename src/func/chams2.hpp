#pragma once

#include "../game/player.hpp"
#include "../game/math.hpp"
#include "../ui/cfg.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <algorithm>

// ================================================================
//  CHAMS — 8 types
//
//  type 0 — flat solid fill       (quad + ellipse, gradient)
//  type 1 — crystal               (faceted polys, highlight)
//  type 2 — outline only          (stroked edges, gradient)
//  type 3 — outline + fill        (stroke + transparent fill)
//  type 4 — wireframe             (NxM mesh grid + diagonals)
//  type 5 — glass                 (no user colour — light only)
//  type 6 — blur / ghost          (no user colour — layered soft halos)
//  type 7 — capsule               (rounded limbs, semi-transparent fill + gradient outline)
// ================================================================

namespace chams {

// ----------------------------------------------------------------
//  Colour helpers
// ----------------------------------------------------------------

static inline ImU32 grad_col(const ImVec4& top, const ImVec4& bot,
                              float y, float y_min, float y_max,
                              float alpha_scale)
{
    float t = (y_max > y_min) ? (y - y_min) / (y_max - y_min) : 0.f;
    t = std::clamp(t, 0.f, 1.f);
    t = t * t * (3.f - 2.f * t); // smoothstep
    return IM_COL32(
        (int)((top.x + (bot.x - top.x) * t) * 255),
        (int)((top.y + (bot.y - top.y) * t) * 255),
        (int)((top.z + (bot.z - top.z) * t) * 255),
        (int)((top.w + (bot.w - top.w) * t) * 255 * alpha_scale));
}

static inline ImU32 solid_col(const ImVec4& c, float a) {
    return IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255),(int)(c.w*255*a));
}

// ----------------------------------------------------------------
//  Shared geometry
// ----------------------------------------------------------------

static inline void quad_grad(ImDrawList* dl,
                              const ImVec2& p1, const ImVec2& p2,
                              const ImVec2& p3, const ImVec2& p4,
                              ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4)
{
    dl->PrimReserve(6, 4);
    ImVec2 uv = dl->_Data->TexUvWhitePixel;
    ImDrawIdx idx = (ImDrawIdx)dl->_VtxCurrentIdx;
    dl->PrimWriteVtx(p1, uv, c1); dl->PrimWriteVtx(p2, uv, c2);
    dl->PrimWriteVtx(p3, uv, c3); dl->PrimWriteVtx(p4, uv, c4);
    dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+1); dl->PrimWriteIdx(idx+2);
    dl->PrimWriteIdx(idx+0); dl->PrimWriteIdx(idx+2); dl->PrimWriteIdx(idx+3);
}

static inline void limb_corners(const ImVec2& a, const ImVec2& b, float hw,
                                  ImVec2& p1, ImVec2& p2, ImVec2& p3, ImVec2& p4)
{
    float dx = b.x-a.x, dy = b.y-a.y;
    float len = sqrtf(dx*dx+dy*dy); if (len < 0.01f) len = 0.01f;
    float nx = -dy/len, ny = dx/len;
    p1={a.x+nx*hw,a.y+ny*hw}; p2={a.x-nx*hw,a.y-ny*hw};
    p3={b.x-nx*hw,b.y-ny*hw}; p4={b.x+nx*hw,b.y+ny*hw};
}

// ----------------------------------------------------------------
//  TYPE 0 — flat solid, vertical gradient
// ----------------------------------------------------------------
static inline void limb_flat(ImDrawList* dl,
                               const ImVec2& a, const ImVec2& b, float hw,
                               const ImVec4& top, const ImVec4& bot,
                               float y_min, float y_max, float alpha)
{
    if (hw < 0.3f) return;
    if (sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)) < 0.5f) return;
    ImVec2 p1,p2,p3,p4; limb_corners(a,b,hw,p1,p2,p3,p4);
    quad_grad(dl,p1,p2,p3,p4,
        grad_col(top,bot,p1.y,y_min,y_max,alpha),
        grad_col(top,bot,p2.y,y_min,y_max,alpha),
        grad_col(top,bot,p3.y,y_min,y_max,alpha),
        grad_col(top,bot,p4.y,y_min,y_max,alpha));
}

static inline void head_flat(ImDrawList* dl, const ImVec2& sc,
                               float hrx, float hry,
                               const ImVec4& top, const ImVec4& bot,
                               float y_min, float y_max, float alpha)
{
    ImU32 fc = grad_col(top,bot,sc.y-hry*0.3f,y_min,y_max,alpha);
    dl->AddEllipseFilled(sc,hrx,hry,fc,0.f,20);
}

// ----------------------------------------------------------------
//  TYPE 1 — crystal
// ----------------------------------------------------------------
static inline void limb_crystal(ImDrawList* dl,
                                  const ImVec2& a, const ImVec2& b, float hw,
                                  const ImVec4& top, const ImVec4& bot,
                                  float y_min, float y_max, float alpha)
{
    if (hw < 0.3f) return;
    float dx=b.x-a.x,dy=b.y-a.y,len=sqrtf(dx*dx+dy*dy);
    if (len < 0.5f) return;
    float nx=-dy/len,ny=dx/len;
    ImVec2 rows[2][5];
    float offs[5]={-hw,-hw*.45f,0.f,hw*.45f,hw};
    for (int r=0;r<2;r++) {
        const ImVec2& bp=(r==0)?a:b;
        for (int c=0;c<5;c++) rows[r][c]={bp.x+nx*offs[c],bp.y+ny*offs[c]};
    }
    float bright[4]={0.35f,0.75f,1.0f,0.40f};
    for (int s=0;s<4;s++) {
        float bm=bright[s];
        auto mk=[&](const ImVec2& pt)->ImU32{
            ImVec4 ct={top.x*bm,top.y*bm,top.z*bm,top.w};
            ImVec4 cb={bot.x*bm,bot.y*bm,bot.z*bm,bot.w};
            return grad_col(ct,cb,pt.y,y_min,y_max,alpha);
        };
        quad_grad(dl,rows[0][s],rows[0][s+1],rows[1][s+1],rows[1][s],
                  mk(rows[0][s]),mk(rows[0][s+1]),mk(rows[1][s+1]),mk(rows[1][s]));
    }
    ImVec2 hl_a={a.x+nx*(-hw*.3f),a.y+ny*(-hw*.3f)};
    ImVec2 hl_b={b.x+nx*(-hw*.3f),b.y+ny*(-hw*.3f)};
    dl->AddLine(hl_a,hl_b,IM_COL32(255,255,255,(int)(60*alpha)),1.f);
}

static inline void head_crystal(ImDrawList* dl, const ImVec2& sc,
                                  float hrx, float hry,
                                  const ImVec4& top, const ImVec4& bot,
                                  float y_min, float y_max, float alpha)
{
    const int N=8; ImVec2 pts[N];
    for (int i=0;i<N;i++){float ang=IM_PI*2.f*i/N+IM_PI/N;pts[i]={sc.x+cosf(ang)*hrx,sc.y+sinf(ang)*hry};}
    for (int i=0;i<N;i++){
        int j=(i+1)%N;
        float bm=0.5f+0.5f*fabsf(cosf(IM_PI*2.f*i/N));
        ImVec4 ct={top.x*bm,top.y*bm,top.z*bm,top.w},cb={bot.x*bm,bot.y*bm,bot.z*bm,bot.w};
        ImU32 ci=grad_col(ct,cb,sc.y,y_min,y_max,alpha);
        dl->AddTriangleFilled(sc,pts[i],pts[j],ci);
    }
    ImU32 hc=grad_col(top,bot,sc.y-hry*.4f,y_min,y_max,alpha*.6f);
    dl->AddCircleFilled(sc,hrx*.35f,hc,8);
    dl->AddLine({sc.x-hrx*.3f,sc.y-hry*.6f},{sc.x+hrx*.1f,sc.y-hry*.2f},
                IM_COL32(255,255,255,(int)(80*alpha)),1.2f);
}

// ----------------------------------------------------------------
//  TYPE 2 — outline only
// ----------------------------------------------------------------
static inline void limb_outline(ImDrawList* dl,
                                  const ImVec2& a, const ImVec2& b, float hw,
                                  const ImVec4& top, const ImVec4& bot,
                                  float y_min, float y_max, float alpha, float thick)
{
    if (hw < 0.3f) return;
    if (sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)) < 0.5f) return;
    ImVec2 p1,p2,p3,p4; limb_corners(a,b,hw,p1,p2,p3,p4);
    auto ec=[&](const ImVec2& p){return grad_col(top,bot,p.y,y_min,y_max,alpha);};
    dl->AddLine(p1,p4,ec({(p1.x+p4.x)*.5f,(p1.y+p4.y)*.5f}),thick);
    dl->AddLine(p2,p3,ec({(p2.x+p3.x)*.5f,(p2.y+p3.y)*.5f}),thick);
    dl->AddLine(p1,p2,ec(a),thick);
    dl->AddLine(p4,p3,ec(b),thick);
}

static inline void head_outline(ImDrawList* dl, const ImVec2& sc,
                                  float hrx, float hry,
                                  const ImVec4& top, const ImVec4& bot,
                                  float y_min, float y_max, float alpha, float thick)
{
    const int N=24;
    for (int i=0;i<N;i++){
        float a0=IM_PI*2.f*i/N, a1=IM_PI*2.f*(i+1)/N;
        ImVec2 pa={sc.x+cosf(a0)*hrx,sc.y+sinf(a0)*hry};
        ImVec2 pb={sc.x+cosf(a1)*hrx,sc.y+sinf(a1)*hry};
        dl->AddLine(pa,pb,grad_col(top,bot,(pa.y+pb.y)*.5f,y_min,y_max,alpha),thick);
    }
}

// ----------------------------------------------------------------
//  TYPE 4 — wireframe
// ----------------------------------------------------------------
static inline void limb_wireframe(ImDrawList* dl,
                                   const ImVec2& a, const ImVec2& b, float hw,
                                   const ImVec4& top, const ImVec4& bot,
                                   float y_min, float y_max, float alpha, float thick)
{
    if (hw < 0.3f) return;
    float dx=b.x-a.x,dy=b.y-a.y,len=sqrtf(dx*dx+dy*dy);
    if (len < 0.5f) return;
    float nx=-dy/len,ny=dx/len;
    const int NC=3,NR=4;
    ImVec2 grid[NR+1][NC+1];
    for (int r=0;r<=NR;r++){
        float tr=(float)r/NR;
        ImVec2 ctr={a.x+(b.x-a.x)*tr,a.y+(b.y-a.y)*tr};
        for (int c=0;c<=NC;c++){float tc=(float)c/NC-.5f;grid[r][c]={ctr.x+nx*hw*tc*2.f,ctr.y+ny*hw*tc*2.f};}
    }
    auto gc=[&](const ImVec2& p){return grad_col(top,bot,p.y,y_min,y_max,alpha);};
    for (int r=0;r<=NR;r++) for (int c=0;c<NC;c++)
        dl->AddLine(grid[r][c],grid[r][c+1],gc({(grid[r][c].x+grid[r][c+1].x)*.5f,(grid[r][c].y+grid[r][c+1].y)*.5f}),thick);
    for (int r=0;r<NR;r++) for (int c=0;c<=NC;c++)
        dl->AddLine(grid[r][c],grid[r+1][c],gc({(grid[r][c].x+grid[r+1][c].x)*.5f,(grid[r][c].y+grid[r+1][c].y)*.5f}),thick);
    for (int r=0;r<NR;r++) for (int c=0;c<NC;c++)
        dl->AddLine(grid[r][c],grid[r+1][c+1],gc({(grid[r][c].x+grid[r+1][c+1].x)*.5f,(grid[r][c].y+grid[r+1][c+1].y)*.5f}),thick*.6f);
}

static inline void head_wireframe(ImDrawList* dl, const ImVec2& sc,
                                    float hrx, float hry,
                                    const ImVec4& top, const ImVec4& bot,
                                    float y_min, float y_max, float alpha, float thick)
{
    const int NLAT=5,NLON=10;
    auto gc=[&](const ImVec2& p){return grad_col(top,bot,p.y,y_min,y_max,alpha);};
    for (int la=1;la<NLAT;la++){
        float phi=IM_PI*la/NLAT;
        for (int lo=0;lo<NLON;lo++){
            float a0=IM_PI*2.f*lo/NLON, a1=IM_PI*2.f*(lo+1)/NLON;
            ImVec2 pa={sc.x+hrx*sinf(phi)*cosf(a0),sc.y+hry*(1.f-2.f*(float)la/NLAT)};
            ImVec2 pb={sc.x+hrx*sinf(phi)*cosf(a1),sc.y+hry*(1.f-2.f*(float)la/NLAT)};
            dl->AddLine(pa,pb,gc({(pa.x+pb.x)*.5f,(pa.y+pb.y)*.5f}),thick);
        }
    }
    for (int lo=0;lo<NLON;lo++){
        float ang=IM_PI*2.f*lo/NLON;
        ImVec2 prev={sc.x+hrx*cosf(ang),sc.y-hry};
        for (int la=1;la<=NLAT;la++){
            float phi=IM_PI*(float)la/NLAT;
            ImVec2 cur={sc.x+hrx*sinf(phi)*cosf(ang),sc.y+hry*(-cosf(phi))};
            dl->AddLine(prev,cur,gc({(prev.x+cur.x)*.5f,(prev.y+cur.y)*.5f}),thick);
            prev=cur;
        }
    }
}

// ----------------------------------------------------------------
//  TYPE 5 — glass  (no user colour)
//
//  Layers (back → front):
//   1. dark semi-transparent body fill   — depth / volume
//   2. thin bright rim on left edge      — specular rim light
//   3. wide bright streak top-left       — primary highlight
//   4. narrow white streak centre        — fresnel hotspot
//   5. stroked outline, white low-alpha  — glass edge
// ----------------------------------------------------------------
static inline void limb_glass(ImDrawList* dl,
                                const ImVec2& a, const ImVec2& b, float hw,
                                float alpha)
{
    if (hw < 0.3f) return;
    float dx=b.x-a.x, dy=b.y-a.y, len=sqrtf(dx*dx+dy*dy);
    if (len < 0.5f) return;
    float nx=-dy/len, ny=dx/len;
    // axis-aligned "up" is screen-up (−y)
    ImVec2 p1={a.x+nx*hw,a.y+ny*hw}, p2={a.x-nx*hw,a.y-ny*hw};
    ImVec2 p3={b.x-nx*hw,b.y-ny*hw}, p4={b.x+nx*hw,b.y+ny*hw};

    // 1. body — very dark blue-tinted fill
    ImU32 body = IM_COL32(20,30,50,(int)(90*alpha));
    dl->AddQuadFilled(p1,p2,p3,p4,body);

    // 2. left rim specular — bright thin vertical strip on p2/p3 side
    ImVec2 rim_a={p2.x+nx*hw*.15f,p2.y+ny*hw*.15f};
    ImVec2 rim_b={p3.x+nx*hw*.15f,p3.y+ny*hw*.15f};
    quad_grad(dl, p2, rim_a, rim_b, p3,
              IM_COL32(180,200,255,(int)(120*alpha)),
              IM_COL32(180,200,255,(int)(40*alpha)),
              IM_COL32(180,200,255,(int)(40*alpha)),
              IM_COL32(180,200,255,(int)(120*alpha)));

    // 3. primary highlight streak — top-left third of limb
    ImVec2 h1a={a.x+nx*hw*.55f,a.y+ny*hw*.55f};
    ImVec2 h1b={a.x+nx*hw*.05f,a.y+ny*hw*.05f};
    ImVec2 h1c={b.x+nx*hw*.05f,b.y+ny*hw*.05f};
    ImVec2 h1d={b.x+nx*hw*.55f,b.y+ny*hw*.55f};
    quad_grad(dl, h1a, h1b, h1c, h1d,
              IM_COL32(220,235,255,(int)(100*alpha)),
              IM_COL32(220,235,255,(int)(15*alpha)),
              IM_COL32(220,235,255,(int)(15*alpha)),
              IM_COL32(220,235,255,(int)(100*alpha)));

    // 4. fresnel hotspot — 1-px bright line near left edge
    dl->AddLine({p2.x+nx*hw*.35f,p2.y+ny*hw*.35f},
                {p3.x+nx*hw*.35f,p3.y+ny*hw*.35f},
                IM_COL32(255,255,255,(int)(160*alpha)), 1.2f);

    // 5. glass edge stroke
    dl->AddLine(p1,p4,IM_COL32(200,220,255,(int)(100*alpha)),1.f);
    dl->AddLine(p2,p3,IM_COL32(200,220,255,(int)(100*alpha)),1.f);
    dl->AddLine(p1,p2,IM_COL32(200,220,255,(int)(60*alpha)),1.f);
    dl->AddLine(p4,p3,IM_COL32(200,220,255,(int)(60*alpha)),1.f);
}

static inline void head_glass(ImDrawList* dl, const ImVec2& sc,
                                float hrx, float hry, float alpha)
{
    // 1. body
    dl->AddEllipseFilled(sc,hrx,hry,IM_COL32(20,30,50,(int)(90*alpha)),0.f,20);

    // 2. full rim outline
    dl->AddEllipse(sc,hrx,hry,IM_COL32(200,220,255,(int)(100*alpha)),0.f,24,1.f);

    // 3. primary highlight — upper-left arc  (angles ~200°..310° in screen space)
    const int NA=14;
    float a_start=-2.4f, a_end=-0.9f;
    ImVec2 prev={sc.x+cosf(a_start)*hrx*0.75f, sc.y+sinf(a_start)*hry*0.75f};
    for (int i=1;i<=NA;i++){
        float ang=a_start+(a_end-a_start)*(float)i/NA;
        ImVec2 cur={sc.x+cosf(ang)*hrx*0.75f, sc.y+sinf(ang)*hry*0.75f};
        float t=(float)i/NA;
        int a_val=(int)(180*(1.f-fabsf(t-.5f)*2.f)*alpha);
        dl->AddLine(prev,cur,IM_COL32(220,235,255,a_val),hrx*.18f);
        prev=cur;
    }

    // 4. fresnel hotspot — small bright circle top-left
    ImVec2 hl={sc.x-hrx*.4f, sc.y-hry*.5f};
    dl->AddCircleFilled(hl,hrx*.22f,IM_COL32(255,255,255,(int)(120*alpha)),10);
    dl->AddCircleFilled(hl,hrx*.10f,IM_COL32(255,255,255,(int)(200*alpha)),8);
}

// ----------------------------------------------------------------
//  TYPE 6 — blur / ghost  (no user colour)
//
//  Concentric expanding halos with decreasing alpha and increasing
//  size — fakes a screen-space gaussian blur impression.
//  Neutral desaturated white, alpha controlled by transparency.
// ----------------------------------------------------------------
static inline void limb_blur(ImDrawList* dl,
                               const ImVec2& a, const ImVec2& b, float hw,
                               float alpha)
{
    if (hw < 0.3f) return;
    if (sqrtf((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)) < 0.5f) return;

    // 4 passes: innermost opaque-ish, outermost nearly invisible
    const int PASSES=4;
    float hw_mul[PASSES]  = {1.0f, 1.55f, 2.2f, 3.1f};
    float alpha_mul[PASSES]= {0.45f,0.22f, 0.10f,0.04f};

    for (int p=0;p<PASSES;p++){
        float phw=hw*hw_mul[p];
        int   pa =(int)(255*alpha*alpha_mul[p]);
        ImU32 col=IM_COL32(200,210,230,pa);
        ImVec2 p1,p2,p3,p4; limb_corners(a,b,phw,p1,p2,p3,p4);
        dl->AddQuadFilled(p1,p2,p3,p4,col);
    }
    // tight bright core
    ImVec2 p1,p2,p3,p4; limb_corners(a,b,hw*.35f,p1,p2,p3,p4);
    dl->AddQuadFilled(p1,p2,p3,p4,IM_COL32(240,245,255,(int)(200*alpha)));
}

static inline void head_blur(ImDrawList* dl, const ImVec2& sc,
                               float hrx, float hry, float alpha)
{
    const int PASSES=4;
    float mul[PASSES]   ={1.0f,1.55f,2.2f,3.1f};
    float amul[PASSES]  ={0.45f,0.22f,0.10f,0.04f};
    for (int p=0;p<PASSES;p++){
        int pa=(int)(255*alpha*amul[p]);
        dl->AddEllipseFilled(sc,hrx*mul[p],hry*mul[p],IM_COL32(200,210,230,pa),0.f,20);
    }
    // core
    dl->AddEllipseFilled(sc,hrx*.4f,hry*.4f,IM_COL32(240,245,255,(int)(200*alpha)),0.f,12);
}

// ----------------------------------------------------------------
//  TYPE 7 — capsule: semi-transparent fill + gradient outline
//
//  Each limb is a filled capsule (rounded rect):
//   – fill:    col/col_gradient with fill_alpha applied
//   – outline: col/col_gradient at full alpha, outline_w thick
//  Head is a proper ellipse capsule (same treatment).
// ----------------------------------------------------------------
static inline void limb_capsule(ImDrawList* dl,
                                  const ImVec2& a, const ImVec2& b, float hw,
                                  const ImVec4& top, const ImVec4& bot,
                                  float y_min, float y_max,
                                  float alpha, float fa, float thick)
{
    if (hw < 0.3f) return;
    float dx=b.x-a.x, dy=b.y-a.y, len=sqrtf(dx*dx+dy*dy);
    if (len < 0.5f) return;
    float nx=-dy/len, ny=dx/len;

    // Side quad corners
    ImVec2 p1,p2,p3,p4;
    limb_corners(a,b,hw,p1,p2,p3,p4);

    // ── fill: side quad ──
    quad_grad(dl,p1,p2,p3,p4,
        grad_col(top,bot,p1.y,y_min,y_max,alpha*fa),
        grad_col(top,bot,p2.y,y_min,y_max,alpha*fa),
        grad_col(top,bot,p3.y,y_min,y_max,alpha*fa),
        grad_col(top,bot,p4.y,y_min,y_max,alpha*fa));

    // ── fill: semicircle caps ──
    const int NS=10;
    float ang_a = atan2f(-ny,-nx); // cap at 'a' points away from b
    float ang_b = atan2f( ny, nx); // cap at 'b'
    auto fill_cap=[&](const ImVec2& ctr, float base_ang){
        ImVec2 prev={ctr.x+cosf(base_ang)*hw, ctr.y+sinf(base_ang)*hw};
        for (int i=1;i<=NS;i++){
            float ang=base_ang+IM_PI*(float)i/NS;
            ImVec2 cur={ctr.x+cosf(ang)*hw, ctr.y+sinf(ang)*hw};
            ImU32 fc=grad_col(top,bot,(ctr.y+prev.y+cur.y)/3.f,y_min,y_max,alpha*fa);
            dl->AddTriangleFilled(ctr,prev,cur,fc);
            prev=cur;
        }
    };
    fill_cap(a, ang_a);
    fill_cap(b, ang_b);

    // ── outline: side lines ──
    auto oc=[&](const ImVec2& p){return grad_col(top,bot,p.y,y_min,y_max,alpha);};
    dl->AddLine(p1,p4,oc({(p1.x+p4.x)*.5f,(p1.y+p4.y)*.5f}),thick);
    dl->AddLine(p2,p3,oc({(p2.x+p3.x)*.5f,(p2.y+p3.y)*.5f}),thick);

    // ── outline: semicircle caps ──
    auto outline_cap=[&](const ImVec2& ctr, float base_ang){
        ImVec2 prev={ctr.x+cosf(base_ang)*hw,ctr.y+sinf(base_ang)*hw};
        for (int i=1;i<=NS;i++){
            float ang=base_ang+IM_PI*(float)i/NS;
            ImVec2 cur={ctr.x+cosf(ang)*hw,ctr.y+sinf(ang)*hw};
            ImU32 c=grad_col(top,bot,(prev.y+cur.y)*.5f,y_min,y_max,alpha);
            dl->AddLine(prev,cur,c,thick);
            prev=cur;
        }
    };
    outline_cap(a, ang_a);
    outline_cap(b, ang_b);
}

static inline void head_capsule(ImDrawList* dl, const ImVec2& sc,
                                  float hrx, float hry,
                                  const ImVec4& top, const ImVec4& bot,
                                  float y_min, float y_max,
                                  float alpha, float fa, float thick)
{
    // fill
    const int N=24;
    ImVec2 pts[N];
    for (int i=0;i<N;i++){float a=IM_PI*2.f*i/N;pts[i]={sc.x+cosf(a)*hrx,sc.y+sinf(a)*hry};}
    for (int i=0;i<N;i++){
        int j=(i+1)%N;
        float my=(sc.y+pts[i].y+pts[j].y)/3.f;
        dl->AddTriangleFilled(sc,pts[i],pts[j],grad_col(top,bot,my,y_min,y_max,alpha*fa));
    }
    // outline
    for (int i=0;i<N;i++){
        int j=(i+1)%N;
        ImU32 c=grad_col(top,bot,(pts[i].y+pts[j].y)*.5f,y_min,y_max,alpha);
        dl->AddLine(pts[i],pts[j],c,thick);
    }
}


// ----------------------------------------------------------------
// TYPE 7 body-only outline: single thin silhouette line.
// Does not outline individual capsules/limbs.
// ----------------------------------------------------------------
static inline void body_silhouette(ImDrawList* dl,
                                   const std::vector<ImVec2>& pts,
                                   const ImVec4& col)
{
    if (pts.size() < 3) return;
    std::vector<ImVec2> hull;
    hull.reserve(pts.size());

    // simple screen-space convex hull
    std::vector<ImVec2> p = pts;
    std::sort(p.begin(), p.end(), [](const ImVec2& a, const ImVec2& b) {
        return a.x == b.x ? a.y < b.y : a.x < b.x;
    });

    auto cross=[](const ImVec2& a,const ImVec2& b,const ImVec2& c){
        return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    };

    for (const auto& v : p) {
        while (hull.size() >= 2 && cross(hull[hull.size()-2], hull.back(), v) <= 0)
            hull.pop_back();
        hull.push_back(v);
    }
    size_t lower = hull.size();
    for (int i=(int)p.size()-2;i>=0;i--) {
        const auto& v=p[i];
        while (hull.size()>lower && cross(hull[hull.size()-2], hull.back(), v) <= 0)
            hull.pop_back();
        hull.push_back(v);
    }
    if (hull.size() > 1) hull.pop_back();

    dl->AddPolyline(hull.data(), (int)hull.size(),
        IM_COL32((int)(col.x*255),(int)(col.y*255),(int)(col.z*255),(int)(col.w*255)),
        ImDrawFlags_Closed, 1.0f);
}

// ----------------------------------------------------------------
//  TYPE 8 — Glow outline fused with solid
//
//  Solid fill (type 0 внутри) + многослойный glow-спрайт по периметру
//  каждого лимба. Солид у краёв затухает плавно (feathered edge) так
//  что fill и glow визуально сливаются без жёсткой границы.
//
//  glow.png — белая gauss-blob текстура, лежит в src/ui/.
//  Тинтируется через top/bot градиент каждой точки силуэта.
//
//  Для каждого лимба AB генерируем N sprite-квадов по длине лимба,
//  каждый quad = одна glow.png, ориентирована вдоль AB и масштабирована
//  по толщине лимба × glow_scale.
// ----------------------------------------------------------------

extern ImTextureID glow_tex; // src/ui/glow.png — defined in globals.cpp

// Один glow-спрайт вдоль кости: квад с UV (0,0)→(1,1)
// ориентирован по направлению a→b, центрирован в mid, размер sz×sz
static inline void glow_sprite(ImDrawList* dl,
                                const ImVec2& mid,
                                float ang,          // угол лимба в радианах
                                float sz,
                                ImU32 tint)
{
    if (!glow_tex || sz < 0.5f) return;
    float ca = cosf(ang), sa = sinf(ang);
    // Четыре угла квада: вдоль лимба (along) × поперёк (perp)
    float hx = sz * 0.5f;
    ImVec2 along = { ca * hx,  sa * hx };
    ImVec2 perp  = {-sa * hx,  ca * hx };
    ImVec2 v0 = { mid.x - along.x - perp.x, mid.y - along.y - perp.y };
    ImVec2 v1 = { mid.x + along.x - perp.x, mid.y + along.y - perp.y };
    ImVec2 v2 = { mid.x + along.x + perp.x, mid.y + along.y + perp.y };
    ImVec2 v3 = { mid.x - along.x + perp.x, mid.y - along.y + perp.y };
    dl->AddImageQuad(glow_tex, v0, v1, v2, v3,
                     ImVec2(0,0), ImVec2(1,0), ImVec2(1,1), ImVec2(0,1),
                     tint);
}

// Feathered solid: solids fill с alpha=0 у самых краёв
// Достигается через quad_grad — угловые вершины темнее центральных
static inline void limb_glow_solid(ImDrawList* dl,
                                    const ImVec2& a, const ImVec2& b, float hw,
                                    const ImVec4& top, const ImVec4& bot,
                                    float y_min, float y_max, float alpha)
{
    if (hw < 0.3f) return;
    float dx=b.x-a.x, dy=b.y-a.y;
    float len=sqrtf(dx*dx+dy*dy); if(len<0.5f) return;
    float nx=-dy/len, ny=dx/len;

    // Centre line points (alpha=1.0 × alpha)
    // Edge points (alpha=0.0 — feathered)
    float edge_a = 0.0f;  // края полностью прозрачные
    float mid_a  = alpha; // центр непрозрачный

    // Три полосы: edge / center / edge
    ImVec2 e0a={a.x+nx*hw,    a.y+ny*hw};
    ImVec2 c0a={a.x+nx*hw*.4f,a.y+ny*hw*.4f};
    ImVec2 cxa={a.x-nx*hw*.4f,a.y-ny*hw*.4f};
    ImVec2 e1a={a.x-nx*hw,    a.y-ny*hw};
    ImVec2 e0b={b.x+nx*hw,    b.y+ny*hw};
    ImVec2 c0b={b.x+nx*hw*.4f,b.y+ny*hw*.4f};
    ImVec2 cxb={b.x-nx*hw*.4f,b.y-ny*hw*.4f};
    ImVec2 e1b={b.x-nx*hw,    b.y-ny*hw};

    auto gc=[&](const ImVec2& p, float a_mul){
        return grad_col(top,bot,p.y,y_min,y_max,a_mul);
    };
    // Strip 1: edge→centre
    quad_grad(dl, e0a, c0a, c0b, e0b,
              gc(e0a,edge_a), gc(c0a,mid_a), gc(c0b,mid_a), gc(e0b,edge_a));
    // Strip 2: centre (full width)
    quad_grad(dl, c0a, cxa, cxb, c0b,
              gc(c0a,mid_a), gc(cxa,mid_a), gc(cxb,mid_a), gc(c0b,mid_a));
    // Strip 3: centre→edge
    quad_grad(dl, cxa, e1a, e1b, cxb,
              gc(cxa,mid_a), gc(e1a,edge_a), gc(e1b,edge_a), gc(cxb,mid_a));
}

static inline void limb_glow8(ImDrawList* dl,
                                const ImVec2& a, const ImVec2& b, float hw,
                                const ImVec4& top, const ImVec4& bot,
                                float y_min, float y_max, float alpha,
                                float glow_scale)
{
    if (hw < 0.3f) return;
    float dx=b.x-a.x, dy=b.y-a.y;
    float len=sqrtf(dx*dx+dy*dy); if(len<0.5f) return;
    float ang=atan2f(dy,dx);

    // ── 1. Feathered solid (слой снизу) ──────────────────────────────────────
    limb_glow_solid(dl,a,b,hw,top,bot,y_min,y_max,alpha*0.85f);

    // ── 2. Glow слои по периметру ─────────────────────────────────────────────
    // Три слоя убывающей прозрачности и нарастающего размера (внешний мягче)
    struct GlowLayer { float sz_mul; float a_mul; };
    const GlowLayer layers[] = {
        { 2.6f, 0.22f },   // внешний — широкий, прозрачный
        { 1.8f, 0.42f },   // средний
        { 1.1f, 0.70f },   // внутренний — узкий, яркий
    };

    // Спрайты расставляем вдоль лимба с шагом hw*0.8
    float step = hw * 0.8f; if(step < 1.f) step = 1.f;
    int n_sprites = std::max(1, (int)(len / step) + 1);

    for (const auto& lyr : layers) {
        float sprite_sz = hw * 2.f * lyr.sz_mul * glow_scale;
        for (int i = 0; i <= n_sprites; i++) {
            float t  = (n_sprites > 0) ? (float)i / (float)n_sprites : 0.f;
            ImVec2 mid = { a.x + dx*t, a.y + dy*t };
            float  my  = mid.y;

            // Тинт = градиент top→bot в этой точке
            float gt = (y_max > y_min) ? (my - y_min) / (y_max - y_min) : 0.f;
            gt = std::clamp(gt,0.f,1.f);
            gt = gt*gt*(3.f-2.f*gt);
            int r = (int)((top.x + (bot.x-top.x)*gt)*255);
            int g = (int)((top.y + (bot.y-top.y)*gt)*255);
            int bv= (int)((top.z + (bot.z-top.z)*gt)*255);
            int av= (int)(alpha * lyr.a_mul * 255);

            // Края лимба (t≈0 или t≈1) плавно угасают
            float edge_fade = sinf(t * IM_PI); // 0→1→0
            av = (int)(av * (0.3f + 0.7f * edge_fade));

            glow_sprite(dl, mid, ang, sprite_sz, IM_COL32(r,g,bv,av));
        }
    }
}

static inline void head_glow8(ImDrawList* dl, const ImVec2& sc,
                                float hrx, float hry,
                                const ImVec4& top, const ImVec4& bot,
                                float y_min, float y_max, float alpha,
                                float glow_scale)
{
    // Solid с feathered edge
    const int N=24;
    ImVec2 pts[N];
    for (int i=0;i<N;i++){
        float a=IM_PI*2.f*i/N;
        pts[i]={sc.x+cosf(a)*hrx,sc.y+sinf(a)*hry};
    }
    // Filled fan (centre→rim, fade at rim)
    for (int i=0;i<N;i++){
        int j=(i+1)%N;
        float my=(sc.y+pts[i].y+pts[j].y)/3.f;
        ImU32 cc = grad_col(top,bot,sc.y,y_min,y_max,alpha*0.85f);
        ImU32 ec = grad_col(top,bot,my,y_min,y_max,0.f); // прозрачный на краях
        // треугольник: centre→pts[i]→pts[j], lerp centre/edge
        dl->AddTriangleFilled(sc, pts[i], pts[j],
            // используем центровой цвет — достаточно для feathered look
            cc);
        (void)ec;
    }

    // Glow кольцо вокруг головы
    struct GlowLayer { float sz_mul; float a_mul; };
    const GlowLayer layers[] = {
        { 3.0f, 0.18f },
        { 2.0f, 0.38f },
        { 1.2f, 0.65f },
    };
    for (const auto& lyr : layers) {
        float sprite_sz = hrx * lyr.sz_mul * glow_scale;
        const int NS = 16;
        for (int i = 0; i < NS; i++) {
            float a = IM_PI * 2.f * i / NS;
            ImVec2 mid = { sc.x + cosf(a)*hrx*0.85f, sc.y + sinf(a)*hry*0.85f };
            float  my  = mid.y;
            float gt = (y_max > y_min) ? (my - y_min)/(y_max - y_min) : 0.f;
            gt = std::clamp(gt,0.f,1.f);
            gt = gt*gt*(3.f-2.f*gt);
            int r=(int)((top.x+(bot.x-top.x)*gt)*255);
            int g=(int)((top.y+(bot.y-top.y)*gt)*255);
            int bv=(int)((top.z+(bot.z-top.z)*gt)*255);
            int av=(int)(alpha*lyr.a_mul*255);
            glow_sprite(dl, mid, a + IM_PI*0.5f, sprite_sz, IM_COL32(r,g,bv,av));
        }
    }
}

// ----------------------------------------------------------------
//  Main draw entry
// ----------------------------------------------------------------
inline void draw_chams(uint64_t player, const matrix& vm)
{
    if (!cfg::chams::enabled) return;
    float alpha = cfg::chams::transparency;
    if (alpha < 0.01f) return;

    player::bones_t b;
    if (!player::get_bones(player, b)) return;

    ImDrawList* dl  = ImGui::GetBackgroundDrawList();
    int   type      = cfg::chams::type;
    float thick     = cfg::chams::outline_w;
    float fa        = cfg::chams::fill_alpha;
    const ImVec4& top = cfg::chams::col;
    const ImVec4& bot = cfg::chams::col_gradient;

    struct Pt { ImVec2 s; bool ok; };
    auto proj=[&](const Vector3& w)->Pt{ Pt p; p.ok=world_to_screen(w,vm,p.s); return p; };

    auto P_head   =proj(b.head);   auto P_neck   =proj(b.neck);
    auto P_sp2    =proj(b.spine2); auto P_sp1    =proj(b.spine1);
    auto P_sp     =proj(b.spine);  auto P_hip    =proj(b.pelvis);
    auto P_lsh    =proj(b.l_shoulder); auto P_larm=proj(b.l_arm);
    auto P_lfore  =proj(b.l_forearm);  auto P_lhand=proj(b.l_hand);
    auto P_rsh    =proj(b.r_shoulder); auto P_rarm=proj(b.r_arm);
    auto P_rfore  =proj(b.r_forearm);  auto P_rhand=proj(b.r_hand);
    auto P_lthigh =proj(b.l_thigh); auto P_lknee=proj(b.l_knee);
    auto P_lfoot  =proj(b.l_foot);
    auto P_rthigh =proj(b.r_thigh); auto P_rknee=proj(b.r_knee);
    auto P_rfoot  =proj(b.r_foot);

    float ph=0.f;
    if (P_head.ok&&P_hip.ok){
        float hdx=P_head.s.x-P_hip.s.x,hdy=P_head.s.y-P_hip.s.y;
        ph=sqrtf(hdx*hdx+hdy*hdy);
    }
    if (ph<1.f) return;

    float base=ph*.12f, tw=base*3.f, aw=base*1.3f, lw=base*1.8f;

    float y_min=P_head.ok?P_head.s.y:0.f;
    float y_max=P_hip.ok?P_hip.s.y:y_min+ph;
    if (P_lfoot.ok) y_max=std::max(y_max,P_lfoot.s.y);
    if (P_rfoot.ok) y_max=std::max(y_max,P_rfoot.s.y);

    struct L{const Pt*a;const Pt*b;float hw;};
    const L limbs[]={
        {&P_neck,&P_sp2,tw},{&P_sp2,&P_sp1,tw*.85f},{&P_sp1,&P_sp,tw*.75f},{&P_sp,&P_hip,tw*.8f},
        {&P_neck,&P_lsh,aw*1.2f},{&P_neck,&P_rsh,aw*1.2f},
        {&P_lsh,&P_larm,aw},{&P_larm,&P_lfore,aw*.85f},{&P_lfore,&P_lhand,aw*.7f},
        {&P_rsh,&P_rarm,aw},{&P_rarm,&P_rfore,aw*.85f},{&P_rfore,&P_rhand,aw*.7f},
        {&P_hip,&P_lthigh,lw*.8f},{&P_lthigh,&P_lknee,lw},{&P_lknee,&P_lfoot,lw*.7f},
        {&P_hip,&P_rthigh,lw*.8f},{&P_rthigh,&P_rknee,lw},{&P_rknee,&P_rfoot,lw*.7f},
    };
    constexpr int NL=(int)(sizeof(limbs)/sizeof(limbs[0]));

    float glow_scale = cfg::chams::glow_scale; // [0.5, 3.0], default 1.0

    for (int i=0;i<NL;i++){
        const L& l=limbs[i];
        if (!l.a->ok||!l.b->ok) continue;
        const ImVec2& sa=l.a->s; const ImVec2& sb=l.b->s; float hw=l.hw;
        switch(type){
        case 0: limb_flat    (dl,sa,sb,hw,top,bot,y_min,y_max,alpha);        break;
        case 1: limb_crystal (dl,sa,sb,hw,top,bot,y_min,y_max,alpha);        break;
        case 2: limb_outline (dl,sa,sb,hw,top,bot,y_min,y_max,alpha,thick);  break;
        case 3: limb_flat    (dl,sa,sb,hw,top,bot,y_min,y_max,alpha*fa);
                limb_outline (dl,sa,sb,hw,top,bot,y_min,y_max,alpha,thick);  break;
        case 4: limb_wireframe(dl,sa,sb,hw,top,bot,y_min,y_max,alpha,thick); break;
        case 5: limb_glass   (dl,sa,sb,hw,alpha);                            break;
        case 6: limb_blur    (dl,sa,sb,hw,alpha);                            break;
        case 7: limb_capsule (dl,sa,sb,hw,top,bot,y_min,y_max,alpha,fa,0.0f); break;
        case 8: limb_glow8   (dl,sa,sb,hw,top,bot,y_min,y_max,alpha,glow_scale); break;
        }
    }

    if (P_head.ok){
        float hrx=base*1.8f,hry=base*2.5f;
        switch(type){
        case 0: head_flat     (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha);       break;
        case 1: head_crystal  (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha);       break;
        case 2: head_outline  (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha,thick); break;
        case 3: head_flat     (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha*fa);
                head_outline  (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha,thick); break;
        case 4: head_wireframe(dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha,thick); break;
        case 5: head_glass    (dl,P_head.s,hrx,hry,alpha);                           break;
        case 6: head_blur     (dl,P_head.s,hrx,hry,alpha);                           break;
        case 7: head_capsule  (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha,fa,0.0f); break;
        case 8: head_glow8    (dl,P_head.s,hrx,hry,top,bot,y_min,y_max,alpha,glow_scale); break;
        }
    }

    if (type == 7) {
        std::vector<ImVec2> bodyPts;
        const Pt* all[] = {
            &P_head,&P_neck,&P_lsh,&P_rsh,&P_lhand,&P_rhand,
            &P_lfoot,&P_rfoot,&P_lthigh,&P_rthigh,&P_hip
        };
        for (auto* p : all)
            if (p->ok) bodyPts.push_back(p->s);

        ImVec4 lineCol = top;
        lineCol.w *= 0.55f; // less visible than old capsule outlines
        body_silhouette(dl, bodyPts, lineCol);
    }
}

} // namespace chams
