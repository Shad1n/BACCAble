#ifndef ABS_PROTO_H
#define ABS_PROTO_H
#include <stdint.h>
#include <stddef.h>
#define ABS_PROTO_FRAME 19u
#define ABS_PROTO_MAX 20u
#define ABS_PROTO_REPLY 0x29u
typedef struct { uint8_t id, status, reason, total, kept, index, code[3]; } AbsReply;
/* Format: route, command, id hex(2), status, reason, total hex(2), kept hex(2), index hex(2), DTC hex(6), pad. */
int abs_encode(uint8_t out[ABS_PROTO_FRAME], uint8_t route, const AbsReply *r);
int abs_decode(const uint8_t in[ABS_PROTO_FRAME], uint8_t route, AbsReply *r);
#endif
