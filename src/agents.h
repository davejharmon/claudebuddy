#pragma once
// Agent board: one entry per Claude Code session, fed by retained MQTT
// messages on claudebuddy/agents/<session_id> (see hooks/agent_event.py).
//
// Payload: {"name":"claudebuddy","state":"needs_you","msg":"Claude needs your permission",
//           "host":"DAVE-PC","ts":1763280000}
// state: working | needs_you | done | error.  An empty payload removes the agent.
#include <Arduino.h>

enum AgentState : uint8_t { AG_NEEDS_YOU, AG_ERROR, AG_WORKING, AG_DONE };  // sort order

struct Agent {
    char       id[40];
    char       name[24];
    char       msg[40];
    char       host[16];
    AgentState state;
    uint32_t   ts;        // daemon/hook Unix time of the last event, 0 = unknown
    uint32_t   rxMs;      // millis() when the last event arrived here
};

#define MAX_AGENTS 16

extern Agent g_agents[MAX_AGENTS];
extern int   g_agentCount;          // sorted: needs_you, error, working, done; newest first

// Set by agentsOnMessage when something worth waking/chiming for happened.
extern bool g_agentWake;            // an agent started working or needs you
extern bool g_agentChime;           // an agent newly needs you
extern bool g_agentsChanged;        // list contents changed, redraw

void     agentsOnMessage(const char* sessionId, const uint8_t* payload, unsigned len);
void     agentsAge();               // drop old DONE / silent agents; call ~once a second
uint32_t agentAgeS(const Agent& a); // seconds since the agent's last event
bool     agentsAnyActive();         // any working or needs_you
int      agentsCount(AgentState s);
