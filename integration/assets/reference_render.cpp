// ============================================================
//  claudebuddy — REFERENCE RENDERER  (drawing layer only)
//
//  This is NOT a full sketch. It has no WiFi/MQTT/setup/loop — you
//  already have those. It is the pixel-exact drawing layer that
//  matches "Claude Desk Buddy.html", ready to lift into your project.
//
//  Drop your parsed state into the `ui` struct and call render().
//  Built for M5Unified / M5GFX, but the geometry & palette are
//  framework-agnostic (see UI-SPEC.md + assets/palette.h).
//
//  Coordinate system: 320 x 240, setRotation(1).
//    header  y 0..18   |  content y 18..178  |  footer y 178..240
// ============================================================
#include <M5Unified.h>

// ---------- warm Claude LCD palette (set once in your setup) ----------
static uint16_t C_BG, C_BG2, C_INK, C_INKSOFT, C_CORAL, C_LITE, C_CLAY,
                C_WHITE, C_ALERT, C_GHOST, C_BLUSH;

void buddyInitPalette() {
  C_BG     = M5.Display.color565(0xE9,0xE1,0xCF); // lit cream
  C_BG2    = M5.Display.color565(0xEF,0xE7,0xD7); // top band
  C_INK    = M5.Display.color565(0x2B,0x27,0x22); // dark pixels
  C_INKSOFT= M5.Display.color565(0x6E,0x65,0x59); // secondary text
  C_CORAL  = M5.Display.color565(0xD9,0x77,0x57); // Claude coral
  C_LITE   = M5.Display.color565(0xEC,0xB0,0x87); // highlight / bar lead
  C_CLAY   = M5.Display.color565(0xB5,0x53,0x2F); // shadow / accent text
  C_WHITE  = M5.Display.color565(0xF6,0xEF,0xE0); // eye white
  C_ALERT  = M5.Display.color565(0xC0,0x49,0x2B); // maxed / >=90%
  C_GHOST  = M5.Display.color565(0xCF,0xC6,0xB2); // unlit segment / hairline
  C_BLUSH  = M5.Display.color565(0xCF,0x6A,0x49); // exertion blush
}

static const int W = 320, H = 240, HDR_H = 18, FTR_H = 62;

enum State { ST_OFFLINE, ST_IDLE, ST_WORKING, ST_MAXED };

struct UI {
  State state = ST_OFFLINE;
  int   h5 = 0, wk = 0;        // window usage %
  char  resetIn[16] = "";      // "1H 29M"  (maxed)
  char  resetAt[16] = "";      // "3:00p"   (maxed)
  char  note[28]    = "";      // "+12.4k this turn" (working)
};

// ============================================================
//  PIXEL MASCOT — 16x16, identical to assets/mascot.json
// ============================================================
static const char* BODY[16] = {
  "................","......KKKK......","....KKCCCCKK....","...KCCCCCCCCK...",
  "..KCCCCCCCCCCK..","..KCCCCCCCCCCK..",".KCCCCCCCCCCCCK.",".KCCCCCCCCCCCCK.",
  ".KCCCCCCCCCCCCK.",".KCCCCCCCCCCCCK.","..KCCCCCCCCCCK..","..KCCCCCCCCCCK..",
  "...KCCCCCCCCK...","....KKCCCCKK....","......KKKK......","................",
};
struct Cell { uint8_t r, c; char ch; };
static const Cell SHADE[]  = {{3,4,'L'},{3,5,'L'},{4,3,'L'},{4,4,'L'},{5,3,'L'},
                              {9,12,'D'},{9,13,'D'},{10,11,'D'},{10,12,'D'},{11,11,'D'}};
static const Cell F_IDLE[] = {{6,4,'W'},{6,5,'W'},{7,4,'W'},{7,5,'P'},
                              {6,10,'W'},{6,11,'W'},{7,10,'P'},{7,11,'W'},{10,7,'K'},{10,8,'K'}};
static const Cell F_WORK[] = {{6,4,'W'},{6,5,'W'},{7,4,'P'},{7,5,'W'},
                              {6,10,'W'},{6,11,'W'},{7,10,'W'},{7,11,'P'},
                              {10,7,'K'},{10,8,'K'},{11,7,'W'},{11,8,'W'}};
static const Cell F_MAXED[]= {{6,4,'K'},{7,5,'K'},{6,11,'K'},{7,10,'K'},{8,3,'R'},{8,12,'R'},
                              {10,6,'K'},{10,7,'K'},{10,8,'K'},{10,9,'K'}};
static const Cell F_OFF[]  = {{7,4,'K'},{7,5,'K'},{7,10,'K'},{7,11,'K'},{10,7,'K'},{10,8,'K'}};

static uint16_t palCol(char ch) {
  switch (ch) {
    case 'K': case 'P': return C_INK;
    case 'C': return C_CORAL; case 'L': return C_LITE; case 'D': return C_CLAY;
    case 'W': return C_WHITE; case 'R': return C_BLUSH;
    default:  return 0; // transparent
  }
}
static void applyCells(char g[16][16], const Cell* cells, int n) {
  for (int i = 0; i < n; i++) g[cells[i].r][cells[i].c] = cells[i].ch;
}

// Draw mascot at (x,y) with pixel `scale` (design uses 6 on-device, 7 in HTML).
static void drawMascot(M5GFX& gfx, int x, int y, int scale, State st) {
  char g[16][16];
  for (int r = 0; r < 16; r++) for (int c = 0; c < 16; c++) g[r][c] = BODY[r][c];
  applyCells(g, SHADE, sizeof(SHADE)/sizeof(Cell));
  if      (st == ST_WORKING) applyCells(g, F_WORK,  sizeof(F_WORK)/sizeof(Cell));
  else if (st == ST_MAXED)   applyCells(g, F_MAXED, sizeof(F_MAXED)/sizeof(Cell));
  else if (st == ST_OFFLINE) applyCells(g, F_OFF,   sizeof(F_OFF)/sizeof(Cell));
  else                       applyCells(g, F_IDLE,  sizeof(F_IDLE)/sizeof(Cell));

  for (int r = 0; r < 16; r++) for (int c = 0; c < 16; c++) {
    char ch = g[r][c]; if (ch == '.') continue;
    gfx.fillRect(x + c*scale, y + r*scale, scale, scale, palCol(ch));
  }

  if (st == ST_WORKING) {               // sparkles
    gfx.fillRect(x+scale*15, y+scale*1, 4, 4, C_CORAL);
    gfx.fillRect(x+scale*1,  y+scale*0, 3, 3, C_CORAL);
    gfx.fillRect(x+scale*14, y+scale*9, 4, 4, C_CORAL);
  } else if (st == ST_MAXED) {          // sweat drop
    int dx = x+scale*14, dy = y+scale*5;
    gfx.fillTriangle(dx+2, dy, dx, dy+4, dx+4, dy+4, C_WHITE);
    gfx.fillCircle(dx+2, dy+5, 2, C_WHITE);
    gfx.drawCircle(dx+2, dy+5, 2, C_INK);
  } else if (st == ST_OFFLINE) {        // z z z
    gfx.setFont(&fonts::Font0); gfx.setTextSize(1); gfx.setTextColor(C_INKSOFT);
    gfx.drawString("z", x+scale*14, y+scale*4);
    gfx.drawString("z", x+scale*15, y+scale*2);
    gfx.drawString("z", x+scale*16, y+scale*0);
  }
}

// ============================================================
//  SEGMENTED USAGE BAR  (12 segments)
// ============================================================
static void drawBar(M5GFX& gfx, const char* label, int y, int pct, bool alert, bool off) {
  gfx.setFont(&fonts::Font0); gfx.setTextSize(1);
  gfx.setTextColor(C_INKSOFT); gfx.setTextDatum(middle_left);
  gfx.drawString(label, 8, y + 5);

  const int bx = 34, bw = 244, segs = 12, gap = 2;
  const int sw = (bw - (segs-1)*gap) / segs;
  int filled = off ? 0 : (pct * segs + 50) / 100;
  for (int i = 0; i < segs; i++) {
    uint16_t col = C_GHOST;
    if (i < filled) col = alert ? C_ALERT : (i == filled-1 ? C_LITE : C_CORAL);
    gfx.fillRect(bx + i*(sw+gap), y, sw, 10, col);
  }
  char val[8];
  if (off) strcpy(val, "--"); else snprintf(val, sizeof(val), "%d%%", pct);
  gfx.setTextDatum(middle_right);
  gfx.setTextColor(alert ? C_ALERT : C_INK);
  gfx.drawString(val, W - 6, y + 5);
  gfx.setTextDatum(top_left);
}

// ============================================================
//  FULL SCREEN  — render into any M5GFX target (display or sprite)
// ============================================================
void buddyRender(M5GFX& gfx, const UI& ui) {
  bool off = (ui.state == ST_OFFLINE);
  uint16_t ink = off ? C_INKSOFT : C_INK;

  gfx.fillRect(0, 0, W, H, C_BG);
  gfx.fillRect(0, 0, W, 60, C_BG2);              // lit top band

  // header
  gfx.setFont(&fonts::Font0); gfx.setTextSize(1); gfx.setTextDatum(middle_left);
  gfx.setTextColor(C_CORAL); gfx.drawString("/", 8, HDR_H/2);
  gfx.setTextColor(ink);     gfx.drawString("claudebuddy", 13, HDR_H/2);
  gfx.setTextDatum(middle_right); gfx.setTextColor(C_INKSOFT);
  gfx.drawString(off ? "no link" : "live", W - 16, HDR_H/2);
  if (off) gfx.drawCircle(W - 8, HDR_H/2, 3, C_INKSOFT);
  else     gfx.fillCircle(W - 8, HDR_H/2, 3, C_CORAL);
  gfx.drawFastHLine(0, HDR_H, W, C_GHOST);
  gfx.setTextDatum(top_left);

  // mascot
  drawMascot(gfx, 14, 50, 6, ui.state);

  // status column
  const int sx = 122;
  if (ui.state == ST_MAXED) {
    gfx.setTextSize(1); gfx.setTextColor(C_INKSOFT);
    gfx.drawString("MAXED \xB7 BACK IN", sx, 60);
    gfx.setTextSize(3); gfx.setTextColor(C_ALERT);
    gfx.drawString(ui.resetIn[0] ? ui.resetIn : "--:--", sx, 78);
    gfx.setTextSize(1); gfx.setTextColor(C_CLAY);
    char line[28];
    snprintf(line, sizeof(line), "at %s \xB7 0 left", ui.resetAt[0] ? ui.resetAt : "?");
    gfx.drawString(line, sx, 112);
  } else {
    const char* big = ui.state == ST_WORKING ? "WORKING"
                    : ui.state == ST_OFFLINE ? "OFFLINE" : "READY";
    gfx.setTextSize(2); gfx.setTextColor(ink); gfx.drawString(big, sx, 74);
    gfx.setTextSize(1); gfx.setTextColor(C_INKSOFT);
    if (ui.state == ST_WORKING) {
      gfx.drawString("spending tokens", sx, 100);
      gfx.setTextColor(C_CLAY); gfx.drawString(ui.note[0] ? ui.note : "...", sx, 114);
    } else if (ui.state == ST_OFFLINE) {
      gfx.drawString("mqtt: no link", sx, 100);
      gfx.drawString("check broker", sx, 114);
    } else {
      gfx.drawString("claude is idle", sx, 100);
      gfx.drawString("waiting...", sx, 114);
    }
  }

  // footer: two usage bars
  gfx.drawFastHLine(0, H - FTR_H, W, C_GHOST);
  drawBar(gfx, "5H", H - 44, ui.h5, ui.h5 >= 90, off);
  drawBar(gfx, "WK", H - 22, ui.wk, ui.wk >= 90, off);
}
