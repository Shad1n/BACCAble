#ifndef DNA_STAGEG_H
#define DNA_STAGEG_H
#include <stdint.h>
/* C1-only: init on power-on, tick from main loop after DNA state decoding.
 * tick returns 1 for BH restore request, 2 after persisted change, negative on error. */
#if defined(C1baccable)
void dna_stageg_set_enabled(uint8_t enabled);
int dna_stageg_dryrun_target(uint8_t *target);
void dna_stageg_note_384(uint32_t millis);
/* Queue a confirmed ignition-return notification for dna_stageg_tick(). */
void dna_stageg_note_ignition_return(void);
/* Latch a manual BH selector input; the state change is consumed in tick(). */
void dna_stageg_note_manual_selection(void);
void dna_stageg_restore_enqueue_failed(void);
int dna_stageg_init(uint8_t enabled);
int dna_stageg_tick(uint32_t millis, uint16_t rpm, uint8_t observed_mode, uint8_t *restore_target);
#endif
#endif
