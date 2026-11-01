#ifndef ABS_C1_SERVICE_H
#define ABS_C1_SERVICE_H
#include <stdint.h>
#include "abs_transport.h"
/* Explicit opt-in: this module does not start ABS diagnostics on its own. */
void abs_c1_service_begin(uint8_t transaction_id);
void abs_c1_service_poll(uint32_t now_ms);
const AbsTransport *abs_c1_service_state(void);
void abs_c1_service_tx_started(uint8_t command, uint8_t id_hi, uint8_t id_lo, uint32_t now_ms);
uint8_t abs_c1_service_tx_is_current(uint8_t command, uint8_t id_hi, uint8_t id_lo);
#endif
