/* Isolated adaptation of Gaucho's faultsConsumePayloadByte().
 * Stage C1a: not yet wired into the active diagnostic state machine. */
#include "dtc_stream_decoder.h"
#include <string.h>
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
