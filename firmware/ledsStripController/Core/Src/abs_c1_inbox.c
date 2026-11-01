#include "abs_c1_inbox.h"
#include <string.h>
#include "compile_time_defines.h"
#if defined(C1baccable)
#include "stm32f0xx.h"
#endif
static uint8_t frames[ABS_C1_INBOX_CAPACITY][ABS_PROTO_FRAME];
static volatile uint8_t head, tail;
static volatile uint16_t dropped;
void abs_c1_inbox_reset(void) {
#if defined(C1baccable)
    /* Preserve the caller's interrupt state, including calls made with IRQs masked. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
#endif
    head = 0u;
    tail = 0u;
    dropped = 0u;
#if defined(C1baccable)
    __set_PRIMASK(primask);
#endif
}
int abs_c1_inbox_push_isr(const uint8_t *frame) {
    uint8_t next;
    if (!frame) return 0;
    next=(uint8_t)((head+1u)%ABS_C1_INBOX_CAPACITY);
    if (next==tail) { ++dropped; return 0; }
    memcpy(frames[head],frame,ABS_PROTO_FRAME);
    head=next;
    return 1;
}
int abs_c1_inbox_pop(uint8_t frame[ABS_PROTO_FRAME]) {
    if (!frame || tail==head) return 0;
    memcpy(frame,frames[tail],ABS_PROTO_FRAME);
    tail=(uint8_t)((tail+1u)%ABS_C1_INBOX_CAPACITY);
    return 1;
}
uint16_t abs_c1_inbox_dropped(void) { return dropped; }
