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

import time, datetime, json, logging, sys
from pathlib import Path
import requests
import paho.mqtt.client as mqtt

# ── Config ────────────────────────────────────────────────────────────────────
CREDENTIALS_PATH  = Path.home() / ".claude" / ".credentials.json"
MQTT_HOST         = "192.168.1.125"
MQTT_PORT         = 1883
MQTT_TOPIC        = "claudebuddy/state"
POLL_INTERVAL_S   = 60
WORKING_HOLD_S    = 180   # seconds to stay "working" after last utilisation rise
TOKEN_REFRESH_URL = "https://platform.claude.com/v1/oauth/token"
CLIENT_ID         = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"

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

def read_creds() -> dict:
    """Read credentials dict from ~/.claude/.credentials.json."""
    data = json.loads(CREDENTIALS_PATH.read_text())
    return data.get("claudeAiOauth") or data

def write_creds(creds: dict):
    """Write updated credentials back to disk."""
    data = json.loads(CREDENTIALS_PATH.read_text())
    if "claudeAiOauth" in data:
        data["claudeAiOauth"] = creds
    else:
        data = creds
    CREDENTIALS_PATH.write_text(json.dumps(data, indent=2))

def token_expires_at(creds: dict) -> float:
    """Return expiry as a Unix timestamp in seconds."""
    exp = creds.get("expiresAt", 0)
    return exp / 1000 if exp > 1e10 else float(exp)

def refresh_token(creds: dict) -> dict:
    """Exchange refresh token for a new access token. Returns updated creds dict."""
    log.info("refreshing OAuth token...")
    resp = requests.post(
        TOKEN_REFRESH_URL,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        data={
            "grant_type":    "refresh_token",
            "refresh_token": creds["refreshToken"],
            "client_id":     CLIENT_ID,
        },
        timeout=15,
    )
    resp.raise_for_status()
    body = resp.json()
    if "error" in body:
        raise RuntimeError(f"token refresh failed: {body}")
    # Update in-place, preserving any fields the response doesn't include
    updated = dict(creds)
    updated["accessToken"] = body["access_token"]
    if "refresh_token" in body:
        updated["refreshToken"] = body["refresh_token"]
    if "expires_in" in body:
        updated["expiresAt"] = int((time.time() + body["expires_in"]) * 1000)
    write_creds(updated)
    log.info("token refreshed, expires in %ds", body.get("expires_in", 0))
    return updated

def ensure_fresh_token(creds: dict) -> dict:
    """Refresh if token expires within 5 minutes."""
    if time.time() > token_expires_at(creds) - 300:
        creds = refresh_token(creds)
    return creds

def fetch_headers(creds: dict) -> dict:
    """Make a minimal Anthropic API call to get rate-limit headers."""
    resp = requests.post(
        "https://api.anthropic.com/v1/messages",
        headers={
            "Authorization":     f"Bearer {creds['accessToken']}",
            "anthropic-version": "2023-06-01",
            "anthropic-beta":    "oauth-2025-04-20",
            "content-type":      "application/json",
        },
        json={
            "model":    "claude-haiku-4-5-20251001",
            "max_tokens": 1,
            "messages": [{"role": "user", "content": "."}],
        },
        timeout=30,
    )
    if resp.status_code == 401:
        raise RuntimeError("OAuth token rejected (401) — token may need manual refresh")
    resp.raise_for_status()
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

    creds = read_creds()
    prev_h5 = None
    last_rise_ts = 0.0

    while True:
        try:
            creds = ensure_fresh_token(creds)
            hdrs = fetch_headers(creds)

            h5_pct  = round(float(hdrs.get("anthropic-ratelimit-unified-5h-utilization", "0")) * 100)
            wk_pct  = round(float(hdrs.get("anthropic-ratelimit-unified-7d-utilization", "0")) * 100)
            h5_rst  = hdrs.get("anthropic-ratelimit-unified-5h-reset", "0")
            wk_rst  = hdrs.get("anthropic-ratelimit-unified-7d-reset", "0")

            h5_mins, h5_ts = parse_reset(h5_rst)
            _,       _     = parse_reset(wk_rst)

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
