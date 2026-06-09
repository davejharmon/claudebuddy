# claudebuddy — UI/UX Spec

Visual + interaction spec for the Claude Code desk buddy on **M5Stack Core2**
(ILI9342C, 320×240, landscape, `setRotation(1)`).

Authoritative visual reference: **`Claude Desk Buddy.html`** (interactive, all 4 states).
Pixel-exact frames: **`assets/states/*.png`**. Drawing reference: **`assets/reference_render.cpp`**.

---

## 1. Canvas & zones

```
320 × 240, origin top-left. Bottom ~20px sits over the A/B/C touch bezel.

y=0    ┌──────────────────────────────────────────────┐
       │ HEADER 18px   /claudebuddy ········· live ●    │
y=18   ├──────────────────────────────────────────────┤  ← 1px ghost hairline
       │ CONTENT 160px                                 │
       │   ┌ mascot ┐   STATUS BLOCK                   │
       │   │ 96×96  │   big word / countdown           │
       │   └────────┘   two sub lines                  │
y=178  ├──────────────────────────────────────────────┤  ← 1px ghost hairline
       │ FOOTER 62px                                   │
       │   5H ▮▮▮▮▮▯▯▯▯▯▯▯  58%                         │
       │   WK ▮▮▮▮▮▯▯▯▯▯▯▯  47%                         │
y=240  └──────────────────────────────────────────────┘
```

| Element        | x / y                              | Notes |
|----------------|------------------------------------|-------|
| Header text    | `/` at x=8, `claudebuddy` at x=13, baseline mid of 18px | coral slash |
| Link label+dot | right-aligned: label at x=W−16, dot at x=W−8 r=3 | |
| Mascot         | x=14, y=50, **scale 6** → 96×96     | scale 7 in HTML mock |
| Status column  | x=122                               | left-aligned |
| Hairlines      | y=18 and y=178, full width, `ghost` | dashed in HTML, solid 1px on device is fine |
| Bar row 1 (5H) | y=196 (text mid y=H−44)             | |
| Bar row 2 (WK) | y=218 (text mid y=H−22)             | |

---

## 2. Palette  (warm Claude LCD)

Low-saturation creams, ink, coral. Full table with RGB565 in **`assets/palette.h`**.

| Token    | Hex       | Use |
|----------|-----------|-----|
| bg       | `#E9E1CF` | lit cream background |
| bg2      | `#EFE7D7` | lighter top band (y 0–60) for a "lit panel" feel |
| ink      | `#2B2722` | dark pixels, primary text |
| inksoft  | `#6E6559` | secondary text, labels |
| coral    | `#D97757` | accent: slash, live dot, bar fill, sparkles |
| lite     | `#ECB087` | mascot highlight, bar **leading edge** segment |
| clay     | `#B5532F` | mascot shadow, accent sub-text |
| white    | `#F6EFE0` | eye whites, sweat drop |
| alert    | `#C0492B` | maxed countdown, any bar ≥90% |
| ghost    | `#CFC6B2` | unlit bar segments, hairlines |
| blush    | `#CF6A49` | maxed-face cheeks |

---

## 3. Typography

Chunky bitmap type — on device use the built-in **GLCD 6×8 font (`Font0`)** scaled with
`setTextSize()`. (The HTML mock uses *Silkscreen* to emulate it.) No anti-aliasing.

| Role            | Font0 size | px      | Color |
|-----------------|-----------:|---------|-------|
| Header / labels | 1          | 6×8     | ink / inksoft |
| Status word     | 2          | 12×16   | ink (alert if maxed) |
| **Maxed countdown** | 3      | 18×24   | **alert** — the hero, glanceable across a room |
| Sub lines       | 1          | 6×8     | inksoft / clay |
| Bar value (%)   | 1          | 6×8     | ink (alert if ≥90%) |

---

## 4. Mascot

Original pixel character (inspired by Clawd, **swappable**). 16×16 grid in
**`assets/mascot.json`** (also embedded in `reference_render.cpp`).

Build order: `body` → apply `shade` (dimension) → overlay the per-state `faces[state].cells`.
Characters map via `palette`; `.` is transparent.

| State   | Face                                  | Decoration |
|---------|---------------------------------------|------------|
| idle    | round eyes, tiny smile (content)      | — |
| working | focused eyes, small open mouth        | 3 coral sparkles around the head |
| maxed   | strained `\ /` eyes, blush, flat mouth| white sweat drop, top-right |
| offline | closed eyes, neutral mouth            | rising `z z z`, **whole screen dimmed** |

To swap the mascot later: replace `body` + the four `faces` in `mascot.json` (keep the same
char keys) and re-export the reference PNGs with `assets/screen-export.html`.

---

## 5. Usage bars

Two segmented bars = the two Claude usage windows.

- **5H** = rolling 5-hour window. **WK** = weekly window.
- 12 segments, 2px gap, 10px tall. Track x=34→278, value right-aligned at x=W−6.
- `filled = round(pct/100 × 12)`.
- Fill color: `coral`; the **last filled** segment is `lite` (bright leading edge) to signal
  movement — **unless** alert.
- **Alert** (pct ≥ 90): all filled segments + value render in `alert` red.
- **Offline**: 0 filled (all `ghost`), value shows `--`.

---

## 6. The four states

| # | state (MQTT) | header | mascot | status block | 5H / WK | brightness |
|---|--------------|--------|--------|--------------|---------|-----------|
| 1 | `offline`*   | hollow dot · "no link" | dozing, dim | OFFLINE · "mqtt: no link" / "check broker" | `--` / `--`, dimmed | 40 |
| 2 | `maxed`      | coral dot · "live" | strained | kicker "MAXED · BACK IN" → **big `1H 29M`** → "at 3:00p · 0 left" | 100% (alert) / 84% | 190 |
| 3 | `working`    | coral dot · "live" | sparkling | WORKING / "spending tokens" / `note` e.g. "+12.4k this turn" | 58%↑ / 47% | 255 |
| 4 | `idle`       | coral dot · "live" | content | READY / "claude is idle" / "waiting…" | 33% / 44% | 120 |

\* **offline is decided by the device**, not the host — when the MQTT link is down or the last
payload is stale. The whole content layer is dimmed + low-contrast (brightness 40, text in
`inksoft`). Use this as the "wake to save the panel" baseline.

### Wake behavior
The buddy "wakes" (full brightness) the instant a `working` payload lands, and steps down as it
goes `maxed → idle → offline`. Drive `setBrightness()` straight off the state (column above).

---

## 7. MQTT state contract  *(reference — your host already produces this)*

Topic the device subscribes to (e.g. `claudebuddy/state`), retained, JSON:

```json
{
  "state":    "working",          // working | idle | maxed | offline
  "h5_pct":   58,                  // 0–100, 5-hour window
  "wk_pct":   47,                  // 0–100, weekly window
  "reset_in": "1H 29M",           // maxed only — the big countdown
  "reset_at": "3:00p",            // maxed only — when tokens return
  "note":     "+12.4k this turn"  // working only — small accent line
}
```

Unused fields can be omitted. The device falls back to `offline` if no message arrives within
its staleness window or the broker drops.

---

## 8. Files in this handoff

```
integration/
├─ UI-SPEC.md                 ← this file
├─ README.md                  ← orientation + how to use each asset
├─ Claude Desk Buddy.html     ← interactive visual reference, all 4 states
└─ assets/
   ├─ mascot.json             ← 16×16 mascot data (body + shade + faces + palette)
   ├─ palette.h               ← hex + RGB565 color tokens
   ├─ reference_render.cpp    ← drawing-only M5GFX renderer (no networking)
   ├─ screen-export.html      ← regenerate the PNGs after a design tweak
   └─ states/                 ← pixel-exact 320×240 frames
      ├─ 01-offline.png
      ├─ 02-maxed.png
      ├─ 03-working.png
      └─ 04-idle.png
```
