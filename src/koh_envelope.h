// vim: fdm=marker

#pragma once

typedef struct Envelope *Envelope_t;

typedef struct EnvelopeOpts {
    int tex_w, tex_h;
} EnvelopeOpts;

Envelope_t env_new(EnvelopeOpts opts);
void env_free(Envelope_t e);

void env_draw_imgui_opts(Envelope_t e);
void env_draw_imgui_env(Envelope_t e);

// 0. <= amount <= 1.
// Возвращает интерполированное значение высоты кривой
float env_eval(Envelope_t e, float amount);
