#!/usr/bin/env python3
"""
Fired by the Claude Code UserPromptSubmit hook.
Publishes to clawdmeter/wake so the M5Stack screen turns on immediately.
Uses paho.mqtt.publish.single() — one-shot connect/publish/disconnect.
Fast on a local network (<100ms); silently exits on any error so Claude
Code is never blocked.
"""
import sys
try:
    import paho.mqtt.publish as publish
    publish.single(
        topic="clawdmeter/wake",
        payload="1",
        hostname="192.168.1.125",
        port=1883,
        keepalive=5,
    )
except Exception:
    pass  # never block Claude Code
sys.exit(0)
