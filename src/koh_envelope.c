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
    RenderTexture2D rt_main, rt_text;
    ImVec2          img_min, img_max;
    // Если stick_index == -1, то индекс сброшен
    int             stick_index;
    Vector2         *under_cursor, cursor_pos;
    /*const char      *name;*/
    Vector2         *left, *right, *leftest, *rightest;

    EnvelopeOpts    opts;
} Envelope;

static void env_point_add_default(Envelope_t e);

// TODO: Придумать абстракцию для передвижения точек под курсором
Vector2 *points_process(Envelope_t e, Vector2 pos, int *stick_index) {
    /*trace("points_process: pos %s\n", Vector2_tostr(pos));*/
    for (int i = 0; i < e->points_num; i++) {
        Vector2 p = e->points[i];
        if (CheckCollisionPointCircle(pos, p, e->opts.handle_size)) {
            if (stick_index)
                *stick_index = i;
            return &e->points[i];
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
    e->left = NULL;
    e->right = NULL;

    e->cursor_pos = Vector2Subtract(
        GetMousePosition(), Im2Vec2(e->img_min)
    );

    /*e->stick_index = -1;*/

    // Поиск точки под курсором
    e->under_cursor = points_process(e, e->cursor_pos, &e->stick_index);

    bool sticked = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !e->under_cursor) {
        env_point_add(e, e->cursor_pos);
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && e->under_cursor) {
        env_point_remove(e, e->stick_index);
        return;
    }

    e->leftest = &e->points[0];
    e->rightest = &e->points[e->points_num - 1];

    //TODO: если курсор на крайнем кружке, то создавать left, right

    if (e->stick_index != -1 && sticked) {
        // Первую и последнюю вершину можно двигать только вверх и вниз
        if (e->stick_index == 0 || e->stick_index == e->points_num - 1) {
            e->points[e->stick_index].y = e->cursor_pos.y;
        } else {

            if (e->stick_index >= 1) {
                e->left = &e->points[e->stick_index - 1];
            }
            if (e->stick_index + 1 < e->points_num) {
                e->right = &e->points[e->stick_index + 1];
            }

            // Ограничение перемещения точки по горизонтали
            Vector2 cursor = e->cursor_pos;
            if (e->left && e->cursor_pos.x < e->left->x)
                cursor.x = e->left->x;
            if (e->right && e->cursor_pos.x > e->right->x)
                cursor.x = e->right->x;

            bool in_range = e->stick_index >= 0 &&
                            e->stick_index < e->points_num;
            if (e->points && in_range)
                e->points[e->stick_index] = cursor;
        }
    }

}

void draw_lines(Envelope_t e) {
    Vector2 p1 = e->points[0];
    const float tex_height = e->rt_main.texture.height;
    p1.y = tex_height - p1.y;
    for (int i = 1; i < e->points_num ; i++) {
        Vector2 p2 = e->points[i];
        p2.y = tex_height - p2.y;
        DrawLineEx(p1, p2, e->opts.line_thick, BLUE);
        p1 = p2;
    }
}

void draw_curves(Envelope_t e) {
    // Рисовать линии
    Vector2 p1 = e->points[0];
    const float tex_height = e->rt_main.texture.height;
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
            DrawSplineBasis(points, 4, e->opts.line_thick, YELLOW);
            DrawSplineCatmullRom(points, 4, e->opts.line_thick, BLUE);
        }

        p1 = p2;
/*DrawSplineSegmentBasis(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float thick, Color color); // Draw spline segment: B-Spline, 4 points*/
/*DrawSplineSegmentCatmullRom(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, float thick, Color color); // Draw spline segment: Catmull-Rom, 4 points*/
/*DrawSplineSegmentBezierQuadratic(Vector2 p1, Vector2 c2, Vector2 p3, float thick, Color color); // Draw spline segment: Quadratic Bezier, 2 points, 1 control point*/
/*DrawSplineSegmentBezierCubic(Vector2 p1, Vector2 c2, Vector2 c3, Vector2 p4, float thick, Color color); // Draw spline segment: Cubic Bezier, 2 points, 2 control points*/
    }
}


static void env_draw(Envelope_t e) {
    Vector2 cursor_point = {};

    if (e->under_cursor) {
        cursor_point = (Vector2){
            .y = e->rt_main.texture.height - e->under_cursor->y,
            .x = e->under_cursor->x,
        };

        char buf[64] = {};
        sprintf(buf, "{%d, %d}", (int)cursor_point.x, (int)cursor_point.y);

        BeginTextureMode(e->rt_text);
        BeginMode2D((Camera2D) { .zoom = 1., });
        Color tmp = e->opts.background_color;
        tmp.a = 0;

        tmp = BROWN;

        ClearBackground(tmp);
        DrawText(buf, 0, 0, e->opts.label_font_size, BLACK);
        EndMode2D();
        EndTextureMode();
    }


    BeginTextureMode(e->rt_main);
    BeginMode2D((Camera2D) { .zoom = 1., });

    ClearBackground(e->opts.background_color);

    Color color = GRAY;
    color.a = 10;
    CoSysOpts cosys_opts = {
        .dest = { 0., 0., e->rt_main.texture.width, e->rt_main.texture.height},
        .color = color,
        .step = 10.f,
    };
    cosys_draw(cosys_opts);

    draw_lines(e);

    /*draw_curves(e);*/

    // Рисовать кружки ручек
    if (e->points) {
        for (int i = 0; i < e->points_num; i++) {
            Color handle_color = VIOLET;

            if (e->leftest == &e->points[i] || e->rightest == &e->points[i])
                handle_color = BLACK;

            Vector2 p = e->points[i];
            p.y = e->rt_main.texture.height - p.y;
            DrawCircleV(p, e->opts.handle_size, handle_color);
        }
    }

    if (e->under_cursor) {
        DrawCircleV(
            cursor_point,
            e->opts.handle_size + e->opts.handle_size / 2.,
            RED
        );

        int tex_w = e->rt_text.texture.width,
            tex_h = e->rt_text.texture.height; 
        int half_tex_w = tex_w / 2, half_tex_h = tex_h / 2;
        Rectangle dest = {
            /*cursor_point.x - half_tex_w,*/
            /*cursor_point.y - half_tex_w,*/
            cursor_point.x,
            cursor_point.y,
            tex_w, tex_h
        };

        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            /*dest.x += tex_w;*/
            DrawTexturePro(e->rt_text.texture, 
                (Rectangle) { 0., 0., tex_w, tex_h }, 
                dest,
                (Vector2) { half_tex_w, half_tex_h },
                0., WHITE
            );
        } else {
            trace("env_draw: cursor_point %s\n", Vector2_tostr(cursor_point));
            if (e->rightest == e->under_cursor)
                dest.x -= tex_w;

            if (cursor_point.y < e->opts.label_font_size) {
                trace("env_draw: upper\n");
                dest.y += e->opts.label_font_size;
            }

            if (cursor_point.y + e->opts.label_font_size >= e->rt_text.texture.height) {
                trace("env_draw: upper\n");
                dest.y -= e->opts.label_font_size;
            }

            DrawTexturePro(e->rt_text.texture, 
                (Rectangle) { 0., 0., tex_w, tex_h }, 
                dest,
                (Vector2) { 0, 0}, 0., WHITE
            );
        }
    }
    /*trace("\n");*/

    EndMode2D();
    EndTextureMode();
}

void env_reset(Envelope_t e) {
    trace("env_reset: name '%s'\n", e->opts.name);
    memset(e->points, 0, sizeof(e->points[0]) * e->poins_cap);
    e->points_num = 0;

    env_point_add_default(e);
}

void env_draw_imgui_opts(Envelope_t e) {
    igText("points num %d\n", e->points_num);
    igSameLine(0., -1.);
    
    char buf[128] = {};
    snprintf(buf, sizeof(buf) - 1, "reset envelope##%s", e->opts.name);

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
    if (e->opts.name) 
        igText("%s", e->opts.name);

    //igPushItemFlag(ImGuiItemFlags_Disabled, true);
    //igButton("Disabled Button", (ImVec2){});
    //igBeginDisabled(true);
    rlImGuiImage(&e->rt_main.texture);
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

    env_draw(e);
}

static void env_point_add_default(Envelope_t e) {
    env_point_add(e, (Vector2) { 0., 0});
    Texture t = e->rt_main.texture;
    env_point_add(e, (Vector2) {  t.width, t.height});
}

Envelope_t env_new(EnvelopeOpts opts) {
    Envelope_t e = calloc(1, sizeof(*e));
    e->rt_main = LoadRenderTexture(opts.tex_w, opts.tex_h);

    // Расчет размеров текстуры
    {
        assert(opts.label_font_size > 0);
        assert(opts.tex_w > 0);
        assert(opts.tex_h > 0);
        int tex_text_w = 128 * 3, tex_text_h = 128 * 1;
        char buf[128] = {};
        snprintf(buf, sizeof(buf) - 1, "{%d, %d}", opts.tex_w, opts.tex_h);
        int text_w = MeasureText(buf, opts.label_font_size);
        e->rt_text = LoadRenderTexture(text_w, opts.label_font_size);
    }

    assert(opts.default_points_cap > 0);
    e->poins_cap = opts.default_points_cap;
    e->points_num = 0;
    e->points = calloc(e->poins_cap, sizeof(e->points[0]));
    e->opts.name = opts.name;
    e->opts = opts;

    if (opts.line_thick)
        e->opts.line_thick = opts.line_thick;
    else
        e->opts.line_thick = 2;

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

    if (e->rt_main.id) {
        UnloadRenderTexture(e->rt_main);
        memset(&e->rt_main, 0, sizeof(e->rt_main));
    }

    if (e->rt_text.id) {
        UnloadRenderTexture(e->rt_text);
        memset(&e->rt_text, 0, sizeof(e->rt_text));
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

    // Сделать неудалимыми крайние точки
    if (index == 0 || index + 1 == e->points_num)
        return;

    for (int i = index; i < e->points_num; i++) {
        e->points[i] = e->points[i + 1];
    }

    if (e->points_num > 0)
        e->points_num--;
}

EnvelopeOpts env_partial_opts(EnvelopeOpts opts) {
    opts.handle_size = 20;
    opts.line_thick = 10;
    opts.default_points_cap = 1024;
    opts.label_font_size = 64;
    opts.background_color = GRAY;
    return opts;
}

