#pragma once
#include <M5Unified.h>

// ST_STALE: linked to the broker, but the usage reading is too old to trust
// (daemon reported a failure, or nothing new arrived recently).
enum BuddyState { ST_OFFLINE, ST_CONNECTING, ST_IDLE, ST_WORKING, ST_MAXED, ST_STALE };

struct UI {
    BuddyState state      = ST_OFFLINE;
    int        h5         = 0;           // 5-hour window usage %
    int        wk         = 0;           // weekly window usage %
    char       resetIn[16] = "";         // "1H 29M"  (maxed only)
    char       resetAt[16] = "";         // "3:00p"   (maxed only)
    char       note[28]    = "";         // "+13pt this poll" (working) / reason (stale)
    uint32_t   ts          = 0;          // daemon's Unix time for this reading, 0 = unknown
    bool       fresh       = false;
    int        battery     = -1;         // 0-100, -1 = unknown
    bool       charging    = false;
};

void buddyInitPalette(bool dark = false);
void buddyRender(lgfx::LovyanGFX& gfx, const UI& ui);
