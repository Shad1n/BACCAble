#ifndef DTC_STREAM_DECODER_H
#define DTC_STREAM_DECODER_H
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
#endif
