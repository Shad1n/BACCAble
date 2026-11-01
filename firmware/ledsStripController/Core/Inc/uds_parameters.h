/*
 * uds_parameters.h
 *
 *  Created on: Nov 22, 2025
 *      Author: emanu
 */

#ifndef INC_UDS_PARAMETERS_H_
	#define INC_UDS_PARAMETERS_H_
	#include "compile_time_defines.h"
	#include "stm32f0xx_hal.h"

	//this is used to invert bytes order in a 32 bit integer
	#define SWAP_UINT32(x) (((uint32_t)(x) >> 24) & 0x000000FF) | \
						   (((uint32_t)(x) >> 8)  & 0x0000FF00) | \
						   (((uint32_t)(x) << 8)  & 0x00FF0000) | \
						   (((uint32_t)(x) << 24) & 0xFF000000)
	typedef struct{
		char		name[DASHBOARD_MESSAGE_MAX_LENGTH +10];
		uint8_t 	udsParamId[2];
	} uds_params_couple_element;

	typedef struct{
		uint32_t 	reqId;
		uint8_t		reqLen;
		uint32_t 	reqData;
		uint32_t 	replyId;
		uint8_t		replyLen;
		uint8_t		replyOffset;
		int32_t		replyValOffset;
		float		replyScale;
		int32_t		replyScaleOffset;
		uint8_t		replyMeasurementUnit[7];
		uint8_t		replyDecimalDigits;
	} uds_param_single_element;
	
	/* @netzmark: to create multi parameters screens */
	typedef struct {
	    uint8_t     groupId;      	// Range > 200
	    uint8_t     udsParamId[4]; 	// Array of 4 real IDs (0- MAX_UDS_PARAMS = 100 today)
	} uds_params_group_element;

	extern const uds_params_group_element uds_params_groups_array[];
	/* end */

	extern float dashboardParamCouple[2];
	extern float dashboardParamMaxHold[2];
	extern uint8_t shownParamsArray[240];
	extern uint8_t params_setup_dashboardPageIndex;
	extern uint8_t total_pages_in_params_setup_dashboard_menu;
	extern uint8_t total_pages_in_dashboard_menu_gasoline;
	extern uint8_t total_pages_in_dashboard_menu_diesel;
	extern const uds_params_couple_element uds_params_array[2][60];
	extern uint8_t currentParamElementSelection;

	/* @netzmark: Automatic calculation of single_uds_params_array size, not constant 100 */
	#define UDS_STOP_ID 200		//uds_params_group_element guard stop
	#define MAX_UDS_PARAMS 100	// the number to increase if the array will be spread.
	//extern const uds_param_single_element single_uds_params_array[100]; //Gaucho ori replaced with makro because of use in many places
	extern const uds_param_single_element single_uds_params_array[MAX_UDS_PARAMS];
	extern float uds_values_cache[MAX_UDS_PARAMS];
	/* end */

	extern const char* dpfRegenEnumStrings[];
	extern const char* setbeltEnumStrings[];
	extern const uint8_t gearArray[11];
	extern const char* speedStatisticEnumStrings[];


#endif /* INC_UDS_PARAMETERS_H_ */
