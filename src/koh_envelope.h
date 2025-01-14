// vim: fdm=marker

#pragma once

typedef struct Envelope *Envelope_t;

Envelope_t env_new();
void env_free(Envelope_t e);

void env_draw_imgui(Envelope_t e);
