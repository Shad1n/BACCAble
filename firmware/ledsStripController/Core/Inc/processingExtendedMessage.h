/*
 * processingMessage0x00000226.h
 *
 *  Created on: May 3, 2025
 *      Author: GauchoHP
 */

#ifndef INC_PROCESSINGEXTENDEDMESSAGE_H_
	#define INC_PROCESSINGEXTENDEDMESSAGE_H_

	#include "globalVariables.h"
	#include "functions_C1baccable.h"

	void processingExtendedMessage();

#if defined(C1baccable)
void stageC1cDtcBegin(void);
void stageC1cDtcTimeout(void);
uint32_t stageC1dDtcTimeoutMs(void);
uint8_t stageC1cDtcIsEcmIndex(uint8_t index);
uint16_t stageC1dDtcTotal(void);
void stageC2AbsUpdate(void);
uint8_t stageC2AbsIndex(uint8_t index);
uint16_t stageC2DtcTotal(void);
#endif

/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#ifndef STAGE_C1C_OPTIONAL_ECU_STATUS_UI
#define STAGE_C1C_OPTIONAL_ECU_STATUS_UI 1 /* Set to 0 to restore original UI */
#endif
#if defined(C1baccable) && STAGE_C1C_OPTIONAL_ECU_STATUS_UI
typedef enum { C1C_ECU_NOT_TESTED, C1C_ECU_WAIT, C1C_ECU_OK, C1C_ECU_TIMEOUT, C1C_ECU_ERROR } C1cEcuStatus;
C1cEcuStatus stageC1cStatus(uint8_t ecu);
uint8_t stageC1cStatusDtcCount(uint8_t ecu);
uint8_t stageC1cStatusPage(void); /* 0: DTC/NO FAULTS; 1: BODY; 2: ECM */
void stageC1cPageNext(void);
void stageC1cPagePrev(void);
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */

/* @Shadin: Interface moved from dtc_stream_decoder.h; original declarations and comments preserved. */
#include <stdint.h>
#define DTC_STREAM_MAX 20u
typedef struct {
    uint8_t record[4], fill;
    uint16_t received, total, kept;
    uint8_t codes[DTC_STREAM_MAX][3];
} DtcStreamDecoder;
void dtcStreamReset(DtcStreamDecoder *s);
void dtcStreamConsume(DtcStreamDecoder *s, uint8_t byte);
uint8_t dtcStreamComplete(const DtcStreamDecoder *s, uint16_t expected);
/* @Shadin: ECU result declarations consolidated with the DTC decoder interface. */
typedef struct {
    uint8_t codes[DTC_STREAM_MAX * 2u][3];
    uint8_t count;
    uint8_t body_end;
    uint8_t ecm_end;
    uint16_t total;
    uint16_t ecu_total[2]; /* accepted DTCs per active ECU */
} DtcEcuResults;
void dtcEcuResultsReset(DtcEcuResults *r);
/* Call only after complete, validated ECU response. Returns 0 for invalid ECU/order. */
uint8_t dtcEcuResultsCommit(DtcEcuResults *r, const DtcStreamDecoder *s, uint8_t ecu);


#endif /* INC_PROCESSINGEXTENDEDMESSAGE_H_ */
