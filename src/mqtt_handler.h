#pragma once
#include "buddy_render.h"

extern UI   g_ui;
extern bool g_mqtt_connected;
extern bool g_wake;

void mqttSetup();
void mqttLoop();
