#ifndef ABS_SENDER_H
#define ABS_SENDER_H
#include "abs_proto.h"
typedef struct {
 uint8_t id, status, reason, kept, codes[ABS_PROTO_MAX][3];
 uint16_t total;
} AbsSender;
void abs_sender_start(AbsSender *s,uint8_t id);
void abs_sender_ready(AbsSender *s,uint16_t total,uint8_t kept,const uint8_t codes[][3]);
void abs_sender_error(AbsSender *s,uint8_t reason);
int abs_sender_get(const AbsSender *s,uint8_t request_id,uint8_t index,uint8_t route,uint8_t out[ABS_PROTO_FRAME]);
#endif
