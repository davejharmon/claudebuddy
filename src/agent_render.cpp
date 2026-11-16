// Agent board: a scrollable list of Claude Code sessions, NEEDS YOU first.
#include "agent_render.h"
#include "agents.h"

static const int HEADER_H = 22;
static const int ROW_Y0   = 26;
static const int ROW_H    = 42;          // 5 rows: 26 + 5*42 = 236
static const int PILL_W   = 92;
static const int TEXT_X   = 8 + PILL_W + 10;
static const int TEXT_W   = 306 - TEXT_X;

// Draw `s` clipped with "..." so it fits in `maxW` pixels in the current font.
static void drawFit(lgfx::LovyanGFX& gfx, const char* s, int x, int y, int maxW) {
    if (gfx.textWidth(s) <= maxW) { gfx.drawString(s, x, y); return; }
    char buf[48];
    strlcpy(buf, s, sizeof(buf));
    int n = strlen(buf);
    while (n > 0) {
        buf[--n] = '\0';
        char tmp[52];
        snprintf(tmp, sizeof(tmp), "%s...", buf);
        if (gfx.textWidth(tmp) <= maxW) { gfx.drawString(tmp, x, y); return; }
    }
}

static void fmtAge(uint32_t s, char* out, size_t n) {
    if (s < 60)        snprintf(out, n, "now");
    else if (s < 3600) snprintf(out, n, "%um", (unsigned)(s / 60));
    else               snprintf(out, n, "%uh", (unsigned)(s / 3600));
}

static void drawHeader(lgfx::LovyanGFX& gfx, const UI& ui, bool linked) {
    gfx.fillRect(0, 0, 320, HEADER_H, g_pal.bg2);
    gfx.setFont(&fonts::Font0);
    gfx.setTextSize(1);
    gfx.setTextDatum(middle_left);

    // Left: needs-you / working counts, or the name when nothing is going on.
    int needs = agentsCount(AG_NEEDS_YOU), working = agentsCount(AG_WORKING);
    char left[32];
    if (needs)        snprintf(left, sizeof(left), "%d need%s you", needs, needs == 1 ? "s" : "");
    else if (working) snprintf(left, sizeof(left), "%d working", working);
    else              snprintf(left, sizeof(left), "claudebuddy");
    gfx.setTextColor(g_pal.coral, g_pal.bg2);
    gfx.drawString("/", 8, 11);
    gfx.setTextColor(needs ? g_pal.clay : g_pal.ink, g_pal.bg2);
    gfx.drawString(left, 15, 11);

    // Right: usage, link dot, battery.
    bool usageOk = linked && (ui.state == ST_IDLE || ui.state == ST_WORKING || ui.state == ST_MAXED);
    char usage[32];
    if (usageOk) snprintf(usage, sizeof(usage), "5h %d%%  7d %d%%", ui.h5, ui.wk);
    else         snprintf(usage, sizeof(usage), "5h --  7d --");
    gfx.setTextDatum(middle_right);
    gfx.setTextColor((usageOk && (ui.h5 >= 90 || ui.wk >= 90)) ? g_pal.alert : g_pal.inkSoft, g_pal.bg2);
    gfx.drawString(usage, 284, 11);
    if (!linked)        gfx.drawCircle(292, 11, 3, g_pal.inkSoft);
    else if (!usageOk)  gfx.fillCircle(292, 11, 3, g_pal.amber);
    else                gfx.fillCircle(292, 11, 3, g_pal.coral);
    if (ui.battery >= 0) buddyDrawBattery(gfx, 300, 7, ui.battery, ui.charging);
    gfx.drawFastHLine(0, HEADER_H, 320, g_pal.ghost);
    gfx.setTextDatum(top_left);
}

static void drawRow(lgfx::LovyanGFX& gfx, const Agent& a, int y, bool blinkOn) {
    const uint16_t dark = M5.Display.color565(0x2B, 0x27, 0x22);
    uint32_t age = agentAgeS(a);
    bool quiet = (a.state == AG_WORKING && age > 30 * 60);   // no event for a long time

    const char* label;  uint16_t pill, pillInk;
    switch (a.state) {
        case AG_NEEDS_YOU: label = "NEEDS YOU"; pill = g_pal.amber;  pillInk = dark;        break;
        case AG_ERROR:     label = "ERROR";     pill = g_pal.alert;  pillInk = g_pal.white; break;
        case AG_WORKING:   label = quiet ? "QUIET" : "WORKING";
                           pill = quiet ? g_pal.ghost : g_pal.coral;
                           pillInk = quiet ? g_pal.inkSoft : g_pal.white;               break;
        default:           label = "DONE";      pill = g_pal.ghost;  pillInk = g_pal.inkSoft; break;
    }

    gfx.fillRoundRect(4, y, 312, ROW_H - 4, 6, g_pal.bg2);
    if (a.state == AG_NEEDS_YOU) {
        gfx.drawRoundRect(4, y, 312, ROW_H - 4, 6, g_pal.amber);
        gfx.drawRoundRect(5, y + 1, 310, ROW_H - 6, 5, g_pal.amber);
    }

    // State pill
    gfx.fillRoundRect(8, y + 8, PILL_W, 22, 5, pill);
    gfx.setFont(&fonts::Font0);
    gfx.setTextSize(1);
    gfx.setTextDatum(middle_center);
    gfx.setTextColor(pillInk, pill);
    gfx.drawString(label, 8 + PILL_W / 2, y + 19);
    if (a.state == AG_WORKING && !quiet && blinkOn)          // activity blip
        gfx.fillCircle(8 + PILL_W - 7, y + 12, 2, g_pal.white);

    // Name (bold) and detail line
    gfx.setTextDatum(top_left);
    gfx.setFont(&fonts::FreeSansBold9pt7b);
    gfx.setTextColor(a.state == AG_DONE ? g_pal.inkSoft : g_pal.ink, g_pal.bg2);
    drawFit(gfx, a.name[0] ? a.name : "agent", TEXT_X, y + 3, TEXT_W);

    char ageS[8], detail[64];
    fmtAge(age, ageS, sizeof(ageS));
    if (a.msg[0] && a.host[0]) snprintf(detail, sizeof(detail), "%s - %s - %s", a.msg, ageS, a.host);
    else if (a.msg[0])         snprintf(detail, sizeof(detail), "%s - %s", a.msg, ageS);
    else if (a.host[0])        snprintf(detail, sizeof(detail), "%s - %s", ageS, a.host);
    else                       snprintf(detail, sizeof(detail), "%s", ageS);
    gfx.setFont(&fonts::Font0);
    gfx.setTextColor(g_pal.inkSoft, g_pal.bg2);
    drawFit(gfx, detail, TEXT_X, y + 24, TEXT_W);
}

void agentsRender(lgfx::LovyanGFX& gfx, const UI& ui, bool linked, int top) {
    gfx.fillRect(0, 0, 320, 240, g_pal.bg);
    drawHeader(gfx, ui, linked);

    bool blinkOn = (millis() / 500) % 2;
    for (int r = 0; r < AGENT_ROWS && top + r < g_agentCount; r++)
        drawRow(gfx, g_agents[top + r], ROW_Y0 + r * ROW_H, blinkOn);

    // Scroll indicator when the list is longer than the screen
    if (g_agentCount > AGENT_ROWS) {
        int trackH = 240 - ROW_Y0 - 4;
        int thumbH = max(12, trackH * AGENT_ROWS / g_agentCount);
        int thumbY = ROW_Y0 + (trackH - thumbH) * top / (g_agentCount - AGENT_ROWS);
        gfx.fillRect(317, ROW_Y0, 2, trackH, g_pal.ghost);
        gfx.fillRect(317, thumbY, 2, thumbH, g_pal.inkSoft);
    }
}
