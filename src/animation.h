#pragma once
#include <Arduino.h>

#define MAX_FRAMES  24
#define MAX_PALETTE 10
#define ANIM_DIM    20

struct Animation {
    uint8_t  frame_count;
    uint16_t hold_ms[MAX_FRAMES];
    uint16_t palette[MAX_PALETTE];  // RGB565
    uint8_t  transparent_idx;
    uint8_t  pixels[MAX_FRAMES][ANIM_DIM][ANIM_DIM];
    bool     valid;
};

bool loadAnimation(const char* path, Animation& out);
