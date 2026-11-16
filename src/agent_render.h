#pragma once
#include "buddy_render.h"

#define AGENT_ROWS 5   // rows visible at once on the 320x240 screen

// Draws the agent board (header with usage, one row per agent from `top`).
// `ui` supplies usage %, link state and battery for the header.
void agentsRender(lgfx::LovyanGFX& gfx, const UI& ui, bool linked, int top);
