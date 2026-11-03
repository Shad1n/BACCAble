/*
 * C1baccableFunctions.h
 *
 *  Created on: May 2, 2025
 *      Author: GauchoHP
 */

#ifndef INC_FUNCTIONS_C1BACCABLE_H_
	#define INC_FUNCTIONS_C1BACCABLE_H_
/* @Shadin: DNA StageG and Guard interfaces consolidated into the existing C1 functions header. */

#include <stdint.h>
/* @Shadin: ABS protocol constants used by C1 service declarations. */
#include "compile_time_defines.h"
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

/* @Shadin: Interface moved from dna_guard.h; original declarations and comments preserved. */
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



	#include "uart.h" /* @Shadin: ABS protocol declarations now in uart.h. */
	/* @Shadin: C1 ABS service interface consolidated from abs_c1_service.h. */



/* @Shadin: C1 ABS receiver/transport declarations consolidated here after
 * moving implementations into functions_C1baccable.c; original comments retained.
 */
typedef struct { uint8_t id, next, kept, status, reason, codes[ABS_PROTO_MAX][3]; uint16_t total; uint8_t total_set; } AbsReceiver;
void abs_receiver_init(AbsReceiver *r,uint8_t id);
/* 1 = accepted record, 2 = completed, 0 = busy, -1 = invalid, -2 = stale, -3 = terminal error */
int abs_receiver_accept(AbsReceiver *r,const uint8_t frame[ABS_PROTO_FRAME],uint8_t route);
#define ABS_TRANSPORT_POLL_MS 1010u
#define ABS_TRANSPORT_MAX_RETRIES 12u /* Allow C2 session + UDS response-pending window before C1 gives up. */
typedef struct {
 AbsReceiver receiver;
 uint32_t last_poll_ms;
 uint8_t polls_without_progress;
 uint8_t poll_started;
 uint8_t timed_out;
} AbsTransport;
void abs_transport_init(AbsTransport *t, uint8_t transaction_id);
/* 1 = send Get with *index; 0 = not due; -1 = terminal; -2 = timed out. */
int abs_transport_tick(AbsTransport *t, uint32_t now_ms, uint8_t *index);
/* Forward receiver result; accepted progress resets retry budget. */
int abs_transport_reply(AbsTransport *t, const uint8_t frame[ABS_PROTO_FRAME], uint8_t route);
/* Explicit opt-in: this module does not start ABS diagnostics on its own. */
void abs_c1_service_begin(uint8_t transaction_id);
void abs_c1_service_poll(uint32_t now_ms);
const AbsTransport *abs_c1_service_state(void);
void abs_c1_service_tx_started(uint8_t command, uint8_t id_hi, uint8_t id_lo, uint32_t now_ms);
uint8_t abs_c1_service_tx_is_current(uint8_t command, uint8_t id_hi, uint8_t id_lo);

	#include "globalVariables.h"
	#include "functions_Common.h"

	void C1baccableInitCheck(void);

	//elm327 function 26/08/2026 - lifecycle of the ELM327 mode, mirroring the sniffer one (see functions_C1baccable.c)
	void elm327Start(void);
	void elm327Stop(void);
	void elm327UsbStartIfRequested(void);
	void elm327CheckActivationTimeout(void);
	void setSchizzaforteMap(uint8_t map);
	void exhaustValvesToggle(void);
	void exhaustValvesOnDNAmode(uint8_t mode);
	void C1baccablePeriodicCheck(void);
	uint8_t mapCommandNotApplied(void);

	void sendMainDashboardPageToSlaveBaccable(void);
	void sendSetupDashboardPageToSlaveBaccable(void);
	void sendParamsSetupDashboardPageToSlaveBaccable(void);
	void sendDashboardPageToSlaveBaccable(void);
	float getNativeParam(uint8_t paramId);
	//@netzmark - to confirm
	//void buildLineWithFormat(const char* template, float *values, const uint8_t *paramId, char* result);
	void buildLineWithFormat(const char* template, const uint8_t *paramId, char* result);
	uint8_t removePatterns(char *str);
	uint32_t getFreeRAM(void);
	void clearDashboardBaccableMenu(void);
	float scaleVolume(uint8_t vol);
	uint8_t scaleColorSet(uint8_t col);
	uint8_t saveOnflash(void);
	uint8_t resetStatisticsOnFlash();
	uint8_t saveStatisticsOnFlash(void);
	uint8_t saveShownParamsOnflash(void);
	void compress_to_uint16(const uint8_t *input, size_t input_len, uint16_t *output);
	void decompress_from_uint16(const uint16_t *input, size_t input_len, uint8_t *output, size_t output_len);
	void readShownParamsFromFlash(void);
	uint16_t readFromFlash(uint8_t paramId);
	float readStatisticsFromFlash(uint8_t paramId);
	uint8_t getParamIndexFromReqId(uint32_t searchedReqId);
	uint8_t getNextVisibleParam(uint8_t curIndex);
	uint8_t getPreviousVisibleParam(uint8_t curIndex);
	void nativeMaxHoldUpdate(uint8_t paramId);
	/* Gaucho DTC integration entry points; implementations added in a later stage. */
	void faultsStartEcuSession(uint8_t ecu);
	void faultsEcuEnded(uint8_t failed);
	void faultsAbsProcess(void);

/* @Shadin: Interface moved from config_store.h; original declarations and comments preserved. */
#include <stdint.h>
#define CONFIG_STORE_COUNT 34u
uint8_t config_store_save(const uint16_t *values, uint8_t count);
uint16_t config_store_read(uint8_t index);

#endif /* INC_FUNCTIONS_C1BACCABLE_H_ */
