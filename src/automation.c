// vim: fdm=marker

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh_common.h"
#include "koh_envelope.h"
#include "koh_routine.h"
#include "koh_console.h"
#include "koh_hashers.h"
#include "koh_hotkey.h"
#include "koh_hotkey.h"
#include "koh_logger.h"
#include "koh_script.h"
#include "raylib.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#if defined(PLATFORM_WEB)
static const int screen_width = 1920;
static const int screen_height = 1080;
#else
static const int screen_width = 1920 * 2;
static const int screen_height = 1080 * 2;
#endif

static Camera2D camera = {0};
static HotkeyStorage hk = {0};

static EnvelopeOpts e_opts[] = {
    {
        .name = "lines",
        .tex_w = 800 * 2,
        .tex_h = 100 * 2,
    },
    {
        .name = "curves",
        .tex_w = 1000 * 2,
        .tex_h = 600 * 2,
    },
};

static Envelope_t e[2];
const static int e_num = sizeof(e_opts) / sizeof(e_opts[0]);

static void render_gui() {
    rlImGuiBegin();

    bool open = false;
    igShowDemoWindow(&open);

    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize | 
                                 ImGuiWindowFlags_NoSavedSettings;
    igBegin("automation", &wnd_open, wnd_flags);

    ImVec2 wnd_pos = {}, wnd_size = {};
    igGetWindowPos(&wnd_pos);
    igGetWindowSize(&wnd_size);

    bool collapsed = false;

    if (igButton("recreate", (ImVec2){})) {
        for (int i = 0; i < e_num; i++) {
            if (e[i]) {
                env_free(e[i]);
                e[i] = NULL;
            }
            e[i] = env_new(env_partial_opts(e_opts[i]));
        }
    }

    igSliderInt("tex_w", &e_opts[0].tex_w, 0, 2000, "%d", 0);
    igSliderInt("tex_h", &e_opts[0].tex_h, 0, 2000, "%d", 0);

    igSeparator();

    ImVec2 img_min = {}, img_max = {};
    const bool header_pressed = true;
    if (header_pressed || igCollapsingHeader_TreeNodeFlags("view", 0)) {
        collapsed = true;

        //ImGuiIO *io = igGetIO();

        for (int i = 0; i < e_num; i++) {
            env_draw_imgui_opts(e[i]);
            env_draw_imgui_env(e[i]);
        }
    }
    igGetItemRectMin(&img_min);
    igGetItemRectMax(&img_max);

    igEnd();

    rlImGuiEnd();

    //DrawCircle(wnd_pos.x, wnd_pos.y, 10, RED);
    //DrawCircle(wnd_pos.x + wnd_size.x, wnd_pos.y + wnd_size.y, 10, RED);

    if (collapsed) {
        /*DrawCircle(img_min.x, img_min.y, 10, BLUE);*/
        /*DrawCircle(img_max.x, img_max.y, 10, BLUE);*/
    }
}


static void update_render() {
    ClearBackground(GRAY); 
    BeginDrawing();
    render_gui();
    EndDrawing();

    char title[128] = {};
    sprintf(title, "fps %d", GetFPS());
    SetWindowTitle(title);
}

int main(void) {
    // {{{
    koh_hashers_init();
    camera.zoom = 1.0f;
    srand(time(NULL));
    InitWindow(screen_width, screen_height, "color worms");
    SetWindowState(FLAG_WINDOW_UNDECORATED);

    SetWindowMonitor(1);
    SetTargetFPS(60);

    rlImGuiSetup(&(struct igSetupOptions) {
            .dark = false,
            .font_path = "assets/djv.ttf",
            .font_size_pixels = 40,
    });

    logger_init();
    sc_init();
    logger_register_functions();
    sc_init_script();

    SetTraceLogLevel(LOG_ERROR);

    hotkey_init(&hk);
    console_init(&hk, &(struct ConsoleSetup) {
        .on_enable = NULL,
        .on_disable = NULL,
        .udata = NULL,
        .color_cursor = BLUE,
        .color_text = BLACK,
        .fnt_path = "assets/djv.ttf",
        .fnt_size = 20,
    });
    console_immediate_buffer_enable(true);

    for (int i = 0; i < e_num; i++) {
        e[i] = env_new(env_partial_opts(e_opts[i]));
    }

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(update_render, NULL, target_fps, 1);
#else
    while (!WindowShouldClose()) {
        update_render();
    }
#endif

    for (int i = 0; i < e_num; i++) {
        env_free(e[i]);
    }

    rlImGuiShutdown();

    CloseWindow();

    hotkey_shutdown(&hk);
    sc_shutdown();
    logger_shutdown();

    return 0;
}
