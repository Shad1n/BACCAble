#ifndef DNA_GUARD_H
#define DNA_GUARD_H
#include <stdint.h>
typedef struct {uint8_t saved,enabled,restore_pending,command_sent,restore_target,restore_failed,candidate,enqueue_retry_used;uint32_t running_since,command_at,candidate_since,manual_hold_until;} dna_guard;
void dna_guard_set_enabled(dna_guard *g, uint8_t enabled);
/* Arm one newly confirmed ignition return; repeated notifications are idempotent. */
void dna_guard_arm_cycle(dna_guard *g);
/* A real DNA selector press cancels restore and delays learning until settling. */
void dna_guard_manual_selection(dna_guard *g, uint32_t now);
void dna_guard_restore_enqueue_failed(dna_guard *g);
void dna_guard_init(dna_guard *g,uint8_t saved,uint8_t enabled);
/* returns 1 if restore is due; output target is saved mode. */
int dna_guard_restore(dna_guard *g,uint32_t now,uint16_t rpm,uint8_t actual,uint8_t *target);
/* returns 1 if stable driver-selected mode should be persisted. */
int dna_guard_observe(dna_guard *g,uint32_t now,uint16_t rpm,uint8_t actual,uint8_t *persist);
#endif
