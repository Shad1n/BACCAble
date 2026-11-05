/*
 * processingMessage0x00000226.c
 *
 *  Created on: May 3, 2025
 *      Author: GauchoHP
 */

#include "processingExtendedMessage.h"
#if defined(C1baccable)
#include "processingExtendedMessage.h"
#include "functions_C1baccable.h"

/* @Shadin: ECM/BCM DTC decoder and results; original implementation preserved. */
/* @Shadin: Moved from dtc_stream_decoder.c into the existing Gaucho extended-CAN handler. */
/* Isolated adaptation of Gaucho's faultsConsumePayloadByte().
 * Stage C1a: not yet wired into the active diagnostic state machine. */
void dtcStreamReset(DtcStreamDecoder *s) { memset(s, 0, sizeof(*s)); }
void dtcStreamConsume(DtcStreamDecoder *s, uint8_t byte) {
    uint16_t pos = s->received++;
    if (pos < 3u) return; /* 59 02 available-status-mask */
    s->record[s->fill++] = byte;
    if (s->fill != 4u) return;
    s->fill = 0;
    if ((s->record[3] & 0x09u) == 0u) return;
    if (s->total != UINT16_MAX) ++s->total;
    if (s->kept < DTC_STREAM_MAX) {
        memcpy(s->codes[s->kept], s->record, 3);
        ++s->kept;
    }
}
uint8_t dtcStreamComplete(const DtcStreamDecoder *s, uint16_t expected) {
    return s->received == expected && s->received >= 3u && s->fill == 0u;
}

/* @Shadin: Moved from dtc_ecu_results.c into the existing Gaucho extended-CAN handler. */
#define DTC_ECU_CAPACITY DTC_STREAM_MAX
#define DTC_ACTIVE_ECUS 2u
void dtcEcuResultsReset(DtcEcuResults *r) { memset(r, 0, sizeof(*r)); }
uint8_t dtcEcuResultsCommit(DtcEcuResults *r, const DtcStreamDecoder *s, uint8_t ecu) {
    if (ecu >= DTC_ACTIVE_ECUS || r->count > DTC_STREAM_MAX * DTC_ACTIVE_ECUS || s->kept > DTC_STREAM_MAX) return 0;
    if (ecu == 0u && (r->body_end != 0u || r->count != 0u)) return 0;
    if (ecu == 1u && r->body_end != r->count) return 0;
    uint8_t copy = (uint8_t)(s->kept < DTC_ECU_CAPACITY ? s->kept : DTC_ECU_CAPACITY);
    for (uint8_t i = 0; i < copy; ++i)
        memcpy(r->codes[r->count + i], s->codes[i], 3);
    r->count = (uint8_t)(r->count + copy);
    r->ecu_total[ecu] = s->total;
    r->total = (uint16_t)(UINT16_MAX - r->total < s->total ? UINT16_MAX : r->total + s->total);
    if (ecu == 0u) r->body_end = r->count;
    r->ecm_end = r->count;
    return 1;
}

static DtcStreamDecoder ecuDtcStream;
static DtcEcuResults ecuDtcResults;
uint16_t stageC1dDtcTotal(void) { return ecuDtcResults.total; }
static uint8_t activeDtcEcu;
/* Stage C1d: extended UDS response-pending wait, DTC reader only. */
static uint8_t dtcResponsePending;
uint32_t stageC1dDtcTimeoutMs(void) {
    return (faultsStateMachine <= 1 && dtcResponsePending) ? 5000u : 2000u;
}

/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
static C1cEcuStatus c1cStatus[3] = { C1C_ECU_NOT_TESTED, C1C_ECU_NOT_TESTED, C1C_ECU_NOT_TESTED };
static uint8_t absResultCommitted;
static uint8_t absTransactionId;
static uint8_t c1cPageCursor;
static uint8_t c1cDtcPageCount(void) { return faultsDTCcount ? faultsDTCcount : 1u; }
static uint8_t c1cPageCount(void) { return (uint8_t)(c1cDtcPageCount() + 3u); }
C1cEcuStatus stageC1cStatus(uint8_t ecu) { return ecu < 3u ? c1cStatus[ecu] : C1C_ECU_NOT_TESTED; }
uint8_t stageC1cStatusDtcCount(uint8_t ecu) {
    if (ecu == 0u) return ecuDtcResults.body_end;
    if (ecu == 1u) return (uint8_t)(ecuDtcResults.count - ecuDtcResults.body_end);
    if (ecu == 2u) return absResultCommitted ? (uint8_t)(faultsDTCcount - ecuDtcResults.count) : 0u;
    return 0u;
}
uint8_t stageC1cStatusPage(void) {
    uint8_t base = c1cDtcPageCount();
    return c1cPageCursor < base ? 0u : (uint8_t)(c1cPageCursor - base + 1u);
}
void stageC1cPageNext(void) {
    c1cPageCursor = (uint8_t)((c1cPageCursor + 1u) % c1cPageCount());
    if (c1cPageCursor < faultsDTCcount) faultsDTCsubmenuIndex = c1cPageCursor;
}
void stageC1cPagePrev(void) {
    c1cPageCursor = c1cPageCursor ? (uint8_t)(c1cPageCursor - 1u) : (uint8_t)(c1cPageCount() - 1u);
    if (c1cPageCursor < faultsDTCcount) faultsDTCsubmenuIndex = c1cPageCursor;
}
static void c1cMarkFailure(C1cEcuStatus reason) { c1cStatus[activeDtcEcu] = reason; }
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */

static void dtcStartEcu(uint8_t ecu) {
    activeDtcEcu = ecu;
    dtcResponsePending = 0;
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
    c1cStatus[ecu] = C1C_ECU_WAIT;
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
    faultsBodyTxHeader.ExtId = ecu ? 0x18DA10F1 : 0x18DA40F1;
    faultsBodyTxHeader.DLC = 3;
    faultsBodyTxData[0] = 0x02;
    faultsBodyTxData[1] = 0x10;
    faultsBodyTxData[2] = 0x03;
    faultsRxExpected = 0;
    faultsRxNextSN = 1;
    faultsStateMachine = 0;
    faultsTimer = currentTime;
    dtcStreamReset(&ecuDtcStream);
    can_tx(&faultsBodyTxHeader, faultsBodyTxData);
}
uint8_t stageC1cDtcIsEcmIndex(uint8_t index) {
    return (uint8_t)(index < ecuDtcResults.count && index >= ecuDtcResults.body_end);
}
void stageC1cDtcBegin(void) {
    dtcEcuResultsReset(&ecuDtcResults);
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
    c1cStatus[0] = c1cStatus[1] = c1cStatus[2] = C1C_ECU_NOT_TESTED;
    absResultCommitted = 0u;
    c1cPageCursor = 0;
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
    faultsDTCcount = 0;
    faultsDTCsubmenuIndex = 0;
    dtcStartEcu(0);
}
/* Stage C2: ABS must start even when ECM returns an error or times out. */
static void stageC2StartAbs(void) {
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
    if (c1cStatus[2] != C1C_ECU_NOT_TESTED) return;
    c1cStatus[2] = C1C_ECU_WAIT;
    absResultCommitted = 0u;
    abs_c1_service_begin(++absTransactionId);
#endif
    faultsStateMachine = 3;
    dtcStreamReset(&ecuDtcStream);
}

void stageC1cDtcTimeout(void) {
    dtcResponsePending = 0;
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
    if (c1cStatus[activeDtcEcu] != C1C_ECU_ERROR) c1cMarkFailure(C1C_ECU_TIMEOUT);
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
    if (activeDtcEcu == 0) {
        /* Body unavailable: independently try ECM, discarding incomplete Body frames. */
        dtcStartEcu(1);
    } else {
        /* Preserve committed BCM records; do not let ECM failure skip ABS. */
        faultsDTCcount = ecuDtcResults.count;
        stageC2StartAbs();
    }
}
static void dtcEcuFinish(void) {
    dtcResponsePending = 0;
    if (!dtcEcuResultsCommit(&ecuDtcResults, &ecuDtcStream, activeDtcEcu)) {
        stageC1cDtcTimeout();
        return;
    }
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
    c1cStatus[activeDtcEcu] = C1C_ECU_OK;
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
    faultsDTCcount = ecuDtcResults.count;
    for (uint8_t i = 0; i < faultsDTCcount; ++i)
        for (uint8_t j = 0; j < 3; ++j)
            faultsDTCbytes[i][j] = ecuDtcResults.codes[i][j];
    faultsDTCsubmenuIndex = 0;
    if (activeDtcEcu == 0) dtcStartEcu(1);
    else stageC2StartAbs();
}

void stageC2AbsUpdate(void) {
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
    if (c1cStatus[2] != C1C_ECU_WAIT || absResultCommitted) return;
    const AbsTransport *t = abs_c1_service_state();
    if (!t) return;
    if (t->timed_out) { c1cStatus[2] = C1C_ECU_TIMEOUT; return; }
    if (t->receiver.status == 'E') {
        c1cStatus[2] = (t->receiver.reason == 'T') ? C1C_ECU_TIMEOUT : C1C_ECU_ERROR;
        return;
    }
    if (!t->receiver.total_set || t->receiver.status != 'R') return;
    uint8_t n = t->receiver.kept;
    if (n > 20u || ecuDtcResults.count + n > FAULTS_DTC_MAX) {
        c1cStatus[2] = C1C_ECU_ERROR; return;
    }
    for (uint8_t i = 0; i < n; ++i)
        for (uint8_t j = 0; j < 3; ++j)
            faultsDTCbytes[ecuDtcResults.count + i][j] = t->receiver.codes[i][j];
    faultsDTCcount = (uint8_t)(ecuDtcResults.count + n);
    absResultCommitted = 1u;
    c1cStatus[2] = C1C_ECU_OK;
#endif
}
uint8_t stageC2AbsIndex(uint8_t index) { return index >= ecuDtcResults.count; }
uint16_t stageC2DtcTotal(void) {
    const AbsTransport *t = abs_c1_service_state();
    return (uint16_t)(ecuDtcResults.total + ((absResultCommitted && t) ? t->receiver.total : 0u));
}
#endif


/* @Shadin: Manual Read/Clear faults journal sender. Uses the existing C1
 * diagnostic snapshots and the standard UART send queue. Stop-and-wait ACKs
 * prevent queue overflow; none of these frames change vehicle CAN traffic. */
#if defined(C1baccable) && STAGE_C1C_OPTIONAL_ECU_STATUS_UI
#define DTC_LOG_EVENT_DEPTH 2u
#define DTC_LOG_EVENT_MAX 60u
/* @Shadin: ECU is derived from per-ECU counts, avoiding a duplicate ECU byte per DTC. */
typedef struct {
    uint8_t kind, id, count[3], status[3], n;
    uint8_t code[DTC_LOG_EVENT_MAX][3]; /* @Shadin: preserve all 24 DTC bits */
} DtcLogEvent;
static DtcLogEvent dtcLogEvents[DTC_LOG_EVENT_DEPTH];
static uint8_t dtcLogHead, dtcLogTail, dtcLogCount, dtcLogId;
static uint8_t dtcLogReadStarted;
static uint8_t dtcLogIndex, dtcLogTxPending, dtcLogAttempts;
static uint32_t dtcLogLastTx;
static volatile uint8_t dtcLogAckPending, dtcLogAckId, dtcLogAckIndex, dtcLogAckStatus;
static volatile uint8_t dtcLogClearReplyPending, dtcLogClearReplyStatus;
static uint8_t dtcLogClearWaiting;
static uint32_t dtcLogClearRequestTime;
static uint8_t dtcLogOverlayState;
static uint32_t dtcLogOverlayTime;

void dtcLogManualReadBegin(void) { dtcLogReadStarted=1u; }
static DtcLogEvent *dtcLogAddEvent(uint8_t kind) {
    if(dtcLogCount>=DTC_LOG_EVENT_DEPTH)return NULL;
    DtcLogEvent *event=&dtcLogEvents[dtcLogTail];
    memset(event,0,sizeof(*event));
    event->kind=kind;
    event->id=++dtcLogId;
    dtcLogTail=(uint8_t)((dtcLogTail+1u)%DTC_LOG_EVENT_DEPTH);
    ++dtcLogCount;
    return event;
}
void dtcLogManualClearBegin(void) {
    /* Requested means only user action occurred; ECU success is unverified. */
    (void)dtcLogAddEvent('C');
}
void dtcLogOnAckIsr(const uint8_t *frame) {
    uint8_t check=0x5Au;
    for(uint8_t i=0u;i<5u;++i)check^=frame[i];
    if(check!=frame[5])return;
    dtcLogAckId=frame[2];dtcLogAckIndex=frame[3];
    dtcLogAckStatus=frame[4];dtcLogAckPending=1u;
}
void dtcLogOnClearReplyIsr(const uint8_t *frame) {
    if((uint8_t)(frame[0]^frame[1]^frame[2]^0x5Au)!=frame[3])return;
    dtcLogClearWaiting=0u;
    dtcLogClearReplyStatus=frame[2];dtcLogClearReplyPending=1u;
}
const char *dtcLogClearOverlay(void) {
    if(!dtcLogOverlayState || (uint32_t)(currentTime-dtcLogOverlayTime)>2000u)
        return NULL;
    return dtcLogOverlayState==1u?"Log cleared":"Clear failed";
}
void dtcLogRequestClear(void) {
    /* @Shadin: Never delete while an earlier manual event is still in transit. */
    if(dtcLogCount || dtcLogReadStarted || dtcLogClearWaiting) {
        dtcLogClearReplyStatus=0u;dtcLogClearReplyPending=1u;return;
    }
    const uint8_t clearLogCmd[4] = {BhBusID, BHcmdDtcLogClear, 0xC3u, 0x3Cu};
    if(tryAddToUARTSendQueue(clearLogCmd, sizeof(clearLogCmd))) {
        dtcLogClearWaiting=1u;
        dtcLogClearRequestTime=currentTime;
    } else {dtcLogClearReplyStatus=0u;dtcLogClearReplyPending=1u;}
}
static void dtcLogRemoveEvent(void) {
    if(!dtcLogCount)return;
    dtcLogHead=(uint8_t)((dtcLogHead+1u)%DTC_LOG_EVENT_DEPTH);
    --dtcLogCount;
    dtcLogIndex=0u;dtcLogTxPending=0u;dtcLogAttempts=0u;
}
static void dtcLogFinishRead(void) {
    if(!dtcLogReadStarted)return;
    stageC2AbsUpdate();
    for(uint8_t i=0u;i<3u;++i)
        if(c1cStatus[i]==C1C_ECU_WAIT || c1cStatus[i]==C1C_ECU_NOT_TESTED)return;
    dtcLogReadStarted=0u;
    DtcLogEvent *e=dtcLogAddEvent('R');
    if(!e)return;
    for(uint8_t i=0u;i<3u;++i) {
        e->status[i]=(uint8_t)c1cStatus[i];
        e->count[i]=stageC1cStatusDtcCount(i);
        if(e->count[i]>20u)e->count[i]=20u;
        /* @Shadin: High status bit marks more DTCs than the 20-code ECU cap. */
        uint16_t found=(i==0u)?ecuDtcResults.ecu_total[0]:
                       (i==1u)?ecuDtcResults.ecu_total[1]:0u;
        if(i==2u) {
            const AbsTransport *t=abs_c1_service_state();
            if(t && absResultCommitted)found=t->receiver.total;
        }
        if(found>e->count[i])e->status[i]|=0x80u;
    }
    uint8_t offset=0u;
    for(uint8_t ecu=0u;ecu<3u;++ecu) {
        for(uint8_t i=0u;i<e->count[ecu] && e->n<DTC_LOG_EVENT_MAX;++i) {
            /* @Shadin: ECU is reconstructed from the stored per-ECU counts on TX. */
            memcpy(e->code[e->n], faultsDTCbytes[offset+i], 3u);
            ++e->n;
        }
        offset=(uint8_t)(offset+e->count[ecu]);
    }
}
void dtcLogService(void) {
    dtcLogFinishRead();
    if(elm327_is_enabled() || lowConsumeIsActive)return;
    if(dtcLogClearWaiting && (uint32_t)(currentTime-dtcLogClearRequestTime)>8000u) {
        dtcLogClearWaiting=0u;
        dtcLogClearReplyStatus=0u;dtcLogClearReplyPending=1u;
    }
    if(dtcLogClearReplyPending) {
        uint8_t ok=dtcLogClearReplyStatus;
        dtcLogClearReplyPending=0u;
        /* @Shadin: 2-second feedback survives the regular menu refresh. */
        dtcLogOverlayState=ok?1u:2u;
        dtcLogOverlayTime=currentTime;
        if(baccableDashboardMenuVisible && dashboard_menu_indent_level==0 && main_dashboardPageIndex==13)
            sendMainDashboardPageToSlaveBaccable();
    }
    if(!dtcLogCount)return;
    DtcLogEvent *e=&dtcLogEvents[dtcLogHead];
    if(dtcLogAckPending) {
        uint32_t mask=__get_PRIMASK();__disable_irq();
        uint8_t ackId=dtcLogAckId,ackIndex=dtcLogAckIndex,ackStatus=dtcLogAckStatus;
        dtcLogAckPending=0u;__set_PRIMASK(mask);
        if(e->id==ackId && dtcLogIndex==ackIndex) {
            if(ackStatus==0u) {
                dtcLogTxPending=0u;dtcLogAttempts=0u;
                if(dtcLogIndex==e->n+1u) {dtcLogRemoveEvent();return;}
                ++dtcLogIndex;
            } else if(ackStatus==2u) {dtcLogRemoveEvent();return;}
            /* Status BUSY is retried after the backoff below. */
        }
    }
    if(dtcLogTxPending && (uint32_t)(currentTime-dtcLogLastTx)<3500u)return;
    if(dtcLogAttempts>=20u) {dtcLogRemoveEvent();return;}
    uint8_t frame[12]={BhBusID,BHcmdDtcLogFrame,e->id,dtcLogIndex};
    if(dtcLogIndex==0u) {
        frame[4]=e->kind;
        for(uint8_t i=0u;i<3u;++i) {
            frame[5u+i]=e->count[i];frame[8u+i]=e->status[i];
        }
    } else if(dtcLogIndex<=e->n) {
        const uint8_t *code=e->code[dtcLogIndex-1u];
        /* @Shadin: The wire protocol remains ECU + three raw DTC bytes. */
        uint8_t record=(uint8_t)(dtcLogIndex-1u);
        frame[4]=(record<e->count[0])?0u:
                 (record<(uint8_t)(e->count[0]+e->count[1]))?1u:2u;
        memcpy(&frame[5],code,3u);
    } else {frame[4]=0xEEu;frame[5]=e->n;}
    /* @Shadin: checksum covers raw DTC bytes (which may contain 0x00). */
    uint8_t check=0x5Au;
    for(uint8_t i=0u;i<11u;++i)check^=frame[i];
    frame[11]=check;
    if(tryAddToUARTSendQueue(frame,sizeof(frame))) {
        dtcLogLastTx=currentTime;
        dtcLogTxPending=1u;
        ++dtcLogAttempts;
    }
}
#else
/* Unsupported configurations have no manual C1 diagnostic journal producer. */
#if defined(C1baccable)
void dtcLogManualReadBegin(void) { }
void dtcLogManualClearBegin(void) { }
const char *dtcLogClearOverlay(void) { return NULL; }
void dtcLogRequestClear(void) { }
void dtcLogOnAckIsr(const uint8_t *frame) { (void)frame; }
void dtcLogOnClearReplyIsr(const uint8_t *frame) { (void)frame; }
void dtcLogService(void) { }
#endif
#endif

#include "security_access_mm10ja.h" //pumpForce test25/07/2026 - algoritmo MM10JA per calcolo key ECM


/* @netzmark: to create multi parameters screens */
#include "uds_parameters.h"
extern uint8_t actuallyRequestedUdsId;
extern uint8_t requestedId;
/* @netzmark: END */

#if defined(C2baccable)
/* Stage C2: stream ABS UDS DTC records, retaining at most 20 codes. */
static void c2AbsDtcConsume(uint8_t b) {
    uint16_t pos = absFaultsRxReceived++;
    if (pos < 3u) return; /* 59 02 available-status-mask */
    absFaultsRecord[absFaultsRecordFill++] = b;
    if (absFaultsRecordFill != 4u) return;
    absFaultsRecordFill = 0u;
    if ((absFaultsRecord[3] & ABS_FAULTS_STATUS_MASK) == 0u) return;
    if (absFaultsDTCtotal != UINT16_MAX) ++absFaultsDTCtotal;
    if (absFaultsDTCcount < ABS_FAULTS_DTC_MAX) {
        memcpy(absFaultsDTCbytes[absFaultsDTCcount], absFaultsRecord, 3u);
        ++absFaultsDTCcount;
    }
}
static void c2AbsDtcFailure(void) { absFaultsState = 4u; }
static void c2AbsDtcRx(void) {
    if (rx_msg_header.ExtId != 0x18DAF128u || absFaultsState >= 3u) return;
    if (DynoStateMachine != 0xffu || DynoModeEnabled || front_brake_forced) return;
    uint8_t dlc = rx_msg_header.DLC;
    if (dlc == 0u || dlc > 8u) return;
    uint8_t pci = rx_msg_data[0];
    uint8_t type = (uint8_t)(pci >> 4);
    if (type == 0u && dlc >= 4u && pci >= 3u &&
        rx_msg_data[1] == 0x7fu &&
        rx_msg_data[2] == (absFaultsState == 0u ? 0x10u : 0x19u)) {
        if (rx_msg_data[3] == 0x78u) {
            absFaultsResponsePending = 1u;
            absFaultsTimer = currentTime;
        } else c2AbsDtcFailure();
        return;
    }
    if (absFaultsState == 0u) {
        if (type == 0u && (pci & 15u) >= 2u && dlc >= 3u &&
            rx_msg_data[1] == 0x50u && rx_msg_data[2] == 0x03u) {
            absFaultsTxHeader.DLC = 4u;
            absFaultsTxData[0] = 0x03u;
            absFaultsTxData[1] = 0x19u;
            absFaultsTxData[2] = 0x02u;
            absFaultsTxData[3] = ABS_FAULTS_STATUS_MASK;
            can_tx(&absFaultsTxHeader, absFaultsTxData);
            absFaultsResponsePending = 0u;
            absFaultsTimer = currentTime;
            absFaultsState = 1u;
        }
        return;
    }
    if (absFaultsState == 1u && type == 0u) {
        uint8_t len = (uint8_t)(pci & 15u);
        if (len < 3u || len > 7u || (uint8_t)(len + 1u) > dlc ||
            rx_msg_data[1] != 0x59u || rx_msg_data[2] != 0x02u ||
            ((len - 3u) % 4u) != 0u) return;
        absFaultsRxReceived = 0u;
        absFaultsRecordFill = 0u;
        for (uint8_t i = 0u; i < len; ++i) c2AbsDtcConsume(rx_msg_data[i + 1u]);
        absFaultsResponsePending = 0u;
        absFaultsState = 3u;
        return;
    }
    if (absFaultsState == 1u && type == 1u) {
        if (dlc < 8u) return;
        uint16_t len = (uint16_t)(((uint16_t)(pci & 15u) << 8) | rx_msg_data[1]);
        if (len <= 7u || len < 3u || ((len - 3u) % 4u) != 0u ||
            rx_msg_data[2] != 0x59u || rx_msg_data[3] != 0x02u) return;
        absFaultsRxExpected = len;
        absFaultsRxReceived = 0u;
        absFaultsRecordFill = 0u;
        absFaultsRxNextSN = 1u;
        for (uint8_t i = 2u; i < 8u; ++i) c2AbsDtcConsume(rx_msg_data[i]);
        absFaultsTxHeader.DLC = 3u;
        absFaultsTxData[0] = 0x30u;
        absFaultsTxData[1] = 0u;
        absFaultsTxData[2] = 0u;
        can_tx(&absFaultsTxHeader, absFaultsTxData);
        absFaultsResponsePending = 0u;
        absFaultsTimer = currentTime;
        absFaultsState = 2u;
        return;
    }
    if (absFaultsState == 2u && type == 2u) {
        if (dlc < 2u || (pci & 15u) != absFaultsRxNextSN) {
            c2AbsDtcFailure();
            return;
        }
        absFaultsRxNextSN = (uint8_t)((absFaultsRxNextSN + 1u) & 15u);
        for (uint8_t i = 1u; i < dlc && absFaultsRxReceived < absFaultsRxExpected; ++i)
            c2AbsDtcConsume(rx_msg_data[i]);
        absFaultsTimer = currentTime;
        if (absFaultsRxReceived == absFaultsRxExpected) {
            absFaultsState = absFaultsRecordFill == 0u ? 3u : 4u;
        }
    }
}
#endif

void processingExtendedMessage(){
	#if defined(C1baccable)
		if(immobilizerEnabled && (engineOnSinceMoreThan5seconds<500)){ //if immo enabled and engine is off
			//if it is a message of connection to RFHUB, reset the connection periodically, but start the panic alarm only once
			if(floodTheBus==0){ //if we are not flooding the bus
				uint8_t responseOffset=rx_msg_data[0]>>4; //0=single frame , 1=first fragmented frame 2=fragmented frame, 3=frame ack
				if((rx_msg_header.ExtId & 0xFFFFFFF0)==0x18DAC7F0){ 		//if it is message from the thief
					if(responseOffset<2){ //we pass this if, in case of single frame and first fragmented frame
						switch(rx_msg_data[responseOffset+1]){
							case 0x10: //diagnostic session
							case 0x27: //security access
							case 0x29: //authentication
							case 0x3E: //tester presence
							//case 0x1A: //??
							case 0x2E: //write data by identifier
							case 0x3D: //write memory by address
								floodTheBus=1; //reset the RFHUB and start the alarm
								break;
							default:
								break;
						}
					}
				}else if((rx_msg_header.ExtId & 0xFFFFF0FF)==0x18DAF0C7) { 	//if it is a reply from rfhub
					//if(floodTheBusStartTime==0){ //this allows to read rfhub messages only if it was the first time
						if(responseOffset<2){ //we pass this if, in case of single frame and first fragmented frame
							switch(rx_msg_data[responseOffset+1]){
								case 0x50: //diagnostic session	//
								case 0x67: //security access
								case 0x69: //authentication
								case 0x7E: //tester presence 	//
								//case 0x1A: //??
								case 0x6E: //write data by identifier
								case 0x7D: //write memory by address
									floodTheBus=1; //reset the RFHUB and start the alarm
									break;
								default:
									break;
							}
						}
					//}
				}
				if(floodTheBus==1){ //if we engaged the immobilizer
					floodTheBusStartTime=currentTime; //set initial time we started to flood the bus
					onboardLed_blue_on(); //light a led
				}

			}
		} //end of immobilizer section

	//ori Gaucho's code
//		if ((rx_msg_header.ExtId==single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyId) && baccableDashboardMenuVisible){ //if we received UDS message with current selected parameter, let's aquire it
//			if(dashboard_menu_indent_level==1 && main_dashboardPageIndex==1){ //if we are in show params menu
//				onboardLed_blue_on();
//				if (rx_msg_header.DLC>=4+single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyOffset+single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyLen){
//					uint8_t numberOfBytesToRead=single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyLen;
//					// Limita il numero di byte a un massimo di 4 per evitare overflow
//					if (numberOfBytesToRead > 4) {
//						numberOfBytesToRead = 4;
//					}
//					uint32_t tmpVal=0; //take value of received parameter
//
//					// Costruisce il valore a partire dai byte ricevuti
//					for (size_t i = 0; i < numberOfBytesToRead; i++) {
//						tmpVal |= ((uint32_t)rx_msg_data[4+single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyOffset+i]) << (8 * (numberOfBytesToRead - 1 - i));
//					}
//
//					tmpVal+=single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyValOffset;
//					float tmpVal2 =tmpVal * single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyScale;
//					tmpVal2 +=single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].replyScaleOffset;
//
//					if(uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]== uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[!currentParamElementSelection]){
//						currentParamElementSelection=0; //single param
//					}
//
//					if(maxHold_enabled){
//						if(isnan(dashboardParamCouple[currentParamElementSelection])){
//							dashboardParamMaxHold[currentParamElementSelection]=tmpVal2; //first update after navigation: reset max hold
//						}else if(tmpVal2 > dashboardParamMaxHold[currentParamElementSelection]){
//							dashboardParamMaxHold[currentParamElementSelection]=tmpVal2; //new maximum found
//						}
//						dashboardParamCouple[currentParamElementSelection]=dashboardParamMaxHold[currentParamElementSelection];
//					}else{
//						dashboardParamCouple[currentParamElementSelection]=tmpVal2; //normal behavior
//					}
//					sendDashboardPageToSlaveBaccable();//send parameters to BH
//				}
//			}
//		}
		
		/* @netzmark: to create multi parameters screens + Ori 2026 MaxHold integration */
// --- 1. CONTEXT CHECK (Directly replacing Gucho's block) ---
if (baccableDashboardMenuVisible && dashboard_menu_indent_level == 1 && main_dashboardPageIndex == 1) {

    // Use actuallyRequestedUdsId as our only anchor.
    // In Group Mode, Rotator sets it and holds it for 500ms (Lag-proof).
    uint8_t currentId = actuallyRequestedUdsId;

    // Safety: only process valid UDS (0- MAX_UDS_PARAMS = 100 today)
    if (currentId < MAX_UDS_PARAMS && single_uds_params_array[currentId].reqId > 0xFF) {

        // --- 2. ADDRESS MATCH ---
        // If the reply address matches the one defined for our active target ID
        if (rx_msg_header.ExtId == single_uds_params_array[currentId].replyId) {

            // --- 3. DATA EXTRACTION ---
            uint8_t offset = single_uds_params_array[currentId].replyOffset;
            uint8_t len    = single_uds_params_array[currentId].replyLen;

            // Boundary Check for DLC
            if (rx_msg_header.DLC >= 4 + offset + len) {
                onboardLed_blue_on();

                uint8_t bytesToRead = (len > 4) ? 4 : len;
                uint32_t tmpVal = 0;

                // Build raw value from received bytes (Gucho's logic)
                for (size_t i = 0; i < bytesToRead; i++) {
                    tmpVal |= ((uint32_t)rx_msg_data[4 + offset + i]) << (8 * (bytesToRead - 1 - i));
                }

                // --- 4. CALCULATION & STORAGE ---
                tmpVal += single_uds_params_array[currentId].replyValOffset;
                float finalVal = (float)tmpVal * single_uds_params_array[currentId].replyScale;
                finalVal += single_uds_params_array[currentId].replyScaleOffset;

                // --- 5. INTEGRATED MAX HOLD LOGIC (Netzmark Dynamic Core Adaptation) ---
                if (maxHold_enabled) {
                    // Check if cache is empty or if we found a new maximum for this specific UDS ID
                    if (isnan(uds_values_cache[currentId]) || finalVal > uds_values_cache[currentId]) {
                        uds_values_cache[currentId] = finalVal; // New maximum found, update cache directly
                    }
                } else {
                    // Normal behavior: Update the "Fridge" (Cache) with current live value
                    uds_values_cache[currentId] = finalVal;
                }
            }
        }
    }
}
/* @netzmark: END */
		
		/*
		if(seatbeltAlarmDisabled==0xfe){ //if seatbelt status acquisition is in progress
			if(rx_msg_header.ExtId==0x18DAF160){ //if received message comes from IPC
				if (rx_msg_header.DLC>=5){ //if at least 5 bytes
					if(rx_msg_data[1]==0x62){ //if param read reply successful
						if((rx_msg_data[2]==0x55) && (rx_msg_data[3]==0xA0 )){ //if param. 55A0 (seat belt alarm status)
							seatbeltAlarmDisabled= !(rx_msg_data[4]); //coherce to boolean and negate
						}
					}
				}
			}
		}
		*/

		if((seatbeltAlarmDisabled==0x11) || (seatbeltAlarmDisabled==0x21)){ //if write param was sent (seatbelt disabling or enabling in progress)
			if(rx_msg_header.ExtId==0x18DAF160){ //if received message comes from IPC
				if (rx_msg_header.DLC>=4){ //if at least 4 bytes
					if(rx_msg_data[1]==0x6F){ //if write param reply successful
						if((rx_msg_data[2]==0x55) && (rx_msg_data[3]==0xA0)){ //if param wrote was 55A0 (en/dis seatbelt alam)
							if(seatbeltAlarmDisabled==0x11){ // if seatbelt disabling
								seatbeltAlarmDisabled=1; //seatbelt disabled
							}
							if(seatbeltAlarmDisabled==0x21){ // if seatbelt enabling
								seatbeltAlarmDisabled=0; //seatbelt enabled
							}
						}
					}
				}
			}
		}


		if((seatbeltAlarmDisabled==0x10) || (seatbeltAlarmDisabled==0x20)){ //if diag session was sent (seatbelt disabling or enabling is in progress)
			if(rx_msg_header.ExtId==0x18DAF160){ //if received message comes from IPC
				if (rx_msg_header.DLC>=2){ //if at least 2 bytes
					if(rx_msg_data[1]==0x50){ //if diag session reply successful
						// Send enable/disable seatbelt alarm message
						uds_parameter_request_msg_header.ExtId=0x18DA60F1;
						uds_parameter_request_msg_header.DLC=6;
						uds_parameter_request_msg_data[0]=0x05;
						uds_parameter_request_msg_data[1]=0x2F;
						uds_parameter_request_msg_data[2]=0x55;
						uds_parameter_request_msg_data[3]=0xA0;
						uds_parameter_request_msg_data[4]=0x03;

						if(seatbeltAlarmDisabled==0x10){ // if seatbelt disabling
							uds_parameter_request_msg_data[5]=0x00; //set byte to disable alarm
						}
						if(seatbeltAlarmDisabled==0x20){ // if seatbelt enabling
							uds_parameter_request_msg_data[5]=0x01; //send msg to enable alarm
						}
						// send message
						can_tx(&uds_parameter_request_msg_header, uds_parameter_request_msg_data); //transmit the diag session request

						seatbeltAlarmDisabled++; //record that operation was executed
						seatbeltAlarmStatusRequestTime=currentTime;
						last_sent_uds_parameter_request_Time=currentTime;
					}
				}
			}
		}



		if(function_route_msg_enabled==1){
			if (rx_msg_header.ExtId==0x18DABAF1){ //if route request and dashboard menu not shown to avoid conflicts
				if (rx_msg_header.DLC>=7){
					routeStdIdMsg=!(rx_msg_data[2]>>4); //standard or extended msgID route request
					routeOffset=(rx_msg_data[2] & 0x0F); //offset from which start to copy
					routeMsgData[2]=rx_msg_data[2]; //copy in the response

					routeMsgId=	((uint32_t)rx_msg_data[3] << 24) |  // MSB
								((uint32_t)rx_msg_data[4] << 16) |
								((uint32_t)rx_msg_data[5] << 8)  |
								((uint32_t)rx_msg_data[6]);       	 // LSB



					onboardLed_blue_on();
				}
			}

			if(baccableDashboardMenuVisible) routeStdIdMsg=0xff; //disables the route request, to avoid conflicts with show params functionality

			if(routeStdIdMsg==0){ //if we have to do it (ext id route request)
				if(rx_msg_header.ExtId==routeMsgId){ //received msg to route
					routeStdIdMsg=0xFF; //set this to disable the request. only one message is routed to avoid bus flood
					if(routeOffset<rx_msg_header.DLC){ //send only if offset is correct
						uint8_t sizeToCopy=5; //
						if((rx_msg_header.DLC - routeOffset )<sizeToCopy) sizeToCopy=rx_msg_header.DLC - routeOffset;
						memcpy(&routeMsgData[3],&rx_msg_data[routeOffset],sizeToCopy);
						if(sizeToCopy<5) memset(&routeMsgData[3+sizeToCopy],0x00, 5-sizeToCopy);

						//send it
						can_tx(&routeMsgHeader, routeMsgData);
						onboardLed_blue_on();
					}

				}
			}
		}

		//pumpForce test25/07/2026 - BEGIN
		// -----------------------------------------------------------------------
		// PUMP FORCE — Gestione risposte ECM (ECU 0x10, ExtId risposta 0x18DAF110)
		//
		// Sequenza UDS attesa:
		//   Stato 0: ricezione 50 03  → invia 27 01 (request seed)        → stato 1
		//   Stato 1: ricezione 67 01  → calcola key MM10JA, invia 27 02   → stato 2
		//   Stato 2: ricezione 67 02  → prepara frame IO Control           → stato 3
		//   Stato 3: invio periodico 2F 50 11 03 FF gestito in C1baccablePeriodicCheck()
		//   Qualsiasi 7F: abort incondizionato (no retry)
		//
		// Formato ISO-TP single-frame (tutti i messaggi di questa sequenza):
		//   byte[0] = PCI  (nibble alto=0 → single frame; nibble basso = n byte dati)
		//   byte[1] = SID risposta (SID richiesta + 0x40)
		//   byte[2] = subfunction o DID high
		//   byte[3..] = payload (seed, key, DID low, controlOption, controlValue)
		// -----------------------------------------------------------------------
		if (rx_msg_header.ExtId == 0x18DAF110 && pumpForceStateMachine != 0xFF) {

			// Risposta negativa (0x7F xx xx): la ECM ha rifiutato l'ultima richiesta
			if (rx_msg_header.DLC >= 3 && rx_msg_data[1] == 0x7F) {
				pumpForceStateMachine = 0xFF; // abort definitivo, nessun retry automatico

			// Stato 0 → 1: conferma sessione estesa ricevuta ([06, 50, 03, 00, 32, 01, F4])
			} else if (pumpForceStateMachine == 0 &&
					   rx_msg_header.DLC >= 3 &&
					   rx_msg_data[1] == 0x50 && rx_msg_data[2] == 0x03) {
				// Sessione aperta: richiedi seed per security access livello 1
				pumpForceTxHeader.DLC = 3;
				pumpForceTxData[0] = 0x02; // PCI: single frame, 2 byte dati
				pumpForceTxData[1] = 0x27; // SID: SecurityAccess
				pumpForceTxData[2] = 0x01; // subfunction: requestSeed livello 1
				can_tx(&pumpForceTxHeader, pumpForceTxData);
				lastPumpForceMsgTime = currentTime; // aggiorna timestamp per calcolo timeout
				pumpForceStateMachine = 1;

			// Stato 1 → 2: seed ricevuto ([06, 67, 01, s0, s1, s2, s3], DLC=7)
			// Il seed è a 4 byte big-endian a partire da byte[3]
			} else if (pumpForceStateMachine == 1 &&
					   rx_msg_header.DLC >= 7 &&
					   rx_msg_data[1] == 0x67 && rx_msg_data[2] == 0x01) {
				ecmSeed[0] = rx_msg_data[3];
				ecmSeed[1] = rx_msg_data[4];
				ecmSeed[2] = rx_msg_data[5];
				ecmSeed[3] = rx_msg_data[6];
				// Calcola la key con algoritmo S1_FGA_ORIGINAL_ECU_Sup0002 (costante interna 0x17591215)
				uint8_t ecmKey[4];
				mm10ja_compute_key(ecmSeed, ecmKey);
				// Invia key: 27 02 + 4 byte key (6 byte dati totali → DLC=7)
				pumpForceTxHeader.DLC = 7;
				pumpForceTxData[0] = 0x06; // PCI: single frame, 6 byte dati
				pumpForceTxData[1] = 0x27; // SID: SecurityAccess
				pumpForceTxData[2] = 0x02; // subfunction: sendKey livello 1
				pumpForceTxData[3] = ecmKey[0];
				pumpForceTxData[4] = ecmKey[1];
				pumpForceTxData[5] = ecmKey[2];
				pumpForceTxData[6] = ecmKey[3];
				can_tx(&pumpForceTxHeader, pumpForceTxData);
				lastPumpForceMsgTime = currentTime;
				pumpForceStateMachine = 2;

			// Stato 2 → 3: accesso sicurezza concesso ([02, 67, 02])
			} else if (pumpForceStateMachine == 2 &&
					   rx_msg_header.DLC >= 3 &&
					   rx_msg_data[1] == 0x67 && rx_msg_data[2] == 0x02) {
				// Prepara il frame IO Control che verrà inviato periodicamente in C1baccablePeriodicCheck()
				// InputOutputControlByIdentifier DID=0x5011, shortTermAdjustment (0x03), val=0xFF (max flow)
				pumpForceTxHeader.DLC = 6;
				pumpForceTxData[0] = 0x05; // PCI: single frame, 5 byte dati
				pumpForceTxData[1] = 0x2F; // SID: InputOutputControlByIdentifier
				pumpForceTxData[2] = 0x50; // DID high byte
				pumpForceTxData[3] = 0x11; // DID low byte
				pumpForceTxData[4] = 0x03; // controlOption: shortTermAdjustment
				pumpForceTxData[5] = 0xFF; // controlValue: massima portata (0xFF)
				lastPumpForceMsgTime = 0;   // azzera il timer → il primo invio avviene al loop successivo
				pumpForceStateMachine = 3;
			}
		}
		//pumpForce test25/07/2026 - END

        // Stage C1b: Gaucho-derived streaming Body ECU DTC reader.
        // The established Netzmark UDS parameter-processing path is unchanged.
        if (rx_msg_header.ExtId == (activeDtcEcu ? 0x18DAF110 : 0x18DAF140) && faultsStateMachine < 3) {
            const uint8_t pci = rx_msg_data[0];
            const uint8_t type = pci >> 4;
            if (rx_msg_header.DLC >= 4 && type == 0 &&
                pci == 0x03 && rx_msg_data[1] == 0x7F &&
                ((faultsStateMachine == 0 && rx_msg_data[2] == 0x10) ||
                 (faultsStateMachine == 1 && rx_msg_data[2] == 0x19))) {
                if (rx_msg_data[3] == 0x78) {
                    dtcResponsePending = 1;
                    faultsTimer = currentTime;
                } else {
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
                    c1cMarkFailure(C1C_ECU_ERROR);
#endif
                    stageC1cDtcTimeout();
                }
            } else if (faultsStateMachine == 0) {
                if (rx_msg_header.DLC >= 3 && type == 0 &&
                    rx_msg_data[1] == 0x50 && rx_msg_data[2] == 0x03) {
                    faultsBodyTxHeader.DLC = 4;
                    faultsBodyTxData[0] = 0x03;
                    faultsBodyTxData[1] = 0x19;
                    faultsBodyTxData[2] = 0x02;
                    faultsBodyTxData[3] = 0x09;
                    can_tx(&faultsBodyTxHeader, faultsBodyTxData);
                    dtcStreamReset(&ecuDtcStream);
                    faultsTimer = currentTime;
                    dtcResponsePending = 0;
                    faultsStateMachine = 1;
                }
            } else if (faultsStateMachine == 1 && type == 0) {
                const uint8_t length = pci & 0x0F;
                if (length >= 3 && length <= 7 && rx_msg_header.DLC >= length + 1 &&
                    rx_msg_data[1] == 0x59 && rx_msg_data[2] == 0x02 &&
                    (length - 3) % 4 == 0) {
                    dtcResponsePending = 0;
                    dtcStreamReset(&ecuDtcStream);
                    for (uint8_t i = 0; i < length; ++i)
                        dtcStreamConsume(&ecuDtcStream, rx_msg_data[i + 1]);
                    if (dtcStreamComplete(&ecuDtcStream, length)) dtcEcuFinish();
                }
            } else if (faultsStateMachine == 1 && type == 1 && rx_msg_header.DLC >= 8) {
                const uint16_t length = ((uint16_t)(pci & 0x0F) << 8) | rx_msg_data[1];
                if (length >= 7 && (length - 3) % 4 == 0 &&
                    rx_msg_data[2] == 0x59 && rx_msg_data[3] == 0x02) {
                    dtcResponsePending = 0;
                    dtcStreamReset(&ecuDtcStream);
                    faultsRxExpected = length;
                    faultsRxNextSN = 1;
                    for (uint8_t i = 2; i < 8 && ecuDtcStream.received < length; ++i)
                        dtcStreamConsume(&ecuDtcStream, rx_msg_data[i]);
                    faultsBodyTxHeader.DLC = 3;
                    faultsBodyTxData[0] = 0x30;
                    faultsBodyTxData[1] = 0;
                    faultsBodyTxData[2] = 0;
                    can_tx(&faultsBodyTxHeader, faultsBodyTxData);
                    faultsTimer = currentTime;
                    faultsStateMachine = 2;
                }
            } else if (faultsStateMachine == 2 && type == 2) {
                if ((pci & 0x0F) == faultsRxNextSN) {
                    faultsRxNextSN = (faultsRxNextSN + 1) & 0x0F;
                    for (uint8_t i = 1; i < rx_msg_header.DLC &&
                         ecuDtcStream.received < faultsRxExpected; ++i)
                        dtcStreamConsume(&ecuDtcStream, rx_msg_data[i]);
                    faultsTimer = currentTime;
                    if (ecuDtcStream.received == faultsRxExpected &&
                        dtcStreamComplete(&ecuDtcStream, faultsRxExpected))
                        dtcEcuFinish();
                } else {
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
                    c1cMarkFailure(C1C_ECU_ERROR);
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
                    stageC1cDtcTimeout();
                    dtcStreamReset(&ecuDtcStream);
                }
            }
        }


	#endif //end define

	#if defined(C2baccable)
        c2AbsDtcRx();
		if (rx_msg_header.ExtId==0x18DAF128 && DynoStateMachine!=0xff ){ //if message from ABS ECU and Dyno state machine is in progress
			if (DynoStateMachine==0 && rx_msg_header.DLC>=3){ //we received a reply to diagnostic session request msg
				if(rx_msg_data[0]==0x06 && rx_msg_data[1]==0x50 && rx_msg_data[2]==0x03){ //if request was successful
					DynoStateMachine++; //send dyno sts msg
				}
			}
			if (DynoStateMachine==1 && rx_msg_header.DLC>=5){ //we received a reply to dyno status msg
				if(rx_msg_data[0]==0x05 && rx_msg_data[1]==0x62 && rx_msg_data[2]==0x30 && rx_msg_data[3]==0x02){ //if request was successful
					DynoStateMachine++; //send dyno disable
					if(rx_msg_data[4]==0x00){ //if it is disabled, we shall enable it
						DynoModeEnabled=0;//refresh current status
						DynoStateMachine++;//send dyno enable
					}else{ //it is enabled, we shall disable it
						DynoModeEnabled=1;//refresh current status
					}
				}
			}
			if (DynoStateMachine==2 && rx_msg_header.DLC>=4){ //we received a reply to dyno disable msg
				if(rx_msg_data[0]==0x03 && rx_msg_data[1]==0x6E && rx_msg_data[2]==0x30 && rx_msg_data[3]==0x02){ //if request was successful
					DynoModeEnabled=0;//success change complete
					DynoStateMachine=0xff; //disable state machine

					//send message to master to inform about the status of Dyno
					uint8_t tmpArr2[2]={C1BusID,C1cmdDynoNotActive};
					if(DynoModeEnabled) tmpArr2[1]=C1cmdDynoActive;
					addToUARTSendQueue(tmpArr2, 2);

					onboardLed_blue_on();
				}
			}
			if (DynoStateMachine==3 && rx_msg_header.DLC>=4){ //we received a reply to dyno enable msg
				if(rx_msg_data[0]==0x03 && rx_msg_data[1]==0x6E && rx_msg_data[2]==0x30 && rx_msg_data[3]==0x02){ //if request was successful
					DynoModeEnabled=1;//success change complete

					DynoStateMachine=0xff; //disable state machine

					//send message to master to inform about the status of Dyno
					uint8_t tmpArr2[2]={C1BusID,C1cmdDynoNotActive};
					if(DynoModeEnabled) tmpArr2[1]=C1cmdDynoActive;
					addToUARTSendQueue(tmpArr2, 2);

					onboardLed_blue_on();
				}
			}

			if (DynoStateMachine!=0xff && rx_msg_header.DLC>=3){ //in any case
				if( rx_msg_data[1]==0x7F ){ //if request refused, abort all
					DynoStateMachine=0xff; //disable state machine

					//send message to master to inform about the status of Dyno
					uint8_t tmpArr2[2]={C1BusID,C1cmdDynoNotActive};
					if(DynoModeEnabled) tmpArr2[1]=C1cmdDynoActive;
					addToUARTSendQueue(tmpArr2, 2);

					onboardLed_blue_on();

				}
			}
			if(DynoStateMachine!=0xff){ //if we are running, send next message
				DYNO_msg_header.DLC=DYNO_msg_data[DynoStateMachine][0]+1;
				can_tx(&DYNO_msg_header, DYNO_msg_data[DynoStateMachine]); //add to the transmission queue
				onboardLed_blue_on();
				DynoStateMachineLastUpdateTime=currentTime;//save last time it was updated
			}
		}
	#endif  //end define C2


}
