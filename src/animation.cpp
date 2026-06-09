#include "animation.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static uint16_t hexToRGB565(const char* hex) {
    if (!hex || hex[0] != '#') return 0x0000;
    long c = strtol(hex + 1, nullptr, 16);
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8)  & 0xFF;
    uint8_t b =  c        & 0xFF;
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

bool loadAnimation(const char* path, Animation& out) {
    out.valid = false;

    File f = LittleFS.open(path, "r");
    if (!f) {
        Serial.printf("loadAnimation: cannot open %s\n", path);
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("loadAnimation: JSON error %s in %s\n", err.c_str(), path);
        return false;
    }

    // Parse palette
    JsonArray palArr = doc["palette"].as<JsonArray>();
    out.transparent_idx = 0;
    uint8_t pi = 0;
    for (JsonVariant v : palArr) {
        if (pi >= MAX_PALETTE) break;
        const char* cs = v.as<const char*>();
        if (!cs) { pi++; continue; }
        if (strcmp(cs, "transparent") == 0) {
            out.transparent_idx = pi;
            out.palette[pi] = 0x0000;
        } else {
            out.palette[pi] = hexToRGB565(cs);
        }
        pi++;
    }

    // Parse frames
    JsonArray frames = doc["frames"].as<JsonArray>();
    out.frame_count = 0;
    for (JsonObject frame : frames) {
        uint8_t fi = out.frame_count;
        if (fi >= MAX_FRAMES) break;
        out.hold_ms[fi] = frame["hold"] | 100;
        JsonArray grid = frame["grid"].as<JsonArray>();
        uint8_t row = 0;
        for (JsonArray rowArr : grid) {
            if (row >= ANIM_DIM) break;
            uint8_t col = 0;
            for (JsonVariant pv : rowArr) {
                if (col >= ANIM_DIM) break;
                out.pixels[fi][row][col] = pv.as<uint8_t>();
                col++;
            }
            row++;
        }
        out.frame_count++;
    }

    out.valid = (out.frame_count > 0);
    Serial.printf("loadAnimation: %s → %d frames\n", path, out.frame_count);
    return out.valid;
}
