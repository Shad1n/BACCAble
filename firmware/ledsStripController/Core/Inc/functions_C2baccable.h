/*
 * functions_C2baccable.h
 *
 *  Created on: May 2, 2025
 *      Author: GauchoHP
 */

#ifndef INC_FUNCTIONS_C2BACCABLE_H_
	#define INC_FUNCTIONS_C2BACCABLE_H_

	#include "globalVariables.h"

	void C2PeriodicCheck();
	void dynoToggle();

/* @Shadin: Interface moved from abs_sender.h; original declarations and comments preserved. */
#include "uart.h" /* @Shadin: ABS protocol declarations now in uart.h. */
typedef struct {
 uint8_t id, status, reason, kept, codes[ABS_PROTO_MAX][3];
 uint16_t total;
} AbsSender;
void abs_sender_start(AbsSender *s,uint8_t id);
void abs_sender_ready(AbsSender *s,uint16_t total,uint8_t kept,const uint8_t codes[][3]);
void abs_sender_error(AbsSender *s,uint8_t reason);
int abs_sender_get(const AbsSender *s,uint8_t request_id,uint8_t index,uint8_t route,uint8_t out[ABS_PROTO_FRAME]);


#endif /* INC_FUNCTIONS_C2BACCABLE_H_ */
