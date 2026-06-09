// ============================================================
//  claudebuddy — warm Claude LCD palette
//  Hex (sRGB) + RGB565 for ILI9342C / M5GFX.
//  Keep saturation low on the creams; the panel reads warm.
// ============================================================
#pragma once
#include <cstdint>

// --- as packed RGB565 (big-endian-friendly literals) ---
#define BUDDY_BG        0xE73A   // #E9E1CF  lit cream background
#define BUDDY_BG2       0xEF3A   // #EFE7D7  lighter top band
#define BUDDY_INK       0x29A4   // #2B2722  dark pixels / primary text
#define BUDDY_INKSOFT   0x6B2B   // #6E6559  secondary text
#define BUDDY_CORAL     0xDBAA   // #D97757  Claude coral (accent / bar fill)
#define BUDDY_LITE      0xED90   // #ECB087  highlight / bar leading edge
#define BUDDY_CLAY      0xB2A5   // #B5532F  shadow / accent text
#define BUDDY_WHITE     0xF77C   // #F6EFE0  eye white
#define BUDDY_ALERT     0xC265   // #C0492B  maxed / >=90% usage
#define BUDDY_GHOST     0xCE36   // #CFC6B2  unlit segment / hairlines
#define BUDDY_BLUSH     0xCB69   // #CF6A49  exertion blush

// --- prefer this if you have M5GFX up: exact, no rounding guesswork ---
// uint16_t BG = M5.Display.color565(0xE9,0xE1,0xCF);  etc.
//
// hex reference:
//   bg     #E9E1CF      ink      #2B2722      coral  #D97757
//   bg2    #EFE7D7      inksoft  #6E6559      lite   #ECB087
//   white  #F6EFE0      clay     #B5532F      alert  #C0492B
//   ghost  #CFC6B2      blush    #CF6A49
