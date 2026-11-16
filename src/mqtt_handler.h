#pragma once
#include "buddy_render.h"

extern UI   g_ui;
extern bool g_mqtt_connected;
extern bool g_wake;
extern uint32_t g_last_msg_ms;   // millis() when the last state payload arrived

void mqttSetup();
void mqttLoop();
