#include "dtc_ecu_results.h"
#include <string.h>
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
