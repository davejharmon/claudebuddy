#include "agents.h"
#include <ArduinoJson.h>
#include <time.h>

Agent g_agents[MAX_AGENTS];
int   g_agentCount   = 0;
bool  g_agentWake    = false;
bool  g_agentChime   = false;
bool  g_agentsChanged = false;

// Defined in mqtt_handler.cpp: clears the retained message for an agent we dropped,
// so a crashed session doesn't come back from the broker after a reboot.
void mqttClearAgent(const char* sessionId);
extern uint32_t g_mqtt_connected_ms;   // millis() of the last broker (re)connect

// How long an agent stays on the board without a new event.
static const uint32_t DONE_TTL_S    = 30 * 60;
static const uint32_t ERROR_TTL_S   = 60 * 60;
static const uint32_t WORKING_TTL_S = 6 * 3600;    // a session that crashed mid-turn
static const uint32_t NEEDS_TTL_S   = 12 * 3600;

static bool clockSynced() { return time(nullptr) > 1700000000; }

uint32_t agentAgeS(const Agent& a) {
    if (a.ts && clockSynced()) {
        time_t now = time(nullptr);
        return now > (time_t)a.ts ? (uint32_t)(now - a.ts) : 0;
    }
    return (millis() - a.rxMs) / 1000;
}

static bool parseState(const char* s, AgentState& out) {
    if (!s) return false;
    if (!strcmp(s, "needs_you")) { out = AG_NEEDS_YOU; return true; }
    if (!strcmp(s, "error"))     { out = AG_ERROR;     return true; }
    if (!strcmp(s, "working"))   { out = AG_WORKING;   return true; }
    if (!strcmp(s, "done"))      { out = AG_DONE;      return true; }
    return false;
}

static int findAgent(const char* id) {
    for (int i = 0; i < g_agentCount; i++)
        if (!strcmp(g_agents[i].id, id)) return i;
    return -1;
}

static void removeAt(int i) {
    for (int j = i; j < g_agentCount - 1; j++) g_agents[j] = g_agents[j + 1];
    g_agentCount--;
}

// Insertion sort: state order first, then most recent event first.
static void sortAgents() {
    for (int i = 1; i < g_agentCount; i++) {
        Agent a = g_agents[i];
        int j = i - 1;
        while (j >= 0) {
            const Agent& b = g_agents[j];
            bool before = a.state < b.state ||
                          (a.state == b.state && agentAgeS(a) < agentAgeS(b));
            if (!before) break;
            g_agents[j + 1] = g_agents[j];
            j--;
        }
        g_agents[j + 1] = a;
    }
}

void agentsOnMessage(const char* sessionId, const uint8_t* payload, unsigned len) {
    if (!sessionId || !*sessionId) return;
    int idx = findAgent(sessionId);

    if (len == 0) {                       // session ended (or cleared)
        if (idx >= 0) { removeAt(idx); g_agentsChanged = true; }
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;
    AgentState st;
    if (!parseState(doc["state"] | (const char*)nullptr, st)) return;

    if (idx < 0) {
        if (g_agentCount >= MAX_AGENTS) {
            // Full: evict the oldest DONE agent, or ignore the newcomer.
            int victim = -1;
            for (int i = 0; i < g_agentCount; i++)
                if (g_agents[i].state == AG_DONE &&
                    (victim < 0 || agentAgeS(g_agents[i]) > agentAgeS(g_agents[victim]))) victim = i;
            if (victim < 0) return;
            removeAt(victim);
        }
        idx = g_agentCount++;
        memset(&g_agents[idx], 0, sizeof(Agent));
        strlcpy(g_agents[idx].id, sessionId, sizeof(g_agents[idx].id));
        g_agents[idx].state = AG_DONE;    // so a first "working" counts as a change
    }

    Agent& a = g_agents[idx];
    AgentState prev = a.state;
    strlcpy(a.name, doc["name"] | "agent", sizeof(a.name));
    strlcpy(a.msg,  doc["msg"]  | "",      sizeof(a.msg));
    strlcpy(a.host, doc["host"] | "",      sizeof(a.host));
    a.ts    = doc["ts"] | (uint32_t)0;
    a.rxMs  = millis();
    a.state = st;

    // Only live events should wake or chime: a retained message replayed on
    // reconnect is old news. With a synced clock, its timestamp says how old it
    // is; without one, ignore whatever arrives in the first seconds after connecting.
    bool live = (clockSynced() && a.ts) ? agentAgeS(a) < 120
                                        : millis() - g_mqtt_connected_ms > 5000;
    if (live && st != prev) {
        if (st == AG_NEEDS_YOU)             { g_agentWake = true; g_agentChime = true; }
        else if (st == AG_WORKING)          { g_agentWake = true; }
    }

    Serial.printf("AGENT: %s %s \"%s\" age=%us live=%d n=%d\n", a.id, doc["state"] | "?",
                  a.name, (unsigned)agentAgeS(a), live, g_agentCount);
    sortAgents();                          // after this, `a` may refer to another agent
    g_agentsChanged = true;
}

void agentsAge() {
    bool changed = false;
    for (int i = g_agentCount - 1; i >= 0; i--) {
        const Agent& a = g_agents[i];
        uint32_t ttl = a.state == AG_DONE    ? DONE_TTL_S
                     : a.state == AG_ERROR   ? ERROR_TTL_S
                     : a.state == AG_WORKING ? WORKING_TTL_S : NEEDS_TTL_S;
        if (agentAgeS(a) > ttl) {
            mqttClearAgent(a.id);
            removeAt(i);
            changed = true;
        }
    }
    if (changed) g_agentsChanged = true;
    sortAgents();                          // ages shift once the clock syncs
}

bool agentsAnyActive() {
    for (int i = 0; i < g_agentCount; i++)
        if (g_agents[i].state == AG_WORKING || g_agents[i].state == AG_NEEDS_YOU) return true;
    return false;
}

int agentsCount(AgentState s) {
    int n = 0;
    for (int i = 0; i < g_agentCount; i++) if (g_agents[i].state == s) n++;
    return n;
}
