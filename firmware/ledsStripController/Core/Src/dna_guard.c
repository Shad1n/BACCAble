#include "dna_guard.h"
#define SETTLE_MS 2000u
#define HOLD_MS 10000u
#define STABLE_MS 750u
#define MANUAL_SETTLE_MS 1000u
#define RUNNING_RPM 400u
static int valid(uint8_t m){return m==0||m==8||m==16||m==48;}
void dna_guard_init(dna_guard *g,uint8_t saved,uint8_t enabled){*g=(dna_guard){0};g->saved=valid(saved)?saved:255;g->enabled=enabled?1:0;g->restore_pending=g->enabled;g->candidate=255;}
/* Apply Setup changes without an unexpected DNA restore while driving.
 * A newly enabled feature learns a stable mode; the next power-on or
 * confirmed ignition return arms one guarded restore. */
void dna_guard_set_enabled(dna_guard *g, uint8_t enabled) {
    enabled = enabled ? 1u : 0u;
    if (g->enabled == enabled) return;
    g->enabled = enabled;
    g->restore_pending = 0u;
    g->command_sent = 0u;
    g->restore_failed = 0u;
    g->candidate = 255u;
    g->running_since = 0u;
    g->manual_hold_until = 0u;
}
void dna_guard_arm_cycle(dna_guard *g) {
    if (!g->enabled || g->restore_pending || g->command_sent) return;
    g->restore_pending = 1u;
    g->restore_failed = 0u;
    g->enqueue_retry_used = 0u;
    g->candidate = 255u;
    g->running_since = 0u;
    g->manual_hold_until = 0u;
}
void dna_guard_manual_selection(dna_guard *g, uint32_t now) {
    if (!g->enabled) return;
    g->restore_pending = 0u;
    g->command_sent = 0u;
    g->restore_failed = 0u;
    g->enqueue_retry_used = 0u;
    g->candidate = 255u;
    g->running_since = 0u;
    g->manual_hold_until = now + MANUAL_SETTLE_MS;
}
int dna_guard_restore(dna_guard *g,uint32_t now,uint16_t rpm,uint8_t actual,uint8_t *target){if(!g->enabled||!g->restore_pending)return 0;if(rpm<=RUNNING_RPM){g->running_since=0;return 0;}if(!g->running_since){g->running_since=now?now:1;return 0;}if((uint32_t)(now-g->running_since)<SETTLE_MS)return 0;g->restore_pending=0;if(!valid(g->saved)||!valid(actual)||g->saved==actual)return 0;g->command_sent=1;g->restore_target=g->saved;g->command_at=now?now:1;*target=g->saved;return 1;}
int dna_guard_observe(dna_guard *g,uint32_t now,uint16_t rpm,uint8_t actual,uint8_t *persist){if(!g->enabled||rpm<=RUNNING_RPM||!valid(actual)||g->restore_pending)return 0;if(g->manual_hold_until && (int32_t)(now-g->manual_hold_until)<0)return 0;g->manual_hold_until=0;if(g->command_sent){if((uint32_t)(now-g->command_at)<HOLD_MS)return 0;g->command_sent=0;g->candidate=255;if(actual!=g->restore_target)g->restore_failed=1;}/* A CAN mode transition alone does not prove driver intent.
 * On failed restoration, inhibit learning for this power cycle.
 * A deliberate Setup OFF -> ON action re-arms learning via set_enabled(). */
if(g->restore_failed)return 0;
if(g->candidate!=actual){g->candidate=actual;g->candidate_since=now;return 0;}if((uint32_t)(now-g->candidate_since)<STABLE_MS||actual==g->saved)return 0;g->saved=actual;*persist=actual;return 1;}

/* Re-arm once if the BH request never entered the UART TX queue.
 * Do not claim a command was sent when the queue was full. */
void dna_guard_restore_enqueue_failed(dna_guard *g) {
    if (g->command_sent) {
        g->command_sent=0u;
        g->running_since=0u;
        g->candidate=255u;
        if (!g->enqueue_retry_used) {
            g->enqueue_retry_used=1u;
            g->restore_pending=1u;
        } else {
            g->restore_pending=0u;
        }
    }
}
