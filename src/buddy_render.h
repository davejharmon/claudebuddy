#pragma once
#include <M5Unified.h>

enum BuddyState { ST_OFFLINE, ST_CONNECTING, ST_IDLE, ST_WORKING, ST_MAXED };

struct UI {
    BuddyState state      = ST_OFFLINE;
    int        h5         = 0;           // 5-hour window usage %
    int        wk         = 0;           // weekly window usage %
    char       resetIn[16] = "";         // "1H 29M"  (maxed only)
    char       resetAt[16] = "";         // "3:00p"   (maxed only)
    char       note[28]    = "";         // "+13pt this poll" (working only)
    bool       fresh       = false;
};

void buddyInitPalette(bool dark = false);
void buddyRender(lgfx::LovyanGFX& gfx, const UI& ui);
