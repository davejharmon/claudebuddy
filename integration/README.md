# claudebuddy — UI/UX handoff

Design assets for the Claude Code desk buddy on **M5Stack Core2** (320×240).
This is the **visual layer only** — your prototype already owns WiFi, MQTT, and state logic.
Everything here tells Claude Code exactly how the four states should *look* and *behave*.

## Start here
1. **`UI-SPEC.md`** — the authoritative spec: zones & coordinates, palette (hex + RGB565),
   type sizes, mascot rules, bar logic, per-state copy, wake/brightness, MQTT field reference.
2. **`Claude Desk Buddy.html`** — open in a browser. Interactive board showing all four states
   inside a true-to-life bezel. The "source of truth" for the look.
3. **`assets/states/*.png`** — pixel-exact 320×240 frames to match against on the panel.

## Assets
| File | What it's for |
|------|---------------|
| `assets/mascot.json` | The 16×16 pixel mascot — `body` + `shade` + per-state `faces` + `palette`. **Swappable**: replace these arrays to redraw the character. |
| `assets/palette.h` | Color tokens as hex **and** RGB565 (`#define`s) for ILI9342C / M5GFX. |
| `assets/reference_render.cpp` | Drawing-only renderer (M5GFX). Lift `drawMascot()`, `drawBar()`, `buddyRender()` straight into your sketch — no networking, no `loop()`. Feed it your parsed `UI` struct. |
| `assets/screen-export.html` | Regenerate the reference PNGs after a design tweak. Open with `#0`–`#3` in the URL to pick a state, screenshot the top-left 320×240. |
| `assets/states/01-offline.png` | NOT CONNECTED — dim, dozing |
| `assets/states/02-maxed.png` | MAXED — big `1H 29M` countdown to next window |
| `assets/states/03-working.png` | WORKING — spending tokens, sparkles |
| `assets/states/04-idle.png` | IDLE — connected, content, waiting |

## Integration sketch
```
your MQTT callback ─► parse JSON ─► fill UI{ state, h5, wk, resetIn, resetAt, note }
                                          │
                       buddyInitPalette() │  (once, after M5.begin)
                                          ▼
                                   buddyRender(canvas, ui)  ─► canvas.pushSprite(0,0)
                                          │
                       setBrightness() per state  (working 255 → offline 40)
```

`offline` is decided **on the device** (link down / payload stale), not sent by the host.
See UI-SPEC §6–7 for the state table and the JSON contract your publisher should emit.

## Mascot note
Original pixel character, inspired by Clawd but not the proprietary art — swap in your own by
editing `mascot.json` (keep the palette char keys) and re-exporting the PNGs.
