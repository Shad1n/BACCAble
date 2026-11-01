#ifndef ABS_TRANSPORT_H
#define ABS_TRANSPORT_H
#include "abs_receiver.h"
#include <stdint.h>
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
#endif
