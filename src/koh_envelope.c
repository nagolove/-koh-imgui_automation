#include "koh_envelope.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

typedef struct Envelope {
    Vector2         *points;
    int             points_num, poins_cap;
    RenderTexture2D tex;
} Envelope;

// TODO: Перенести в структуру как изменямую величину
static const int handle_size = 5;

// TODO: Должна находить либо кружок под курсором или соседей кружка.
Vector2 *points_process(Envelope *env, Vector2 mouse_pos) {
    for (int i = 0; i < env->points_num; i++) {
        Vector2 p = env->points[i];
        if (CheckCollisionPointCircle(mouse_pos, p, handle_size)) {
            return &env->points[i];
        }
    }
    return NULL;
}

void env_draw_imgui(Envelope_t e) {
    ImVec2 img_min = {}, img_max = {};

    BeginTextureMode(e->tex);
    ClearBackground(GRAY);
    EndTextureMode();

    //igPushItemFlag(ImGuiItemFlags_Disabled, true);
    //igButton("Disabled Button", (ImVec2){});
    //igBeginDisabled(true);
    rlImGuiImage(&e->tex.texture);
    /*igEndDisabled();*/
    /*igPopItemFlag();*/

    igGetItemRectMin(&img_min);
    igGetItemRectMax(&img_max);

    Vector2 mp = GetMousePosition();

    if (mp.x >= img_min.x && mp.x <= img_max.x && 
            mp.y >= img_min.y && mp.y <= img_max.y) {
        ImGuiWindow *wnd = igGetCurrentWindow();
        ImVec2 sz = {
            img_max.x - img_min.x,
            img_max.y - img_min.y,
        };
        igSetWindowHitTestHole(wnd, img_min, sz);

        /*
           trace(
           "render_gui: area %s with size %s disabled\n",
           Vector2_tostr(Im2Vec2(img_min)), 
           Vector2_tostr(Im2Vec2(sz))
           );
           */


    }

}


Envelope_t env_new() {
    Envelope_t e = calloc(1, sizeof(*e));
    e->tex = LoadRenderTexture(1200, 300);
    return e;
}

void env_free(Envelope_t e) {
    if (e->points) {
        free(e->points);
        e->points = NULL;
        e->points_num = 0;
        e->poins_cap = 0;
    }

    if (e->tex.id) {
        UnloadRenderTexture(e->tex);
        memset(&e->tex, 0, sizeof(e->tex));
    }

    free(e);
}

