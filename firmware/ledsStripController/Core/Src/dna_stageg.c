/* Stage G v3.9.0 C1 adapter. dna_stageg_tick() is the sole restore action path. */
#include "compile_time_defines.h"
#if defined(C1baccable)
#include "dna_stageg.h"
#include "dna_journal_stm32.h"
#include "dna_guard.h"
#include "stageg_ign_diag.h"
#include <stdint.h>
static dna_guard guard;
static dna_journal_state journal;
static uint8_t initialized;
static uint8_t dryrun_enabled;
static uint8_t seen_384;
static uint32_t last_384_ms;
static uint8_t confirmed_return_pending;
static volatile uint8_t manual_selection_pending;
void dna_stageg_note_384(uint32_t millis) { seen_384=1u; last_384_ms=millis; }
void dna_stageg_note_ignition_return(void) { if (initialized) confirmed_return_pending=1u; }
void dna_stageg_note_manual_selection(void) { if (initialized) manual_selection_pending=1u; }
void dna_stageg_restore_enqueue_failed(void) {
    if (initialized) dna_guard_restore_enqueue_failed(&guard);
}
int dna_stageg_init(uint8_t enabled) {
    if (dna_journal_load(&dna_stm32_flash_ops, &journal)) return -1;
    dna_guard_init(&guard, journal.valid ? journal.mode : 0xFFu, enabled);
    seen_384 = 0u;
    confirmed_return_pending = 0u;
    manual_selection_pending = 0u;
    dryrun_enabled = enabled ? 1u : 0u;
    initialized = 1u;
    return 0;
}
void dna_stageg_set_enabled(uint8_t enabled) {
    dryrun_enabled = enabled ? 1u : 0u;
    if (initialized) dna_guard_set_enabled(&guard, enabled);
}
/* Read-only eligibility check. No guard mutation, flash writes, or CAN TX. */
int dna_stageg_dryrun_target(uint8_t *target) {
    if (!initialized || !dryrun_enabled || !journal.valid || !target) return 0;
    if (journal.mode != 0x00u && journal.mode != 0x08u &&
        journal.mode != 0x10u && journal.mode != 0x30u) return 0;
    *target = journal.mode;
    return 1;
}
int dna_stageg_tick(uint32_t millis, uint16_t rpm, uint8_t observed_mode, uint8_t *restore_target) {
    uint8_t save_mode = 0xFFu;
    if (!initialized || !restore_target) return -1;
    if (confirmed_return_pending) {
        confirmed_return_pending = 0u;
        dna_guard_arm_cycle(&guard);
    }
    if (manual_selection_pending) {
        manual_selection_pending = 0u;
        dna_guard_manual_selection(&guard, millis);
        stageg_ign_diag_note_manual_selection();
    }
    /* Never restore or learn from an uninitialized/stale CAN mode. */
    if (!seen_384 || (uint32_t)(millis-last_384_ms)>500u) return 0;
    if (dna_guard_restore(&guard, millis, rpm, observed_mode, restore_target)) {
        stageg_ign_diag_note_guard_request(*restore_target);
        return 1;
    }
    if (dna_guard_observe(&guard, millis, rpm, observed_mode, &save_mode)) {
        if (dna_journal_save(&dna_stm32_flash_ops, &journal, save_mode) < 0) {
            /* Guard updated its RAM copy before flash commit. Restore the
             * last committed value so the same stable mode can be retried. */
            guard.saved = journal.valid ? journal.mode : 0xFFu;
            return -2;
        }
        return 2;
    }
    return 0;
}
#endif
