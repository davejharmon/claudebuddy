#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include "buddy_render.h"
#include "mqtt_handler.h"

static Preferences prefs;

#define SCREEN_TIMEOUT_MS (5UL * 60 * 1000)
static uint32_t   g_last_active = 0;
static bool       g_screen_on   = true;
static bool       g_dark_mode   = false;
static BuddyState g_prev_state   = ST_OFFLINE;
static bool       g_prev_conn    = false;
static uint32_t   g_conn_lost_ms = 0;
#define CONNECTING_TIMEOUT_MS 30000UL

static M5Canvas g_canvas(&M5.Display);

static uint8_t brightForState(BuddyState s) {
    switch (s) {
        case ST_WORKING: return 255;
        case ST_MAXED:   return 190;
        case ST_IDLE:    return 120;
        default:         return 40;
    }
}

static void render() {
    buddyRender(g_canvas, g_ui);
    g_canvas.pushSprite(0, 0);
}

static void screenOn() {
    if (!g_screen_on) {
        M5.Display.wakeup();
        g_screen_on = true;
        render();
    }
    M5.Display.setBrightness(brightForState(g_ui.state));
    g_last_active = millis();
}

static void screenOff() {
    if (g_screen_on) {
        M5.Display.sleep();
        M5.Display.setBrightness(0);
        g_screen_on = false;
    }
}

void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    prefs.begin("buddy", true);           // read-only open
    g_dark_mode = prefs.getBool("dark", false);
    prefs.end();

    buddyInitPalette(g_dark_mode);
    g_canvas.createSprite(320, 240);
    g_ui.state = ST_CONNECTING;
    g_conn_lost_ms = millis();
    mqttSetup();
    g_last_active = millis();
    render();
    M5.Display.setBrightness(brightForState(ST_CONNECTING));
}

void loop() {
    M5.update();
    mqttLoop();
    bool redraw = false;

    // MQTT connection state changed
    if (g_mqtt_connected != g_prev_conn) {
        g_prev_conn = g_mqtt_connected;
        if (!g_mqtt_connected) {
            g_conn_lost_ms = millis();
            g_ui.state = ST_CONNECTING;
        }
        if (g_screen_on) M5.Display.setBrightness(brightForState(g_ui.state));
        redraw = true;
    }

    // Connecting too long → offline
    if (!g_mqtt_connected && g_ui.state == ST_CONNECTING &&
        millis() - g_conn_lost_ms > CONNECTING_TIMEOUT_MS) {
        g_ui.state = ST_OFFLINE;
        redraw = true;
    }

    // New payload arrived
    if (g_ui.fresh) {
        g_ui.fresh = false;
        bool became_working = (g_prev_state != ST_WORKING && g_ui.state == ST_WORKING);
        if (became_working) screenOn();            // wake immediately when Claude starts spending
        g_prev_state = g_ui.state;
        if (g_screen_on) M5.Display.setBrightness(brightForState(g_ui.state));
        redraw = true;
    }

    // Hook: prompt submitted in Claude Code
    if (g_wake) { g_wake = false; screenOn(); redraw = true; }

    // A/B: wake only
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
        screenOn(); redraw = true;
    }
    // C: wake when off, toggle dark mode when on
    if (M5.BtnC.wasPressed()) {
        if (!g_screen_on) {
            screenOn(); redraw = true;
        } else {
            g_dark_mode = !g_dark_mode;
            prefs.begin("buddy", false);
            prefs.putBool("dark", g_dark_mode);
            prefs.end();
            buddyInitPalette(g_dark_mode);
            redraw = true;
        }
    }

    // Idle timeout
    if (g_screen_on && millis() - g_last_active > SCREEN_TIMEOUT_MS) screenOff();

    if (redraw && g_screen_on) render();
}
