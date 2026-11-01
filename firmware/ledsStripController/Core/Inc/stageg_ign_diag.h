#ifndef STAGEG_IGN_DIAG_H
#define STAGEG_IGN_DIAG_H
#include <stdint.h>
#include "compile_time_defines.h" /* defines C1baccable before the C1-only API guard */
#if defined(C1baccable)
/* 8-event ring. Events 1-12 are observations; DNA request counters below report guarded actions. */
typedef struct { uint32_t ms; uint16_t rpm; uint8_t ignition_raw; uint8_t power_raw; uint8_t dna_mode; uint8_t rfhub_raw; uint8_t stopstart_raw; uint8_t event; } stageg_ign_event_t;
extern volatile stageg_ign_event_t stageg_ign_events[8];
extern volatile uint32_t stageg_ign_event_count;
/* Saturating per-ID counters, independent of the 8-event ring. */
extern volatile uint16_t stageg_ign_timeout_count[3], stageg_ign_recovery_count[3];
extern volatile uint16_t stageg_ign_rfhub_short_count, stageg_ign_rfhub_long_count;
extern volatile uint16_t stageg_ign_rfhub_last_duration_ms, stageg_ign_384_5to4_count;
extern volatile uint16_t stageg_ign_rfhub_last_short_ms, stageg_ign_rfhub_last_long_ms;
extern volatile uint16_t stageg_ign_confirmed_cycle_count;
extern volatile uint16_t stageg_dryrun_ready_count, stageg_dryrun_skip_count;
extern volatile uint8_t stageg_dryrun_last_target;
extern volatile uint16_t stageg_armed_count, stageg_armed_already_count;
extern volatile uint16_t stageg_guard_request_count, stageg_guard_manual_count;
extern volatile uint8_t stageg_guard_request_target;
extern volatile uint8_t stageg_armed_last_target;
extern volatile uint32_t stageg_ign_last_384_ms, stageg_ign_last_1ef_ms, stageg_ign_last_0fc_ms;
extern volatile uint8_t stageg_ign_seen_384, stageg_ign_seen_1ef, stageg_ign_seen_0fc;
extern volatile uint8_t stageg_ign_live_ign, stageg_ign_live_power, stageg_ign_live_dna, stageg_ign_live_rfhub, stageg_ign_live_ss;
void stageg_ign_diag_note_0fc(void);
void stageg_ign_diag_check_timeouts(uint32_t now);
void stageg_ign_diag_note_rfhub(uint8_t raw);
void stageg_ign_diag_note_stopstart(uint8_t raw);
void stageg_ign_diag_note_guard_request(uint8_t target);
void stageg_ign_diag_note_manual_selection(void);
#endif
#endif
