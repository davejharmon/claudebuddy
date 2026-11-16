#!/usr/bin/env python3
"""
Claude Code hook -> claudebuddy agent board.

Register it (async) for UserPromptSubmit, PostToolUse, Notification, Stop,
StopFailure and SessionEnd. Each run reads the hook JSON on stdin and publishes
a retained MQTT message:

  topic   claudebuddy/agents/<session_id>
  payload {"name":"claudebuddy","state":"needs_you","msg":"Claude needs your permission",
           "host":"DAVE-PC","model":"opus 5.5","ts":1763280000}

  state   working | needs_you | done | error ; SessionEnd publishes an empty
          retained payload, which removes the agent.

Standard library only (a minimal MQTT 3.1.1 publish), so it works with any
Python on the PATH. Never fails loudly: Claude Code must not be held up.

Env: CLAUDEBUDDY_MQTT_HOST (default 192.168.1.125), CLAUDEBUDDY_MQTT_PORT (1883).
"""
import json, os, re, socket, sys, tempfile, time
from pathlib import Path

HOST = os.environ.get("CLAUDEBUDDY_MQTT_HOST", "192.168.1.125")
PORT = int(os.environ.get("CLAUDEBUDDY_MQTT_PORT", "1883"))
TOPIC_PREFIX = "claudebuddy/agents/"
CACHE_DIR = Path(tempfile.gettempdir()) / "claudebuddy"
REFRESH_S = 60      # re-publish an unchanged state at most this often (keeps "age" fresh)

NEEDS_YOU_TYPES = {"permission_prompt", "agent_needs_input", "elicitation_dialog",
                   "elicitation_url_dialog"}


# ── Minimal MQTT 3.1.1 publish (QoS 0) ────────────────────────────────────────
def _varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b, n = n % 128, n // 128
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)

def _str(s: str) -> bytes:
    b = s.encode("utf-8")
    return len(b).to_bytes(2, "big") + b

def mqtt_publish(topic: str, payload: bytes, retain: bool = True) -> None:
    client_id = f"cb-hook-{os.getpid()}"
    connect_vh = _str("MQTT") + bytes([4, 0x02]) + (30).to_bytes(2, "big")   # v3.1.1, clean session
    connect = connect_vh + _str(client_id)
    publish = _str(topic) + payload
    with socket.create_connection((HOST, PORT), timeout=2) as s:
        s.sendall(bytes([0x10]) + _varint(len(connect)) + connect)
        ack = s.recv(4)
        if len(ack) < 4 or ack[0] != 0x20 or ack[3] != 0:
            raise ConnectionError(f"MQTT CONNACK refused: {ack!r}")
        s.sendall(bytes([0x30 | (0x01 if retain else 0)]) + _varint(len(publish)) + publish)
        s.sendall(bytes([0xE0, 0x00]))   # DISCONNECT


# ── Per-session cache: name, and last state published (to skip duplicates) ───
def _cache_path(sid: str) -> Path:
    safe = "".join(c for c in sid if c.isalnum() or c in "-_")[:64]
    return CACHE_DIR / f"{safe}.json"

def load_cache(sid: str) -> dict:
    try:
        return json.loads(_cache_path(sid).read_text(encoding="utf-8"))
    except Exception:
        return {}

def save_cache(sid: str, data: dict) -> None:
    try:
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        _cache_path(sid).write_text(json.dumps(data), encoding="utf-8")
    except Exception:
        pass


def session_name(event: dict, cache: dict) -> str:
    for key in ("session_title", "session_name"):
        if event.get(key):
            return str(event[key])[:40]
    if cache.get("name"):
        return cache["name"]
    cwd = event.get("cwd") or ""
    return Path(cwd).name or "claude"


# ── Model: "claude-opus-5-5" -> "opus 5.5" ────────────────────────────────────
FAMILIES = ("opus", "sonnet", "haiku", "fable")
MODEL_RE = re.compile(rb'"model":"(claude-[A-Za-z0-9.\-\[\]]+)"')
TRANSCRIPT_TAIL = 256 * 1024

def short_model(model_id: str) -> str:
    mid = model_id.lower().split("[")[0]
    if mid.startswith("claude-"):
        mid = mid[len("claude-"):]
    parts = [p for p in mid.split("-") if not (p.isdigit() and len(p) == 8)]   # drop dates
    family = next((p for p in parts if p in FAMILIES), None)
    if not family:
        return mid[:15]
    version = ".".join(p for p in parts if p.isdigit())
    return f"{family} {version}".strip()[:15]

def session_model(event: dict, cache: dict) -> str:
    """Model from the event (SessionStart has it), else the newest one in the
    transcript (follows /model switches), else whatever was cached."""
    m = event.get("model")
    if isinstance(m, dict):
        m = m.get("id") or m.get("display_name")
    if isinstance(m, str) and m:
        return short_model(m)
    path = event.get("transcript_path")
    if path:
        try:
            with open(path, "rb") as f:
                f.seek(0, os.SEEK_END)
                f.seek(max(0, f.tell() - TRANSCRIPT_TAIL))
                found = MODEL_RE.findall(f.read())
            if found:
                return short_model(found[-1].decode("ascii", "ignore"))
        except Exception:
            pass
    return cache.get("model", "")


def classify(event: dict):
    """Return (state, msg) for this hook event, or None to publish nothing."""
    name = event.get("hook_event_name", "")
    if name == "UserPromptSubmit":
        return "working", ""
    if name == "PostToolUse":
        return "working", ""          # also clears needs_you once a permission is granted
    if name == "Notification":
        if event.get("notification_type") in NEEDS_YOU_TYPES:
            return "needs_you", (event.get("message") or event.get("title") or "needs you")[:60]
        return None                   # idle_prompt etc.: Stop already said "done"
    if name == "Stop":
        return "done", "your turn"
    if name == "StopFailure":
        return "error", "turn failed"
    return None


def main() -> None:
    try:
        event = json.loads(sys.stdin.read() or "{}")
    except Exception:
        return
    sid = event.get("session_id")
    if not sid:
        return
    topic = TOPIC_PREFIX + sid

    if event.get("hook_event_name") == "SessionEnd":
        mqtt_publish(topic, b"")      # empty retained payload removes the agent
        try:
            _cache_path(sid).unlink()
        except Exception:
            pass
        return

    cache = load_cache(sid)
    name = session_name(event, cache)
    result = classify(event)
    if result is None:
        if name != cache.get("name"):
            cache["name"] = name
            save_cache(sid, cache)
        return
    model = session_model(event, cache)
    state, msg = result

    now = int(time.time())
    if (cache.get("state") == state and cache.get("msg") == msg and cache.get("name") == name
            and cache.get("model", "") == model and now - cache.get("ts", 0) < REFRESH_S):
        return                        # nothing new; PostToolUse fires often

    payload = {"name": name, "state": state, "msg": msg,
               "host": socket.gethostname()[:15], "model": model, "ts": now}
    mqtt_publish(topic, json.dumps(payload, separators=(",", ":")).encode("utf-8"))
    save_cache(sid, {"name": name, "state": state, "msg": msg, "model": model, "ts": now})


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass                          # never block or break Claude Code
    sys.exit(0)
