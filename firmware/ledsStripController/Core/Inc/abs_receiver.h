#ifndef ABS_RECEIVER_H
#define ABS_RECEIVER_H
#include "abs_proto.h"
typedef struct { uint8_t id, next, kept, status, reason, codes[ABS_PROTO_MAX][3]; uint16_t total; uint8_t total_set; } AbsReceiver;
void abs_receiver_init(AbsReceiver *r,uint8_t id);
/* 1 = accepted record, 2 = completed, 0 = busy, -1 = invalid, -2 = stale, -3 = terminal error */
int abs_receiver_accept(AbsReceiver *r,const uint8_t frame[ABS_PROTO_FRAME],uint8_t route);
#endif
