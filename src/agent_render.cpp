// Agent board: a scrollable list of Claude Code sessions, NEEDS YOU first.
#include "agent_render.h"
#include "agents.h"

static const int ROW_Y0   = BUDDY_HEADER_H + 4;
static const int ROW_H    = 42;          // 5 rows: 26 + 5*42 = 236
static const int ICON_X   = 19;          // state icon centre
static const int ICON_R   = 10;
static const int TEXT_X   = ICON_X + ICON_R + 7;
static const int TEXT_W   = 312 - TEXT_X;

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

// Round state icon: DONE green tick, WORKING coral spinner, QUIET grey dots,
// NEEDS YOU amber "!", ERROR red cross. Accent colours read on both themes.
static void drawStateIcon(lgfx::LovyanGFX& gfx, AgentState st, bool quiet, int cx, int cy, int phase) {
    const uint16_t dark = M5.Display.color565(0x2B, 0x27, 0x22);
    const int r = ICON_R;
    switch (st) {
        case AG_DONE:
            gfx.fillCircle(cx, cy, r, g_pal.green);
            gfx.drawWideLine(cx - 5, cy + 0, cx - 2, cy + 4, 1.3f, g_pal.white);
            gfx.drawWideLine(cx - 2, cy + 4, cx + 5, cy - 4, 1.3f, g_pal.white);
            break;
        case AG_NEEDS_YOU:
            gfx.fillCircle(cx, cy, r, g_pal.amber);
            gfx.fillRoundRect(cx - 1, cy - 6, 3, 8, 1, dark);
            gfx.fillRect(cx - 1, cy + 4, 3, 3, dark);
            break;
        case AG_ERROR:
            gfx.fillCircle(cx, cy, r, g_pal.alert);
            gfx.drawWideLine(cx - 4, cy - 4, cx + 4, cy + 4, 1.3f, g_pal.white);
            gfx.drawWideLine(cx - 4, cy + 4, cx + 4, cy - 4, 1.3f, g_pal.white);
            break;
        default:  // AG_WORKING
            if (quiet) {
                gfx.fillCircle(cx, cy, r, g_pal.ghost);
                gfx.drawCircle(cx, cy, r, g_pal.inkSoft);
                for (int i = -1; i <= 1; i++) gfx.fillCircle(cx + i * 4, cy, 1, g_pal.inkSoft);
            } else {
                gfx.fillCircle(cx, cy, r, g_pal.coral);
                float a0 = phase * 90.0f;             // turns a quarter per redraw
                gfx.fillArc(cx, cy, 6, 4, a0, a0 + 270.0f, g_pal.white);
            }
            break;
    }
}

static void drawRow(lgfx::LovyanGFX& gfx, const Agent& a, int y, int phase) {
    uint32_t age = agentAgeS(a);
    bool quiet = (a.state == AG_WORKING && age > 30 * 60);   // no event for a long time

    gfx.fillRoundRect(4, y, 312, ROW_H - 4, 6, g_pal.bg2);
    if (a.state == AG_NEEDS_YOU) {
        gfx.drawRoundRect(4, y, 312, ROW_H - 4, 6, g_pal.amber);
        gfx.drawRoundRect(5, y + 1, 310, ROW_H - 6, 5, g_pal.amber);
    }

    drawStateIcon(gfx, a.state, quiet, ICON_X, y + (ROW_H - 4) / 2, phase);

    // Name (bold) and detail line
    gfx.setTextDatum(top_left);
    gfx.setFont(&fonts::FreeSansBold9pt7b);
    gfx.setTextColor(a.state == AG_DONE ? g_pal.inkSoft : g_pal.ink, g_pal.bg2);
    drawFit(gfx, a.name[0] ? a.name : "agent", TEXT_X, y + 3, TEXT_W);

    // "msg - age - host - model", skipping any part that's empty
    char ageS[8], detail[96];
    fmtAge(age, ageS, sizeof(ageS));
    const char* parts[] = { a.msg, ageS, a.host, a.model };
    detail[0] = 0;
    for (const char* p : parts) {
        if (!p[0]) continue;
        if (detail[0]) strlcat(detail, " - ", sizeof(detail));
        strlcat(detail, p, sizeof(detail));
    }
    gfx.setFont(&fonts::Font0);
    gfx.setTextColor(g_pal.inkSoft, g_pal.bg2);
    drawFit(gfx, detail, TEXT_X, y + 24, TEXT_W);
}

// Empty board: one centred line on the link / usage state, one hint below.
static void drawEmpty(lgfx::LovyanGFX& gfx, const UI& ui) {
    const char* title;  char sub[48];
    uint16_t titleCol = g_pal.ink;
    switch (ui.state) {
        case ST_CONNECTING: title = "Connecting...";      strlcpy(sub, "mqtt broker", sizeof(sub)); break;
        case ST_OFFLINE:    title = "No link";            strlcpy(sub, "mqtt broker unreachable", sizeof(sub));
                            titleCol = g_pal.inkSoft; break;
        case ST_STALE:      title = "Usage not updating";
                            strlcpy(sub, ui.note[0] ? ui.note : "check daemon", sizeof(sub)); break;
        case ST_MAXED:      title = "Limit reached";      titleCol = g_pal.alert;
                            snprintf(sub, sizeof(sub), "back in %s at %s",
                                     ui.resetIn[0] ? ui.resetIn : "--", ui.resetAt[0] ? ui.resetAt : "?");
                            break;
        default:            title = "No active sessions";
                            strlcpy(sub, "Claude Code sessions show up here", sizeof(sub)); break;
    }
    const int cy = ROW_Y0 + (240 - ROW_Y0) / 2;
    gfx.setTextDatum(middle_center);
    gfx.setFont(&fonts::FreeSansBold9pt7b);
    gfx.setTextColor(titleCol, g_pal.bg);
    gfx.drawString(title, 160, cy - 9);
    gfx.setFont(&fonts::Font0);
    gfx.setTextColor(g_pal.inkSoft, g_pal.bg);
    gfx.drawString(sub, 160, cy + 12);
    gfx.setTextDatum(top_left);
}

void agentsRender(lgfx::LovyanGFX& gfx, const UI& ui, bool linked, int top) {
    gfx.fillRect(0, 0, 320, 240, g_pal.bg);
    buddyDrawHeader(gfx, ui, linked);

    if (g_agentCount == 0) { drawEmpty(gfx, ui); return; }

    int phase = (millis() / 500) % 4;
    for (int r = 0; r < AGENT_ROWS && top + r < g_agentCount; r++)
        drawRow(gfx, g_agents[top + r], ROW_Y0 + r * ROW_H, phase);

    // Scroll indicator when the list is longer than the screen
    if (g_agentCount > AGENT_ROWS) {
        int trackH = 240 - ROW_Y0 - 4;
        int thumbH = max(12, trackH * AGENT_ROWS / g_agentCount);
        int thumbY = ROW_Y0 + (trackH - thumbH) * top / (g_agentCount - AGENT_ROWS);
        gfx.fillRect(317, ROW_Y0, 2, trackH, g_pal.ghost);
        gfx.fillRect(317, thumbY, 2, thumbH, g_pal.inkSoft);
    }
}
