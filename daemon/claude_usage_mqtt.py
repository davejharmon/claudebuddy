#!/usr/bin/env python3
"""
claudebuddy daemon — polls Anthropic rate-limit headers, emits state-aware JSON to MQTT.

Topic: claudebuddy/state   retain=True
Payload: {"state":"working","h5_pct":58,"wk_pct":47,"reset_in":"1H 29M","reset_at":"3:00p","note":"+13pt this poll"}

States:
  maxed   — h5_pct >= 100 (or wk_pct >= 100)
  working — h5_pct increased since last poll (held for WORKING_HOLD_S after last rise)
  idle    — connected, not spending
  (offline is decided by the device when MQTT drops — never published here)
"""

import os, time, datetime, json, logging, sys
from pathlib import Path
import requests
import paho.mqtt.client as mqtt

# ── Config ────────────────────────────────────────────────────────────────────
CREDENTIALS_PATH = Path.home() / ".claude" / ".credentials.json"
MQTT_HOST         = "192.168.1.123"
MQTT_PORT         = 1883
MQTT_TOPIC        = "claudebuddy/state"
POLL_INTERVAL_S   = 60
WORKING_HOLD_S    = 180   # seconds to stay "working" after last utilisation rise

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s %(levelname)s %(message)s",
                    stream=sys.stdout)
log = logging.getLogger("claudebuddy")

# ── Helpers ───────────────────────────────────────────────────────────────────
def parse_reset(value: str) -> tuple[int, int]:
    """Return (minutes_remaining, reset_unix_ts) from a reset header value.
    The header is a Unix epoch timestamp (seconds)."""
    try:
        ts = float(value)
        now = time.time()
        mins = max(0, int((ts - now) / 60))
        return mins, int(ts)
    except (ValueError, TypeError):
        return 0, 0

def fmt_reset_in(minutes: int) -> str:
    if minutes <= 0:
        return "now"
    h, m = divmod(minutes, 60)
    if h:
        return f"{h}H {m}M"
    return f"{m}M"

def fmt_reset_at(unix_ts: int) -> str:
    if not unix_ts:
        return "?"
    dt = datetime.datetime.fromtimestamp(unix_ts)
    h = dt.hour % 12 or 12
    ampm = "a" if dt.hour < 12 else "p"
    return f"{h}:{dt.minute:02d}{ampm}"

def read_token() -> str:
    """Read OAuth access token from ~/.claude/.credentials.json."""
    raw = CREDENTIALS_PATH.read_text()
    data = json.loads(raw)
    # Support both {"accessToken":...} and {"claudeAiOauth":{"accessToken":...}}
    if "accessToken" in data:
        return data["accessToken"]
    if "claudeAiOauth" in data:
        return data["claudeAiOauth"]["accessToken"]
    raise RuntimeError("accessToken not found in credentials file")

def fetch_headers() -> dict:
    """Make a minimal Anthropic API call to get rate-limit headers."""
    token = read_token()
    resp = requests.post(
        "https://api.anthropic.com/v1/messages",
        headers={
            "Authorization":   f"Bearer {token}",
            "anthropic-version": "2023-06-01",
            "anthropic-beta":  "oauth-2025-04-20",
            "content-type":    "application/json",
        },
        json={
            "model":      "claude-haiku-4-5-20251001",
            "max_tokens": 1,
            "messages":   [{"role": "user", "content": "."}],
        },
        timeout=30,
    )
    return resp.headers

# ── Daemon ────────────────────────────────────────────────────────────────────
def main():
    if not CREDENTIALS_PATH.exists():
        log.error("credentials not found at %s — exiting", CREDENTIALS_PATH)
        sys.exit(1)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, "claudebuddy-daemon")
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    log.info("MQTT connected to %s:%d", MQTT_HOST, MQTT_PORT)

    prev_h5 = None
    last_rise_ts = 0.0

    while True:
        try:
            hdrs = fetch_headers()

            # Parse 5h window
            h5_pct  = round(float(hdrs.get("anthropic-ratelimit-unified-5h-utilization", "0")) * 100)
            wk_pct  = round(float(hdrs.get("anthropic-ratelimit-unified-7d-utilization", "0")) * 100)
            h5_rst  = hdrs.get("anthropic-ratelimit-unified-5h-reset", "0")
            wk_rst  = hdrs.get("anthropic-ratelimit-unified-7d-reset", "0")

            h5_mins, h5_ts = parse_reset(h5_rst)
            _,       _     = parse_reset(wk_rst)   # wk reset not shown in UI currently

            now = time.time()
            note = ""
            state = "idle"

            if h5_pct >= 100 or wk_pct >= 100:
                state = "maxed"
            else:
                if prev_h5 is not None and h5_pct > prev_h5:
                    delta = h5_pct - prev_h5
                    last_rise_ts = now
                    note = f"+{delta}pt this poll"
                if now - last_rise_ts < WORKING_HOLD_S:
                    state = "working"
                    if not note:
                        note = "spending tokens"

            payload = {
                "state":  state,
                "h5_pct": h5_pct,
                "wk_pct": wk_pct,
            }
            if state == "maxed":
                payload["reset_in"] = fmt_reset_in(h5_mins)
                payload["reset_at"] = fmt_reset_at(h5_ts)
            if state == "working" and note:
                payload["note"] = note

            prev_h5 = h5_pct

            msg = json.dumps(payload)
            client.publish(MQTT_TOPIC, msg, retain=True)
            log.info("→ %s", msg)

        except Exception as exc:
            log.warning("poll error: %s", exc)

        time.sleep(POLL_INTERVAL_S)

if __name__ == "__main__":
    main()
