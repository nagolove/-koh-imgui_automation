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
static const int screen_width = 1920;
static const int screen_height = 1080;
#endif

static Camera2D camera = {0};
static HotkeyStorage hk = {0};

static Envelope_t e;

static void render_gui() {
    rlImGuiBegin();

    bool open = false;
    igShowDemoWindow(&open);

    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("automation", &wnd_open, wnd_flags);

    ImVec2 wnd_pos = {}, wnd_size = {};
    igGetWindowPos(&wnd_pos);
    igGetWindowSize(&wnd_size);

    bool collapsed = true;

    ImVec2 img_min = {}, img_max = {};
    if (igCollapsingHeader_TreeNodeFlags("view", 0)) {
        collapsed = false;
        env_draw_imgui(e);
    }
    igGetItemRectMin(&img_min);
    igGetItemRectMax(&img_max);

    igEnd();

    rlImGuiEnd();

    DrawCircle(wnd_pos.x, wnd_pos.y, 10, RED);
    DrawCircle(wnd_pos.x + wnd_size.x, wnd_pos.y + wnd_size.y, 10, RED);

    if (collapsed) {
        DrawCircle(img_min.x, img_min.y, 10, BLUE);
        DrawCircle(img_max.x, img_max.y, 10, BLUE);
    }
}


static void update_render() {
    ClearBackground(BLACK); 
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

    e = env_new();
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop_arg(update_render, NULL, target_fps, 1);
#else
    while (!WindowShouldClose()) {
        update_render();
    }
#endif
    env_free(e);

    rlImGuiShutdown();

    CloseWindow();

    hotkey_shutdown(&hk);
    sc_shutdown();
    logger_shutdown();

    return 0;
}
