// Shared drawing layer: palette, battery icon and the header used by the agent board.
// Palette from integration/assets/reference_render.cpp ("Claude Desk Buddy.html").
// Call buddyInitPalette() once after M5.begin().
#include "buddy_render.h"

static uint16_t C_BG, C_BG2, C_INK, C_INKSOFT, C_CORAL, C_LITE, C_CLAY,
                C_WHITE, C_ALERT, C_GHOST, C_AMBER, C_GREEN;

BuddyPal g_pal;

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
    // Accents are identical in both modes
    C_AMBER   = M5.Display.color565(0xF0,0xC0,0x00);
    C_CORAL   = M5.Display.color565(0xD9,0x77,0x57);
    C_LITE    = M5.Display.color565(0xEC,0xB0,0x87);
    C_CLAY    = M5.Display.color565(0xB5,0x53,0x2F);
    C_WHITE   = M5.Display.color565(0xF6,0xEF,0xE0);
    C_ALERT   = M5.Display.color565(0xC0,0x49,0x2B);
    C_GREEN   = M5.Display.color565(0x4F,0x9A,0x58);  // agent DONE
    g_pal = { C_BG, C_BG2, C_INK, C_INKSOFT, C_GHOST, C_CORAL, C_LITE, C_CLAY,
              C_WHITE, C_ALERT, C_AMBER, C_GREEN };
}

// ── Battery icon ─────────────────────────────────────────────────────────────
void buddyDrawBattery(lgfx::LovyanGFX& gfx, int x, int y, int pct, bool chg) {
    const int bw = 12, bh = 8;
    uint16_t col = chg ? C_CORAL : (pct < 20 ? C_ALERT : (pct < 50 ? C_AMBER : C_INKSOFT));
    gfx.drawRect(x, y, bw, bh, C_INKSOFT);
    gfx.fillRect(x + bw, y + 2, 2, 4, C_INKSOFT);
    int fill = (pct * (bw - 2) + 50) / 100;
    if (fill > 0) gfx.fillRect(x + 1, y + 1, fill, bh - 2, col);
}

// ── Header: [5h ████░░ 58%] [1w ██░░░░ 47%] ● [batt] ──────────────────────────
static const int HB_LABEL_W = 15;   // "5h" + gap
static const int HB_BAR_W   = 92;
static const int HB_PCT_W   = 30;   // room for "100%"

static void drawHeaderBar(lgfx::LovyanGFX& gfx, int x, int cy, const char* label, int pct, bool ok) {
    bool alert = ok && pct >= 90;
    gfx.setTextDatum(middle_left);
    gfx.setTextColor(C_INKSOFT, C_BG2);
    gfx.drawString(label, x, cy);

    int bx = x + HB_LABEL_W;
    gfx.fillRoundRect(bx, cy - 4, HB_BAR_W, 8, 3, C_GHOST);
    if (ok) {
        int p = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
        int w = (p * HB_BAR_W + 50) / 100;
        if (w > 0) gfx.fillRoundRect(bx, cy - 4, w < 6 ? 6 : w, 8, 3, alert ? C_ALERT : C_CORAL);
    }

    char val[8];
    if (ok) snprintf(val, sizeof(val), "%d%%", pct);
    else    strcpy(val, "--");
    gfx.setTextDatum(middle_right);
    gfx.setTextColor(alert ? C_ALERT : (ok ? C_INK : C_INKSOFT), C_BG2);
    gfx.drawString(val, bx + HB_BAR_W + HB_PCT_W, cy);
}

void buddyDrawHeader(lgfx::LovyanGFX& gfx, const UI& ui, bool linked) {
    const int cy = BUDDY_HEADER_H / 2;
    bool usageOk = linked && (ui.state == ST_IDLE || ui.state == ST_WORKING || ui.state == ST_MAXED);

    gfx.fillRect(0, 0, 320, BUDDY_HEADER_H, C_BG2);
    gfx.setFont(&fonts::Font0);
    gfx.setTextSize(1);
    drawHeaderBar(gfx,   5, cy, "5h", ui.h5, usageOk);
    drawHeaderBar(gfx, 150, cy, "1w", ui.wk, usageOk);

    // Link dot: coral = live, amber = connecting or usage stale, hollow = no link
    const int dx = 294;
    if (ui.state == ST_CONNECTING || (linked && !usageOk)) gfx.fillCircle(dx, cy, 3, C_AMBER);
    else if (!linked)                                     gfx.drawCircle(dx, cy, 3, C_INKSOFT);
    else                                                  gfx.fillCircle(dx, cy, 3, C_CORAL);

    if (ui.battery >= 0) buddyDrawBattery(gfx, 302, cy - 4, ui.battery, ui.charging);
    gfx.drawFastHLine(0, BUDDY_HEADER_H, 320, C_GHOST);
    gfx.setTextDatum(top_left);
}
