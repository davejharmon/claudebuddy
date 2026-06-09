// Drawing layer — adapted from integration/assets/reference_render.cpp
// Matches "Claude Desk Buddy.html" pixel-for-pixel.
// Call buddyInitPalette() once after M5.begin(), then buddyRender() on any LovyanGFX target.
#include "buddy_render.h"

static uint16_t C_BG, C_BG2, C_INK, C_INKSOFT, C_CORAL, C_LITE, C_CLAY,
                C_WHITE, C_ALERT, C_GHOST, C_BLUSH, C_OUTLINE;

void buddyInitPalette(bool dark) {
    if (dark) {
        // Warm dark — charcoal-brown bg, cream text, same coral accents
        C_BG      = M5.Display.color565(0x1A,0x16,0x14);  // dark charcoal-brown
        C_BG2     = M5.Display.color565(0x1F,0x1C,0x1A);  // slightly lighter top band
        C_INK     = M5.Display.color565(0xED,0xE5,0xD4);  // warm white (inverted)
        C_INKSOFT = M5.Display.color565(0x7D,0x72,0x65);  // warm mid-grey
        C_GHOST   = M5.Display.color565(0x2E,0x29,0x24);  // dark unlit segments
    } else {
        // Warm light (original)
        C_BG      = M5.Display.color565(0xE9,0xE1,0xCF);
        C_BG2     = M5.Display.color565(0xEF,0xE7,0xD7);
        C_INK     = M5.Display.color565(0x2B,0x27,0x22);
        C_INKSOFT = M5.Display.color565(0x6E,0x65,0x59);
        C_GHOST   = M5.Display.color565(0xCF,0xC6,0xB2);
    }
    // Mascot outline — always dark regardless of mode
    C_OUTLINE = M5.Display.color565(0x2B,0x27,0x22);
    // Accents are identical in both modes
    C_CORAL   = M5.Display.color565(0xD9,0x77,0x57);
    C_LITE    = M5.Display.color565(0xEC,0xB0,0x87);
    C_CLAY    = M5.Display.color565(0xB5,0x53,0x2F);
    C_WHITE   = M5.Display.color565(0xF6,0xEF,0xE0);
    C_ALERT   = M5.Display.color565(0xC0,0x49,0x2B);
    C_BLUSH   = M5.Display.color565(0xCF,0x6A,0x49);
}

// ── Mascot ────────────────────────────────────────────────────────────────────
static const char* BODY[16] = {
    "................","......KKKK......","....KKCCCCKK....","...KCCCCCCCCK...",
    "..KCCCCCCCCCCK..","..KCCCCCCCCCCK..",".KCCCCCCCCCCCCK.",".KCCCCCCCCCCCCK.",
    ".KCCCCCCCCCCCCK.",".KCCCCCCCCCCCCK.","..KCCCCCCCCCCK..","..KCCCCCCCCCCK..",
    "...KCCCCCCCCK...","....KKCCCCKK....","......KKKK......","................",
};

struct Cell { uint8_t r, c; char ch; };

static const Cell SHADE[]   = {{3,4,'L'},{3,5,'L'},{4,3,'L'},{4,4,'L'},{5,3,'L'},
                                {9,12,'D'},{9,13,'D'},{10,11,'D'},{10,12,'D'},{11,11,'D'}};
static const Cell F_IDLE[]  = {{6,4,'W'},{6,5,'W'},{7,4,'W'},{7,5,'P'},
                                {6,10,'W'},{6,11,'W'},{7,10,'P'},{7,11,'W'},{10,7,'K'},{10,8,'K'}};
static const Cell F_WORK[]  = {{6,4,'W'},{6,5,'W'},{7,4,'P'},{7,5,'W'},
                                {6,10,'W'},{6,11,'W'},{7,10,'W'},{7,11,'P'},
                                {10,7,'K'},{10,8,'K'},{11,7,'W'},{11,8,'W'}};
static const Cell F_MAXED[] = {{6,4,'K'},{7,5,'K'},{6,11,'K'},{7,10,'K'},{8,3,'R'},{8,12,'R'},
                                {10,6,'K'},{10,7,'K'},{10,8,'K'},{10,9,'K'}};
static const Cell F_OFF[]   = {{7,4,'K'},{7,5,'K'},{7,10,'K'},{7,11,'K'},{10,7,'K'},{10,8,'K'}};

static uint16_t palCol(char ch) {
    switch (ch) {
        case 'K': case 'P': return C_OUTLINE;
        case 'C': return C_CORAL;
        case 'L': return C_LITE;
        case 'D': return C_CLAY;
        case 'W': return C_WHITE;
        case 'R': return C_BLUSH;
        default:  return 0;
    }
}

static void applyCells(char g[16][16], const Cell* cells, int n) {
    for (int i = 0; i < n; i++) g[cells[i].r][cells[i].c] = cells[i].ch;
}

static void drawMascot(lgfx::LovyanGFX& gfx, int x, int y, int scale, BuddyState st) {
    char g[16][16];
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 16; c++)
            g[r][c] = BODY[r][c];

    applyCells(g, SHADE, sizeof(SHADE)/sizeof(Cell));
    if      (st == ST_WORKING) applyCells(g, F_WORK,  sizeof(F_WORK)/sizeof(Cell));
    else if (st == ST_MAXED)   applyCells(g, F_MAXED, sizeof(F_MAXED)/sizeof(Cell));
    else if (st == ST_OFFLINE) applyCells(g, F_OFF,   sizeof(F_OFF)/sizeof(Cell));
    else                       applyCells(g, F_IDLE,  sizeof(F_IDLE)/sizeof(Cell));

    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 16; c++) {
            char ch = g[r][c];
            if (ch == '.') continue;
            gfx.fillRect(x + c*scale, y + r*scale, scale, scale, palCol(ch));
        }

    if (st == ST_WORKING) {
        gfx.fillRect(x+scale*15, y+scale*1, 4, 4, C_CORAL);
        gfx.fillRect(x+scale*1,  y+scale*0, 3, 3, C_CORAL);
        gfx.fillRect(x+scale*14, y+scale*9, 4, 4, C_CORAL);
    } else if (st == ST_MAXED) {
        int dx = x+scale*14, dy = y+scale*5;
        gfx.fillTriangle(dx+2, dy, dx, dy+4, dx+4, dy+4, C_WHITE);
        gfx.fillCircle(dx+2, dy+5, 2, C_WHITE);
        gfx.drawCircle(dx+2, dy+5, 2, C_INK);
    } else if (st == ST_OFFLINE) {
        gfx.setFont(&fonts::Font0);
        gfx.setTextSize(1);
        gfx.setTextColor(C_INKSOFT, C_BG2);
        gfx.drawString("z", x+scale*14, y+scale*4);
        gfx.drawString("z", x+scale*15, y+scale*2);
        gfx.drawString("z", x+scale*16, y+scale*0);
    }
}

// ── Usage bar (12 segments) ───────────────────────────────────────────────────
static void drawBar(lgfx::LovyanGFX& gfx, const char* label, int y, int pct, bool alert, bool off) {
    gfx.setFont(&fonts::Font0);
    gfx.setTextSize(1);
    gfx.setTextColor(C_INKSOFT, C_BG);
    gfx.setTextDatum(middle_left);
    gfx.drawString(label, 8, y + 5);

    const int bx = 34, bw = 244, segs = 12, gap = 2;
    const int sw = (bw - (segs-1)*gap) / segs;
    int filled = off ? 0 : (pct * segs + 50) / 100;
    for (int i = 0; i < segs; i++) {
        uint16_t col = C_GHOST;
        if (i < filled)
            col = alert ? C_ALERT : (i == filled-1 ? C_LITE : C_CORAL);
        gfx.fillRect(bx + i*(sw+gap), y, sw, 10, col);
    }

    char val[8];
    if (off) strcpy(val, "--");
    else snprintf(val, sizeof(val), "%d%%", pct);
    gfx.setTextDatum(middle_right);
    gfx.setTextColor(alert ? C_ALERT : C_INK, C_BG);
    gfx.drawString(val, 320 - 6, y + 5);
    gfx.setTextDatum(top_left);
}

// ── Full-screen render ────────────────────────────────────────────────────────
void buddyRender(lgfx::LovyanGFX& gfx, const UI& ui) {
    bool off    = (ui.state == ST_OFFLINE);
    uint16_t ink = off ? C_INKSOFT : C_INK;

    gfx.fillRect(0, 0, 320, 240, C_BG);
    gfx.fillRect(0, 0, 320,  60, C_BG2);   // lit top band

    // Header
    gfx.setFont(&fonts::Font0);
    gfx.setTextSize(1);
    gfx.setTextDatum(middle_left);
    gfx.setTextColor(C_CORAL, C_BG2);
    gfx.drawString("/", 8, 9);
    gfx.setTextColor(ink, C_BG2);
    gfx.drawString("claudebuddy", 13, 9);
    gfx.setTextDatum(middle_right);
    gfx.setTextColor(C_INKSOFT, C_BG2);
    gfx.drawString(off ? "no link" : "live", 320 - 16, 9);
    if (off) gfx.drawCircle(320-8, 9, 3, C_INKSOFT);
    else     gfx.fillCircle(320-8, 9, 3, C_CORAL);
    gfx.drawFastHLine(0, 18, 320, C_GHOST);
    gfx.setTextDatum(top_left);

    // Mascot (x=14, y=50, scale=6 → 96×96px)
    drawMascot(gfx, 14, 50, 6, ui.state);

    // Status column
    const int sx = 122;
    gfx.setFont(&fonts::Font0);

    if (ui.state == ST_MAXED) {
        gfx.setTextSize(1);
        gfx.setTextColor(C_INKSOFT, C_BG2);
        gfx.drawString("MAXED \xB7 BACK IN", sx, 60);
        gfx.setTextSize(3);
        gfx.setTextColor(C_ALERT, C_BG2);
        gfx.drawString(ui.resetIn[0] ? ui.resetIn : "--:--", sx, 78);
        gfx.setTextSize(1);
        gfx.setTextColor(C_CLAY, C_BG);
        char line[28];
        snprintf(line, sizeof(line), "at %s \xB7 0 left", ui.resetAt[0] ? ui.resetAt : "?");
        gfx.drawString(line, sx, 112);
    } else {
        const char* big = ui.state == ST_WORKING ? "WORKING"
                        : ui.state == ST_OFFLINE  ? "OFFLINE" : "READY";
        gfx.setTextSize(2);
        gfx.setTextColor(ink, ui.state == ST_OFFLINE ? C_BG2 : C_BG);
        gfx.drawString(big, sx, 74);
        gfx.setTextSize(1);
        gfx.setTextColor(C_INKSOFT, C_BG);
        if (ui.state == ST_WORKING) {
            gfx.drawString("spending tokens", sx, 100);
            gfx.setTextColor(C_CLAY, C_BG);
            gfx.drawString(ui.note[0] ? ui.note : "...", sx, 114);
        } else if (ui.state == ST_OFFLINE) {
            gfx.setTextColor(C_INKSOFT, C_BG2);
            gfx.drawString("mqtt: no link", sx, 100);
            gfx.drawString("check broker", sx, 114);
        } else {
            gfx.drawString("claude is idle", sx, 100);
            gfx.drawString("waiting...", sx, 114);
        }
    }

    // Footer
    gfx.drawFastHLine(0, 178, 320, C_GHOST);
    drawBar(gfx, "5H", 240 - 44, ui.h5, ui.h5 >= 90, off);
    drawBar(gfx, "WK", 240 - 22, ui.wk, ui.wk >= 90, off);
}
