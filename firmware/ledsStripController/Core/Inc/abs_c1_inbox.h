#ifndef ABS_C1_INBOX_H
#define ABS_C1_INBOX_H
#include <stdint.h>
#include "abs_proto.h"
/* Interrupt-safe single producer / single consumer FIFO. */
#define ABS_C1_INBOX_CAPACITY 8u
void abs_c1_inbox_reset(void);
int abs_c1_inbox_push_isr(const uint8_t *frame);
int abs_c1_inbox_pop(uint8_t frame[ABS_PROTO_FRAME]);
uint16_t abs_c1_inbox_dropped(void);
#endif
