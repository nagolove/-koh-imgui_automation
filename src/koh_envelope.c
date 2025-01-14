#include "koh_envelope.h"

#include "koh_routine.h"
#include "koh_logger.h"
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
    ImVec2          img_min, img_max;
} Envelope;

// TODO: Перенести в структуру как изменямую величину
static const int handle_size = 5;

// TODO: Должна находить либо кружок под курсором или соседей кружка.
Vector2 *points_process(Envelope_t env, Vector2 mouse_pos) {
    for (int i = 0; i < env->points_num; i++) {
        Vector2 p = env->points[i];
        if (CheckCollisionPointCircle(mouse_pos, p, handle_size)) {
            return &env->points[i];
        }
    }
    return NULL;
}

static int cmp(const void *a, const void *b, void *ud) {
    const Vector2 *_a = a;
    const Vector2 *_b = b;
    return _a->x - _b->x;
}

Vector2 *points_add(Envelope_t e, Vector2 mouse_pos) {
    if (e->points_num + 1 == e->poins_cap) {
        trace("points_add: not enough memory, skipping\n");
        return NULL;
    }

    trace("points_add: mouse_pos %s\n", Vector2_tostr(mouse_pos));

    // Добавить точку в конец массива
    e->points[e->points_num] = mouse_pos;
    Vector2 *ret = &e->points[e->points_num];
    e->points_num++;

    // TODO: пересортировать массив
    koh_qsort(e->points, e->points_num, sizeof(e->points[0]), cmp, NULL);

    return ret;
}

static void env_input(Envelope_t e) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = Vector2Subtract(GetMousePosition(), Im2Vec2(e->img_min));
        /*trace("env_input: img_max %s\n", Vector2_tostr(Im2Vec2(e->img_max)));*/
        points_add(e, mp);
    }
}

static void env_draw(Envelope_t e) {
    BeginTextureMode(e->tex);
    Camera2D cam = {
        .zoom = 1.,
    };
    BeginMode2D(cam);

    ClearBackground(GRAY);

    /*
    DrawCircle(0, 100, 20, YELLOW);
    DrawCircle(0, -100, 20, RED);
    */

    for (int i = 0; i < e->points_num; i++) {
        Vector2 p = e->points[i];
        p.y = e->tex.texture.height - p.y;
        DrawCircleV(p, handle_size, VIOLET);
    }

    Vector2 mp = Vector2Subtract(GetMousePosition(), Im2Vec2(e->img_min));
    Vector2 *under_cursor = points_process(e, mp);
    if (under_cursor) {
        Vector2 p = {
            .y = e->tex.texture.height - under_cursor->y,
            .x = under_cursor->x,
        };
        DrawCircleV(p, handle_size + handle_size / 2., RED);
    }

    /*trace("\n");*/

    EndMode2D();
    EndTextureMode();
}

void env_draw_imgui_opts(Envelope_t e) {
    igText("points num %d\n", e->points_num);
    igSameLine(0., 15.);
    
    if (igButton("reset envelope", (ImVec2){})) {
        memset(e->points, 0, sizeof(e->points[0]) * e->poins_cap);
        e->points_num = 0;
    }

}

void env_draw_imgui_env(Envelope_t e) {

    env_draw(e);

    //igPushItemFlag(ImGuiItemFlags_Disabled, true);
    //igButton("Disabled Button", (ImVec2){});
    //igBeginDisabled(true);
    rlImGuiImage(&e->tex.texture);
    /*igEndDisabled();*/
    /*igPopItemFlag();*/

    igGetItemRectMin(&e->img_min);
    igGetItemRectMax(&e->img_max);

    Vector2 mp = GetMousePosition();

    if (mp.x >= e->img_min.x && mp.x <= e->img_max.x && 
            mp.y >= e->img_min.y && mp.y <= e->img_max.y) {
        ImGuiWindow *wnd = igGetCurrentWindow();
        ImVec2 sz = {
            e->img_max.x - e->img_min.x,
            e->img_max.y - e->img_min.y,
        };
        igSetWindowHitTestHole(wnd, e->img_min, sz);

        env_input(e);

        /*
           trace(
           "render_gui: area %s with size %s disabled\n",
           Vector2_tostr(Im2Vec2(img_min)), 
           Vector2_tostr(Im2Vec2(sz))
           );
           */


    }

}

Envelope_t env_new(EnvelopeOpts opts) {
    Envelope_t e = calloc(1, sizeof(*e));
    // TODO: Задать размер текстуры из опций
    e->tex = LoadRenderTexture(opts.tex_w, opts.tex_h);
    e->poins_cap = 1024;
    e->points_num = 0;
    e->points = calloc(e->poins_cap, sizeof(e->points[0]));
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

float env_eval(Envelope_t e, float amount) {
    return 0.;
}
