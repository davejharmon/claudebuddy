#include "mqtt_handler.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

UI       g_ui;
bool     g_mqtt_connected = false;
bool     g_wake           = false;
uint32_t g_last_msg_ms    = 0;

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static void onMessage(char* topic, byte* payload, unsigned int len) {
    if (strcmp(topic, "clawdmeter/wake") == 0) {
        g_wake = true;
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;

    const char* st = doc["state"] | "offline";
    if      (strcmp(st, "working") == 0) g_ui.state = ST_WORKING;
    else if (strcmp(st, "maxed")   == 0) g_ui.state = ST_MAXED;
    else if (strcmp(st, "idle")    == 0) g_ui.state = ST_IDLE;
    else if (strcmp(st, "stale")   == 0) g_ui.state = ST_STALE;
    else                                  g_ui.state = ST_OFFLINE;

    g_ui.h5 = doc["h5_pct"] | 0;
    g_ui.wk = doc["wk_pct"] | 0;
    g_ui.ts = doc["ts"] | (uint32_t)0;
    strlcpy(g_ui.resetIn, doc["reset_in"] | "", sizeof(g_ui.resetIn));
    strlcpy(g_ui.resetAt, doc["reset_at"] | "", sizeof(g_ui.resetAt));
    if (g_ui.state == ST_STALE)
        strlcpy(g_ui.note, doc["err"] | "daemon failing", sizeof(g_ui.note));
    else
        strlcpy(g_ui.note, doc["note"] | "", sizeof(g_ui.note));
    g_ui.fresh = true;
    g_last_msg_ms = millis();

    Serial.printf("MQTT: %s  h5=%d%%  wk=%d%%\n", st, g_ui.h5, g_ui.wk);
}

static void connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.printf("WiFi: connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\nWiFi: %s\n", WiFi.localIP().toString().c_str());
}

static void connectMqtt() {
    Serial.printf("MQTT: connecting to %s:%d... ", MQTT_HOST, MQTT_PORT);
    if (mqtt.connect("claudebuddy")) {
        mqtt.subscribe(MQTT_TOPIC);
        mqtt.subscribe("clawdmeter/wake");
        Serial.println("ok");
    } else {
        Serial.printf("failed rc=%d\n", mqtt.state());
    }
}

void mqttSetup() {
    WiFi.mode(WIFI_STA);
    connectWifi();
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // UTC; only used to age readings
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(onMessage);
    connectMqtt();
}

void mqttLoop() {
    if (WiFi.status() != WL_CONNECTED) {
        g_mqtt_connected = false;
        connectWifi();
    }
    if (!mqtt.connected()) {
        g_mqtt_connected = false;
        static uint32_t lastTry = 0;
        if (millis() - lastTry > 5000) { lastTry = millis(); connectMqtt(); }
    }
    g_mqtt_connected = mqtt.connected();
    mqtt.loop();
}
