#include "koh_envelope.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include <raymath.h>
#include "koh_routine.h"
#include "koh_logger.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_timerman.h"

// TODO: Сделать все на double
// TODO: Сделать высоту первой точки - нулем, при любом её положении
typedef struct Envelope {
    Vector2         *points;
    float           *lengths, *lengths_sorted;
    float           length_full;
    int             points_num, poins_cap;
    RenderTexture2D rt_main, rt_text;
    ImVec2          img_min, img_max;
    // Если stick_index == -1, то индекс сброшен
    int             stick_index;
    Vector2         *under_cursor, cursor_pos;
    /*const char      *name;*/
    Vector2         *left, *right, *leftest, *rightest;

    bool            is_playing, draw_ruler;
    TimerMan        *tm;
    // Положение анимированной метки
    Vector2         player;
    EnvelopeOpts    opts;
    bool            baked;
    float           last_amount;
} Envelope;

static const Color color_active_handle = RED;
static const Color color_player = GREEN;
static const double play_duration = 10.;
static const Color color_line_default = BLUE;
static const Color color_line_marked = GREEN;
static const Color color_ruler = BLACK;
static const Color color_handle = VIOLET;
static const Color color_handle_ruler = YELLOW;
static const Color color_handle_supreme = BLACK;

enum EnvelopeMode {
    ENV_MODE_LINEAR,
    ENV_MODE_CURVE,
};

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

static int cmp_points(const void *a, const void *b, void *ud) {
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
    size_t sz = sizeof(e->points[0]);
    koh_qsort(e->points, e->points_num, sz, cmp_points, NULL);
    e->baked = false;
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
        e->draw_ruler = true;

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
    } else 
        e->draw_ruler = false;

}

void draw_lines(Envelope_t e) {
    Vector2 p1 = e->points[0];
    const float tex_height = e->rt_main.texture.height;
    p1.y = tex_height - p1.y;
    for (int i = 1; i < e->points_num ; i++) {
        Vector2 p2 = e->points[i];
        p2.y = tex_height - p2.y;
        DrawLineEx(p1, p2, e->opts.line_thick, color_line_default);
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

static void env_draw_cursor_text(Envelope_t e, Vector2 cursor_point) {
    int tex_w = e->rt_text.texture.width,
        tex_h = e->rt_text.texture.height; 
    Rectangle dest = { cursor_point.x, cursor_point.y, tex_w, tex_h };

    const int label_font_size = e->opts.label_font_size;

    /*trace("env_draw: cursor_point %s\n", Vector2_tostr(cursor_point));*/

    if (cursor_point.x >= tex_w) {
        dest.x -= tex_w;
    }

    if (cursor_point.y - label_font_size >= tex_w) {
        dest.y -= label_font_size;
    }

    DrawTexturePro(e->rt_text.texture, 
        (Rectangle) { 0., 0., tex_w, tex_h }, 
        dest,
        (Vector2) { 0, 0}, 0., WHITE
    );
}

static void draw_text(Envelope_t e, Vector2 *cursor_point) {
    assert(cursor_point);
    *cursor_point = (Vector2){
        .y = e->rt_main.texture.height - e->under_cursor->y,
        .x = e->under_cursor->x,
    };

    char buf[64] = {};
    sprintf(buf, "{%d, %d}", (int)cursor_point->x, (int)cursor_point->y);

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

static void draw_lines_marked(Envelope_t e) {
    Vector2 p1 = e->points[0];
    const float tex_height = e->rt_main.texture.height;
    p1.y = tex_height - p1.y;
    float len = 0;
    float per = e->last_amount * e->length_full;
    /*trace("draw_lines_marker: per %f\n", per);*/
    for (int i = 1; i < e->points_num ; i++) {
        if (len >= per)
            break;

        Vector2 p2 = e->points[i];
        p2.y = tex_height - p2.y;
        DrawLineEx(p1, p2, e->opts.line_thick, color_line_marked);
        len += e->lengths[i - 1];
        /*trace("draw_lines_marker: len %f\n", len);*/
        p1 = p2;
    }
}

static void draw_ruler(Envelope_t e) {
    if (!e->points) 
        return;

    int tex_w = e->rt_main.texture.width,
        tex_h = e->rt_main.texture.height;
    Vector2 p1 = { 0., e->cursor_pos.y}, 
            p2 = { tex_w, e->cursor_pos.y };

    p1.y = tex_h - p1.y;
    p2.y = tex_h - p2.y;

    DrawLineV(p1, p2, color_ruler);

    for (int i = 0; i < e->points_num; i++) {
        Vector2 point = e->points[i];
        point.y = tex_h - point.y;
        bool has = CheckCollisionPointLine(point, p1, p2, 1);
        if (has) {
            const int handle_size = e->opts.handle_size;
            DrawCircleV(point, handle_size, color_handle_ruler);
        }
    }


    // TODO: Сделать линейку для деления по частям
    /*
    int stick_index = e->stick_index;
    if (stick_index > 0 && stick_index + 1 < e->points_num) {
        int left = stick_index - 1,
            right = stick_index + 1;

        Vector2 q1 = e->points[left], q2 = e->points[right];
        q1.y = tex_h - q1.y;
        q2.y = tex_h - q2.y;
        DrawLineV(q1, q2, color_ruler);
    }
    */
}

static void env_draw(Envelope_t e) {
    Vector2 cursor_point = {};

    if (e->under_cursor) {
        draw_text(e, &cursor_point);
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

    if (e->baked) {
        // TODO: Рисовать предыдущие отрезки с подсветкой, кроме текущего
        draw_lines_marked(e);
    }

    /*draw_curves(e);*/
    
    // Рисовать кружки ручек
    if (e->points) {
        for (int i = 0; i < e->points_num; i++) {
            Color color = color_handle;

            if (e->leftest == &e->points[i] || e->rightest == &e->points[i])
                color = color_handle_supreme;

            Vector2 p = e->points[i];
            p.y = e->rt_main.texture.height - p.y;
            DrawCircleV(p, e->opts.handle_size, color);
        }
    }

    if (e->draw_ruler) {
        // Рисовать горизонтальную линию для выравнивания по высоте
        draw_ruler(e);
    }


    if (e->under_cursor) {
        DrawCircleV(
            cursor_point,
            e->opts.handle_size + e->opts.handle_size / 2.,
            color_active_handle
        );

        env_draw_cursor_text(e, cursor_point);
    }

    if (e->is_playing) {
        DrawCircleV(e->player, e->opts.handle_size, color_player);
    }

    EndMode2D();
    EndTextureMode();
}

void env_reset(Envelope_t e) {
    trace("env_reset: name '%s'\n", e->opts.name);
    memset(e->points, 0, sizeof(e->points[0]) * e->poins_cap);
    memset(e->lengths, 0, sizeof(e->lengths[0]) * e->poins_cap);
    memset(e->lengths_sorted, 0, sizeof(e->lengths_sorted[0]) * e->poins_cap);
    e->points_num = 0;
    e->length_full = 0;
    e->baked = false;

    env_point_add_default(e);
}

bool on_anim_update(Timer *t) {
    //trace("on_anim_update: amount %f\n", t->amount);
    Envelope_t e = t->data;
    e->player.y = env_eval(e, t->amount);
    e->player.x = t->amount * e->rt_main.texture.width;

    // Возможность остановки по нажатию 'stop'
    return !e->is_playing;
}

void on_anim_stop(Timer *t) {
    trace("on_anim_stop:\n");
    Envelope_t e = t->data;
    e->is_playing = false;
}

void env_stop(Envelope_t e) {
    e->is_playing = false;
    timerman_clear(e->tm);
    timerman_pause(e->tm, false);
}

static void env_pause(Envelope_t e) {
    timerman_pause(e->tm, !timerman_is_paused(e->tm));
}

void env_play(Envelope_t e) {
    if (e->is_playing)
        return;

    e->is_playing = true;
    e->player.x = 0.;
    e->player.y = 0.;
    timerman_add(e->tm, (TimerDef) {
        .data = e,
        .on_update = on_anim_update,
        .on_stop = on_anim_stop,
        .duration = play_duration,
    });
}

void env_draw_imgui_opts(Envelope_t e) {
    igText("points num %d\n", e->points_num);
    igSameLine(0., -1.);
    
    char buf[128] = {};
    snprintf(buf, sizeof(buf) - 1, "reset envelope##%s", e->opts.name);

    if (igSmallButton(buf)) {
        env_reset(e);
    }

    igSameLine(0, -1);
    snprintf(buf, sizeof(buf) - 1, "play##%s", e->opts.name);
    if (igSmallButton(buf)) {
        env_play(e);
    }

    igSameLine(0, -1);
    snprintf(buf, sizeof(buf) - 1, "pause##%s", e->opts.name);
    if (igSmallButton(buf)) {
        env_pause(e);
    }

    igSameLine(0, -1);
    snprintf(buf, sizeof(buf) - 1, "stop##%s", e->opts.name);
    if (igSmallButton(buf)) {
        env_stop(e);
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
    timerman_update(e->tm);

    if (e->opts.name) 
        igText("%s", e->opts.name);

    rlImGuiImage(&e->rt_main.texture);

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
    e->draw_ruler = false;
    e->baked = false;
    e->is_playing = false;
    e->tm = timerman_new(1, "envelope player");
    e->rt_main = LoadRenderTexture(opts.tex_w, opts.tex_h);

    // Расчет размеров текстуры
    {
        assert(opts.label_font_size > 0);
        assert(opts.tex_w > 0);
        assert(opts.tex_h > 0);
        char buf[128] = {};
        snprintf(buf, sizeof(buf) - 1, "{%d, %d}", opts.tex_w, opts.tex_h);
        int text_w = MeasureText(buf, opts.label_font_size);
        e->rt_text = LoadRenderTexture(text_w, opts.label_font_size);
    }

    assert(opts.default_points_cap > 0);
    e->poins_cap = opts.default_points_cap;
    e->points_num = 0;
    e->points = calloc(e->poins_cap, sizeof(e->points[0]));
    e->lengths = calloc(e->poins_cap, sizeof(e->lengths[0]));
    e->lengths_sorted = calloc(e->poins_cap, sizeof(e->lengths_sorted[0]));
    e->length_full = 0;
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
    if (e->tm) {
        timerman_free(e->tm);
        e->tm = NULL;
    }

    if (e->lengths_sorted) {
        free(e->lengths_sorted);
        e->lengths_sorted = NULL;
    }

    if (e->lengths) {
        free(e->lengths);
        e->lengths = NULL;
    }

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

static int cmp(const void *a, const void *b, void *ud) {
    const float *_a = a, *_b = b;
    return *_a - *_b;
}

static float len(Vector2 a, Vector2 b) {
    const float t = (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
    return sqrtf(t * t);
}

void env_bake(Envelope_t e) {
    // Расчитать длину каждого сегмента
    const int lengths_num = e->points_num - 1;
    for (int i = 0; i < lengths_num; i++) {
        e->lengths[i] = len(e->points[i], e->points[i + 1]);
    }

    for (int i = 0; i < lengths_num; i++) {
        trace("env_bake: len %f\n", e->lengths[i]);
    }
    trace("\n");

    for (int i = 0; i < lengths_num; i++) {
        e->lengths_sorted[i] = e->lengths[i];
    }
    size_t sz = sizeof(e->lengths_sorted[0]);
    koh_qsort(e->lengths_sorted, lengths_num, sz, cmp, NULL);

    for (int i = 0; i < lengths_num; i++) {
        trace("env_bake: len sorted %f\n", e->lengths_sorted[i]);
    }
    trace("\n");

    // Расчитать длину всей кривой
    // Сперва складывать наименьшие величины
    float length_full = 0.f;
    for (int i = 0; i < e->points_num; i++) {
        /*trace("env_bak: delta %f\n", e->lengths_sorted[i]);*/
        length_full += e->lengths_sorted[i];
    }

    e->length_full = length_full;
    trace("length_full: %f\n", e->length_full);

    e->baked = true;
}

float env_eval(Envelope_t e, float amount) {
    if (amount < 0)
        amount = 0;
    if (amount > 1.f)
        amount = 1.f;

    if (!e->baked) 
        env_bake(e);

    float path_len = 0;

    /*
    Если для сохранения значения интерполяции использовать только amount,
    то непонятно как делать?

    Придется каждый раз, с самого начала считать, пока не будет достигнуто
    значение amount и после него просчитывать еще несколько итераций
     */

    e->last_amount = amount;

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

    e->baked = false;
}

EnvelopeOpts env_partial_opts(EnvelopeOpts opts) {
    opts.handle_size = 20;
    opts.line_thick = 10;
    opts.default_points_cap = 1024;
    opts.label_font_size = 64;
    opts.background_color = GRAY;
    return opts;
}

