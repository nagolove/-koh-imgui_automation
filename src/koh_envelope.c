#include "koh_envelope.h"

#include "koh_routine.h"
#include "koh_logger.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

typedef struct Envelope {
    Vector2         *points;
    int             points_num, poins_cap;
    RenderTexture2D tex;
    ImVec2          img_min, img_max;
    // Если stick_index == -1, то индекс сброшен
    int             stick_index;
    Vector2         *under_cursor, cursor_pos;
    const char      *name;
    int             thick;
} Envelope;

static void env_point_add_default(Envelope_t e);

// TODO: Перенести в структуру как изменямую величину
static const int handle_size = 10;

// TODO: Придумать абстракцию для передвижения точек под курсором
Vector2 *points_process(Envelope_t env, Vector2 pos, int *stick_index) {
    /*trace("points_process: pos %s\n", Vector2_tostr(pos));*/
    for (int i = 0; i < env->points_num; i++) {
        Vector2 p = env->points[i];
        if (CheckCollisionPointCircle(pos, p, handle_size)) {
            if (stick_index)
                *stick_index = i;
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

void env_point_add(Envelope_t e, Vector2 pos) {
    if (e->points_num + 1 == e->poins_cap) {
        trace("points_add: not enough memory, skipping\n");
        return;
    }

    /*trace("points_add: pos %s\n", Vector2_tostr(pos));*/

    // Добавить точку в конец массива
    e->points[e->points_num] = pos;
    e->points_num++;

    // XXX: Проверить сортировку
    koh_qsort(e->points, e->points_num, sizeof(e->points[0]), cmp, NULL);
}

// TODO: Добавить режимы:
// с зажатым шифтом - перемещение только по горизонтали,
// с зажатым контрол - только по вертикали
// TODO: ПКМ - удалить
static void env_input(Envelope_t e) {

    e->cursor_pos = Vector2Subtract(
        GetMousePosition(), Im2Vec2(e->img_min)
    );

    /*e->stick_index = -1;*/

    e->under_cursor = points_process(e, e->cursor_pos, &e->stick_index);
    bool sticked = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    /*if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && e->stick_index == -1) {*/
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !e->under_cursor) {
        env_point_add(e, e->cursor_pos);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && e->under_cursor) {
        env_point_remove(e, e->stick_index);
    }

    if (e->stick_index != -1 && sticked) {
        // Первую и последнюю вершину можно двигать только вверх и вниз
        if (e->stick_index == 0 || e->stick_index == e->points_num - 1) {
            e->points[e->stick_index].y = e->cursor_pos.y;
        } else {
            Vector2 *left = NULL, *right = NULL;

            if (e->stick_index >= 1) {
                left = &e->points[e->stick_index - 1];
            }
            if (e->stick_index + 1 < e->points_num) {
                right = &e->points[e->stick_index + 1];
            }

            // Ограничение перемещения точки по горизонтали
            Vector2 cursor = e->cursor_pos;
            if (left && e->cursor_pos.x < left->x)
                cursor.x = left->x;
            if (right && e->cursor_pos.x > right->x)
                cursor.x = right->x;

            bool in_range = e->stick_index >= 0 &&
                            e->stick_index < e->points_num;
            if (e->points && in_range)
                e->points[e->stick_index] = cursor;
        }
    }

}

void draw_lines(Envelope_t e) {
    Vector2 p1 = e->points[0];
    const float tex_height = e->tex.texture.height;
    p1.y = tex_height - p1.y;
    for (int i = 1; i < e->points_num ; i++) {
        Vector2 p2 = e->points[i];
        p2.y = tex_height - p2.y;
        DrawLineEx(p1, p2, e->thick, BLUE);
        p1 = p2;
    }
}

void draw_curves(Envelope_t e) {
    // Рисовать линии
    Vector2 p1 = e->points[0];
    const float tex_height = e->tex.texture.height;
    p1.y = tex_height - p1.y;
    const float thick = 3.f;
    Vector2 points[4] = {};
    int point_i = 0;
    for (int i = 1; i < e->points_num ; i++) {
        points[point_i % 4] = p1;
        point_i++;

        Vector2 p2 = e->points[i];
        p2.y = tex_height - p2.y;

        if (e->points_num >= 4) {
            DrawSplineSegmentLinear(p1, p2, thick, RED);                    
            DrawSplineBasis(points, 4, e->thick, YELLOW);
            DrawSplineCatmullRom(points, 4, e->thick, BLUE);
        }

        p1 = p2;
/*DrawSplineSegmentBasis(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float thick, Color color); // Draw spline segment: B-Spline, 4 points*/
/*DrawSplineSegmentCatmullRom(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float thick, Color color); // Draw spline segment: Catmull-Rom, 4 points*/
/*DrawSplineSegmentBezierQuadratic(Vector2 p1, Vector2 c2, Vector2 p3, float thick, Color color); // Draw spline segment: Quadratic Bezier, 2 points, 1 control point*/
/*DrawSplineSegmentBezierCubic(Vector2 p1, Vector2 c2, Vector2 c3, Vector2 p4, float thick, Color color); // Draw spline segment: Cubic Bezier, 2 points, 2 control points*/
    }
}


static void env_draw(Envelope_t e) {
    BeginTextureMode(e->tex);
    Camera2D cam = {
        .zoom = 1.,
    };
    BeginMode2D(cam);

    ClearBackground(GRAY);

    CoSysOpts cosys_opts = {
        .dest = { 0., 0., e->tex.texture.width, e->tex.texture.height},
        .color = WHITE,
        .step = 10.f,
    };
    cosys_draw(cosys_opts);

    draw_lines(e);

    /*draw_curves(e);*/

    // Рисовать кружки ручек
    for (int i = 0; i < e->points_num; i++) {
        Vector2 p = e->points[i];
        p.y = e->tex.texture.height - p.y;
        DrawCircleV(p, handle_size, VIOLET);
    }

    if (e->under_cursor) {
        Vector2 p = {
            .y = e->tex.texture.height - e->under_cursor->y,
            .x = e->under_cursor->x,
        };
        DrawCircleV(p, handle_size + handle_size / 2., RED);
    }


    /*trace("\n");*/

    EndMode2D();
    EndTextureMode();
}

void env_reset(Envelope_t e) {
    trace("env_reset: name '%s'\n", e->name);
    memset(e->points, 0, sizeof(e->points[0]) * e->poins_cap);
    e->points_num = 0;

    env_point_add_default(e);
}

void env_draw_imgui_opts(Envelope_t e) {
    igText("points num %d\n", e->points_num);
    igSameLine(0., -1.);
    
    char buf[128] = {};
    snprintf(buf, sizeof(buf) - 1, "reset envelope##%s", e->name);

    if (igSmallButton(buf)) {
        env_reset(e);
    }

    /*
    igSameLine(0., -1.);
    if (igSmallButton("copy")) {
        char *data_str = env_export_alloc(e);
        if (data_str) {
            SetClipboardText(data_str);
            free(data_str);
        }
    }
    igSameLine(0., -1.);
    if (igSmallButton("paste")) {
        const char *data_str = GetClipboardText();
        env_export_import(e, data_str);
    }
    */

}

void env_input_reset(Envelope_t e) {
    assert(e);
    e->stick_index = -1;
}

void env_draw_imgui_env(Envelope_t e) {

    env_draw(e);

    if (e->name) 
        igText("%s", e->name);

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
        // Необрабытвать ввод на участке окна в который рисуется картинка с 
        // кривой.
        igSetWindowHitTestHole(wnd, e->img_min, sz);
        env_input(e);
    } else {
        env_input_reset(e);
    }

}

static void env_point_add_default(Envelope_t e) {
    env_point_add(e, (Vector2) { 0., 0});
    Texture t = e->tex.texture;
    env_point_add(e, (Vector2) {  t.width, t.height});
}

Envelope_t env_new(EnvelopeOpts opts) {
    Envelope_t e = calloc(1, sizeof(*e));
    // TODO: Задать размер текстуры из опций
    e->tex = LoadRenderTexture(opts.tex_w, opts.tex_h);
    e->poins_cap = 1024;
    e->points_num = 0;
    e->points = calloc(e->poins_cap, sizeof(e->points[0]));
    e->name = opts.name;

    if (opts.line_thick)
        e->thick = opts.line_thick;
    else
        e->thick = 2;

    e->stick_index = -1;

    env_point_add_default(e);

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
    if (amount < 0)
        amount = 0;
    if (amount > 1.f)
        amount = 1.f;

    // TODO: Всегда использовать линейный режим, когда использовать 
    // режим кривых?
    return 0.;
}

char *env_export_alloc(Envelope_t e) {
    return NULL;
}

void env_export_import(Envelope_t e, const char *lua_str) {
}

void env_point_remove(Envelope_t e, int index) {
    assert(e);
    assert(index >= 0 && index < e->points_num);

    if (e->points_num > 0)
        e->points_num--;
}
