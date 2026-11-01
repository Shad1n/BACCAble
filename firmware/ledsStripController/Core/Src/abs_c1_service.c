#include "abs_c1_service.h"
#include "abs_c1_inbox.h"
#include "uart.h"
#include <string.h>
#if defined(C1baccable)
static AbsTransport service;
static uint8_t enabled;
static uint8_t start_pending;
static uint8_t start_in_flight;
static uint8_t get_in_flight;
static uint32_t queued_at_ms;
static uint8_t queue_wait_armed;
#define ABS_UART_QUEUE_WAIT_MAX_MS 5000u
static const char hex[]="0123456789ABCDEF";
static void encode_byte(uint8_t *p, uint8_t v) { p[0]=hex[v>>4]; p[1]=hex[v&15u]; }
void abs_c1_service_begin(uint8_t id) {
    abs_c1_inbox_reset();
    abs_transport_init(&service,id);
    enabled=1u;
    start_in_flight=0u;
    get_in_flight=0u;
    queue_wait_armed=0u;
    /* Defer Start until processUART() permits actual UART traffic. */
    start_pending=1u;
}
void abs_c1_service_poll(uint32_t now_ms) {
    if (!enabled) return;
    if (queue_wait_armed && (start_in_flight || get_in_flight) &&
        (uint32_t)(now_ms - queued_at_ms) >= ABS_UART_QUEUE_WAIT_MAX_MS) {
        service.timed_out=1u;
        enabled=0u;
        queue_wait_armed=0u;
        return;
    }
    if (start_pending) {
        /* Do not start the poll budget while the Start command is pending. */
        uint8_t msg[4]={C2BusID,C2cmdAbsFaultsStart,0,0};
        encode_byte(&msg[2],service.receiver.id);
        if (!uart_try_enqueue_abs(msg,sizeof(msg))) {
            service.timed_out=1u;
            enabled=0u;
            start_pending=0u;
            return;
        }
        start_pending=0u;
        start_in_flight=1u;
        queued_at_ms=now_ms;
        queue_wait_armed=1u;
        /* Allow processUART() to transmit Start before scheduling Get. */
        return;
    }
    if (start_in_flight) return; /* Wait until Start actually enters UART hardware. */
    uint8_t frame[ABS_PROTO_FRAME];
    /* A dropped reply makes the transfer incomplete: fail closed instead of
       committing a potentially truncated DTC list. */
    if (abs_c1_inbox_dropped() != 0u) {
        service.timed_out = 1u;
        enabled = 0u;
        return;
    }
    while (abs_c1_inbox_pop(frame)) {
        (void)abs_transport_reply(&service,frame,C1BusID);
        /* Once a transfer is complete or rejected, stop consuming replies.
           Late frames cannot alter the published terminal state. */
        if (service.receiver.status == 'E' ||
            (service.receiver.status == 'R' && service.receiver.total_set &&
             service.receiver.next == service.receiver.kept)) {
            enabled = 0u;
            return;
        }
    }
    if (abs_c1_inbox_dropped() != 0u) {
        service.timed_out = 1u;
        enabled = 0u;
        return;
    }
    if (get_in_flight) return; /* No duplicate Get while it is queued. */
    uint8_t index;
    int result=abs_transport_tick(&service,now_ms,&index);
    if (result < 0) {
        /* Timeout or terminal transport state: do not keep polling. */
        enabled = 0u;
        return;
    }
    if(result==1) {
        uint8_t msg[6]={C2BusID,C2cmdAbsFaultsGet,0,0,0,0};
        encode_byte(&msg[2],service.receiver.id);
        encode_byte(&msg[4],index);
        if (uart_try_enqueue_abs(msg,sizeof(msg))) {
            get_in_flight=1u;
            queued_at_ms=now_ms;
            queue_wait_armed=1u;
        } else {
        service.timed_out = 1u;
        enabled = 0u;
    }
    }
}
void abs_c1_service_tx_started(uint8_t command, uint8_t id_hi, uint8_t id_lo, uint32_t now_ms) {
    if (!enabled) return;
    uint8_t expected[2];
    encode_byte(expected, service.receiver.id);
    if (id_hi != expected[0] || id_lo != expected[1]) return;
    if (command == C2cmdAbsFaultsStart && start_in_flight) {
        start_in_flight=0u;
        queue_wait_armed=0u;
        service.last_poll_ms=now_ms;
        service.poll_started=1u; /* First Get after the normal poll interval. */
    } else if (command == C2cmdAbsFaultsGet && get_in_flight) {
        get_in_flight=0u;
        queue_wait_armed=0u;
        service.last_poll_ms=now_ms; /* Retry budget starts on actual transmit. */
    }
}
uint8_t abs_c1_service_tx_is_current(uint8_t command, uint8_t id_hi, uint8_t id_lo) {
    uint8_t expected[2];
    if (!enabled) return 0u;
    encode_byte(expected, service.receiver.id);
    if (id_hi != expected[0] || id_lo != expected[1]) return 0u;
    if (command == C2cmdAbsFaultsStart) return start_in_flight;
    if (command == C2cmdAbsFaultsGet) return get_in_flight;
    return 0u;
}
const AbsTransport *abs_c1_service_state(void) { return &service; }
#else
void abs_c1_service_begin(uint8_t id) {(void)id;}
void abs_c1_service_poll(uint32_t now_ms) {(void)now_ms;}
void abs_c1_service_tx_started(uint8_t command, uint8_t id_hi, uint8_t id_lo, uint32_t now_ms) {(void)command;(void)id_hi;(void)id_lo;(void)now_ms;}
uint8_t abs_c1_service_tx_is_current(uint8_t command, uint8_t id_hi, uint8_t id_lo) {(void)command;(void)id_hi;(void)id_lo;return 0u;}
const AbsTransport *abs_c1_service_state(void) { return 0; }
#endif
