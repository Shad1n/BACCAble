#ifndef DTC_ECU_RESULTS_H
#define DTC_ECU_RESULTS_H
#include <stdint.h>
#include "dtc_stream_decoder.h"
typedef struct {
    uint8_t codes[DTC_STREAM_MAX * 2u][3];
    uint8_t count;
    uint8_t body_end;
    uint8_t ecm_end;
    uint16_t total;
    uint16_t ecu_total[2]; /* accepted DTCs per active ECU */
} DtcEcuResults;
void dtcEcuResultsReset(DtcEcuResults *r);
/* Call only after complete, validated ECU response. Returns 0 for invalid ECU/order. */
uint8_t dtcEcuResultsCommit(DtcEcuResults *r, const DtcStreamDecoder *s, uint8_t ecu);
#endif
