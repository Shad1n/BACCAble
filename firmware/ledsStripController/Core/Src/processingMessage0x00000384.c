/*
 * processingMessage0x00000226.c
 *
 *  Created on: May 3, 2025
 *      Author: GauchoHP
 */

#include "processingMessage0x00000384.h"
#include "dna_stageg.h"
#include "stageg_ign_diag.h"

/* Passive snapshots, 8-event ring. No CAN TX or flash writes. */
#if defined(C1baccable)
volatile stageg_ign_event_t stageg_ign_events[8];
volatile uint32_t stageg_ign_event_count;
static uint8_t last_ign=0xFFu, last_power=0xFFu, last_dna=0xFFu;
static uint8_t last_rfhub=0xFFu, last_stopstart=0xFFu;
volatile uint32_t stageg_ign_last_384_ms, stageg_ign_last_1ef_ms, stageg_ign_last_0fc_ms;
volatile uint8_t stageg_ign_seen_384, stageg_ign_seen_1ef, stageg_ign_seen_0fc;
volatile uint8_t stageg_ign_live_ign=0xFFu, stageg_ign_live_power=0xFFu, stageg_ign_live_dna=0xFFu;
volatile uint8_t stageg_ign_live_rfhub=0xFFu, stageg_ign_live_ss=0xFFu;
static void stageg_ign_snapshot(uint8_t reason);
static uint8_t timeout_mask;
volatile uint16_t stageg_ign_timeout_count[3], stageg_ign_recovery_count[3];
static void stageg_ign_count(volatile uint16_t *counter) { if (*counter != 0xFFFFu) ++(*counter); }
/* Passive RFHUB non-RUN -> RUN observation; no DNA/CAN actions. */
static uint8_t ignition_nonrun_pending;
static uint8_t ignition_nonrun_confirmed;
static uint32_t ignition_nonrun_since;
/* Diagnostic-only transition counters: no CAN TX, flash writes, or DNA actions. */
volatile uint16_t stageg_ign_rfhub_short_count, stageg_ign_rfhub_long_count;
volatile uint16_t stageg_ign_rfhub_last_duration_ms;
volatile uint16_t stageg_ign_rfhub_last_short_ms, stageg_ign_rfhub_last_long_ms;
volatile uint16_t stageg_ign_384_5to4_count;
static uint8_t stageg_ign_previous_384 = 0xFFu;
/* Passive, one-shot confirmed ignition return. Never sends CAN or writes flash. */
volatile uint16_t stageg_ign_confirmed_cycle_count;
volatile uint16_t stageg_dryrun_ready_count, stageg_dryrun_skip_count;
volatile uint8_t stageg_dryrun_last_target = 0xFFu;
/* Armed is observational only: no DNA guard mutation or CAN enqueue. */
volatile uint16_t stageg_armed_count, stageg_armed_already_count;
volatile uint8_t stageg_armed_last_target = 0xFFu;
/* Guard actions are counted only when tick() actually requests them. */
volatile uint16_t stageg_guard_request_count, stageg_guard_manual_count;
volatile uint8_t stageg_guard_request_target = 0xFFu;
static uint8_t stageg_ign_await_run_confirmation;
static uint32_t stageg_ign_run_return_ms;
#define STAGEG_IGN_RUN_CONFIRM_WINDOW_MS 5000u
#define STAGEG_IGN_NONRUN_CONFIRM_MS 1000u
#define STAGEG_CAN_TIMEOUT_MS 1500u
void stageg_ign_diag_note_0fc(void) {
    stageg_ign_last_0fc_ms=currentTime; stageg_ign_seen_0fc=1u;
    if (timeout_mask & 4u) { timeout_mask &= (uint8_t)~4u; stageg_ign_count(&stageg_ign_recovery_count[2]); stageg_ign_snapshot(11u); }
}
void stageg_ign_diag_check_timeouts(uint32_t now) {
    if (stageg_ign_await_run_confirmation) {
        uint32_t elapsed = (uint32_t)(now - stageg_ign_run_return_ms);
        if (elapsed > STAGEG_IGN_RUN_CONFIRM_WINDOW_MS) {
            stageg_ign_await_run_confirmation = 0u;
        } else if (stageg_ign_seen_384 && stageg_ign_seen_1ef && stageg_ign_seen_0fc &&
                   (uint32_t)(now - stageg_ign_last_384_ms) <= STAGEG_CAN_TIMEOUT_MS &&
                   (uint32_t)(now - stageg_ign_last_1ef_ms) <= STAGEG_CAN_TIMEOUT_MS &&
                   (uint32_t)(now - stageg_ign_last_0fc_ms) <= STAGEG_CAN_TIMEOUT_MS &&
                   stageg_ign_last_384_ms >= stageg_ign_run_return_ms &&
                   stageg_ign_live_rfhub == 0x08u && stageg_ign_live_ign == 4u &&
                   currentRpmSpeed > 0u) {
            stageg_ign_await_run_confirmation = 0u;
            stageg_ign_count(&stageg_ign_confirmed_cycle_count);
            stageg_ign_snapshot(12u);
            /* The event only queues a flag; dna_stageg_tick() owns guard mutation and restore. */
            dna_stageg_note_ignition_return();
            /* Keep the existing eligibility counters for ATIGLOG diagnostics. */
            uint8_t dry_target = 0xFFu;
            if (dna_stageg_dryrun_target(&dry_target)) {
                stageg_dryrun_last_target = dry_target;
                stageg_ign_count(&stageg_dryrun_ready_count);
                stageg_ign_snapshot(13u);
                /* Once per confirmed cycle, and only with a fresh observed mode.
                 * A matching mode is not a restore candidate. */
                if (stageg_ign_live_dna == dry_target) {
                    stageg_ign_count(&stageg_armed_already_count);
                    stageg_armed_last_target = 0xFFu;
                    stageg_ign_snapshot(16u);
                } else {
                    stageg_armed_last_target = dry_target;
                    stageg_ign_count(&stageg_armed_count);
                    stageg_ign_snapshot(15u);
                }
            } else {
                stageg_dryrun_last_target = 0xFFu;
                stageg_ign_count(&stageg_dryrun_skip_count);
                stageg_ign_snapshot(14u);
            }
        }
    }
    if (ignition_nonrun_pending && !ignition_nonrun_confirmed &&
        (uint32_t)(now - ignition_nonrun_since) >= STAGEG_IGN_NONRUN_CONFIRM_MS &&
        stageg_ign_seen_1ef &&
        (uint32_t)(now - stageg_ign_last_1ef_ms) <= STAGEG_CAN_TIMEOUT_MS &&
        stageg_ign_live_rfhub != 0x08u) {
        ignition_nonrun_confirmed = 1u;
        stageg_ign_snapshot(8u); /* sustained non-RUN, not an ignition cycle */
    }
    /* Poll from normal C1 loop only; no CAN transmission or flash writes. */
    if(stageg_ign_seen_384 && (uint32_t)(now-stageg_ign_last_384_ms)>STAGEG_CAN_TIMEOUT_MS && !(timeout_mask&1u)) {
        timeout_mask |= 1u; stageg_ign_count(&stageg_ign_timeout_count[0]); stageg_ign_snapshot(4u);
    }
    if(stageg_ign_seen_1ef && (uint32_t)(now-stageg_ign_last_1ef_ms)>STAGEG_CAN_TIMEOUT_MS && !(timeout_mask&2u)) {
        timeout_mask |= 2u; stageg_ign_count(&stageg_ign_timeout_count[1]); stageg_ign_snapshot(5u);
    }
    if(stageg_ign_seen_0fc && (uint32_t)(now-stageg_ign_last_0fc_ms)>STAGEG_CAN_TIMEOUT_MS && !(timeout_mask&4u)) {
        timeout_mask |= 4u; stageg_ign_count(&stageg_ign_timeout_count[2]); stageg_ign_snapshot(6u);
    }
}
void stageg_ign_diag_note_guard_request(uint8_t target) {
    stageg_ign_count(&stageg_guard_request_count);
    stageg_guard_request_target=target;
}
void stageg_ign_diag_note_manual_selection(void) {
    stageg_ign_count(&stageg_guard_manual_count);
}
static void stageg_ign_snapshot(uint8_t reason) {
    uint32_t slot=stageg_ign_event_count % 8u;
    volatile stageg_ign_event_t *e=&stageg_ign_events[slot];
    e->ms=currentTime;
    e->rpm=(uint16_t)currentRpmSpeed;
    e->ignition_raw=last_ign;
    e->power_raw=last_power;
    e->dna_mode=last_dna;
    e->rfhub_raw=last_rfhub;
    e->stopstart_raw=last_stopstart;
    e->event=reason;
    stageg_ign_event_count++;
}
void stageg_ign_diag_note_rfhub(uint8_t raw) {
    stageg_ign_last_1ef_ms=currentTime; stageg_ign_seen_1ef=1u;
    stageg_ign_live_rfhub=raw;
    if (timeout_mask & 2u) { timeout_mask &= (uint8_t)~2u; stageg_ign_count(&stageg_ign_recovery_count[1]); stageg_ign_snapshot(10u); }
    if (raw != 0x08u) {
        if (!ignition_nonrun_pending) {
            ignition_nonrun_pending = 1u;
            ignition_nonrun_confirmed = 0u;
            ignition_nonrun_since = currentTime;
        }
    } else if (ignition_nonrun_pending) {
        /* Candidate is logged even when too brief to be confirmed. */
        uint32_t elapsed = (uint32_t)(currentTime - ignition_nonrun_since);
        stageg_ign_rfhub_last_duration_ms = (uint16_t)(elapsed > 65535u ? 65535u : elapsed);
        if (elapsed >= STAGEG_IGN_NONRUN_CONFIRM_MS && ignition_nonrun_confirmed) {
            stageg_ign_count(&stageg_ign_rfhub_long_count);
            stageg_ign_rfhub_last_long_ms = stageg_ign_rfhub_last_duration_ms;
            stageg_ign_await_run_confirmation = 1u;
            stageg_ign_run_return_ms = currentTime;
        } else {
            stageg_ign_count(&stageg_ign_rfhub_short_count);
            stageg_ign_rfhub_last_short_ms = stageg_ign_rfhub_last_duration_ms;
        }
        stageg_ign_snapshot(7u);
        ignition_nonrun_pending = 0u;
        ignition_nonrun_confirmed = 0u;
    }
    if(raw!=last_rfhub) { last_rfhub=raw; stageg_ign_snapshot(2u); }
}
void stageg_ign_diag_note_stopstart(uint8_t raw) {
    stageg_ign_live_ss=raw;
    if(raw!=last_stopstart) { last_stopstart=raw; stageg_ign_snapshot(3u); }
}
static void stageg_ign_diag_record(void) {
    stageg_ign_last_384_ms=currentTime; stageg_ign_seen_384=1u;
    if (timeout_mask & 1u) { timeout_mask &= (uint8_t)~1u; stageg_ign_count(&stageg_ign_recovery_count[0]); stageg_ign_snapshot(9u); }
    uint8_t ign=(uint8_t)((rx_msg_data[0] >> 1) & 7u);
    uint8_t power=(uint8_t)((rx_msg_data[3] >> 4) & 3u);
    uint8_t dna=(uint8_t)(rx_msg_data[1] & 0x7Cu);
    stageg_ign_live_ign=ign; stageg_ign_live_power=power; stageg_ign_live_dna=dna;
    if (stageg_ign_previous_384 == 5u && ign == 4u) {
        stageg_ign_count(&stageg_ign_384_5to4_count);
    }
    stageg_ign_previous_384 = ign;
    if(ign!=last_ign || power!=last_power || dna!=last_dna) {
        last_ign=ign; last_power=power; last_dna=dna;
        stageg_ign_snapshot(1u);
    }
}
#endif

void processingMessage0x00000384(){
	// pressStartButton
	#if defined(C1baccable)
        stageg_ign_diag_record();
		if(currentDNAmode!=(rx_msg_data[1]& 0x7C)){ //RDNA mode was changed, reset the ESCandTCinversion
			ESCandTCinversion=0;
		}
		//current DNA mode, also called "Drive Style Status" (RDNA mode) is on byte 1 from bit 6 to bit 2 (0x0=Natural [shifted by 2 bits becomes 0x00], 0x2=dynamic [shifted by 2 bits becomes 0x08], 0x4=AllWeather [shifted by 2 bits becomes 0x10], 0xC=race [shifted by 2 bits becomes 0x30]
		currentDNAmode=rx_msg_data[1] & 0x7C;
		dna_stageg_note_384(currentTime);
		if(QV_exhaust_flap_function_enabled) exhaustValvesOnDNAmode(currentDNAmode); //7C is the mask from bit 6 to 2 (we avoid bit shift to save cpu loops)
		if (ESCandTCinversion){

			if(currentDNAmode!=0x30){ //if not in race
				rx_msg_data[1] = (rx_msg_data[1] & ~0x7C) | 0x30;  //set Race mode (0x30) to show on IPC the race screen (msg from body to TCM, IPC, ECM, DTCM, DCTM, DASM, CDCM, BCM)
			}
			//uint8_t tmpCounter=(rx_msg_data[6] & 0x0F)+1;
			//if(tmpCounter>0x0F) tmpCounter=0;
			//rx_msg_data[6]= (rx_msg_data[6] & 0xF0) | tmpCounter;   //increment counter
			rx_msg_data[7]=calculateCRC(rx_msg_data,rx_msg_header.DLC); //update CRC
			can_tx((CAN_TxHeaderTypeDef *)&rx_msg_header, rx_msg_data); //transmit the modified packet
			//can_process(); //we try to send it ASAP - I commented it since It had no success
			//onboardLed_blue_on();
		}
		//memcpy(&DNA_msg_data, &rx_msg_data, 8);

	#endif

	#if defined(C2baccable)
		//on C2 can bus, msg 0x384 contains, in byte3, bit6 contains left stalk button press status (LANE indicator button)
		if((rx_msg_data[3] & 0x40) ==0x40){ // left stalk button was pressed (lane following indicator)
			if(LANEbuttonPressBeginTime==0){ //if button was not pressed, and now it is pressed
				LANEbuttonPressBeginTime=currentTime;//save current time it was pressed (press begin)
				numberOfLaneButtonClicks++;
				if (numberOfLaneButtonClicks==1) LANEbuttonFirstClickTime=currentTime;
			}

			if ((currentTime-LANEbuttonPressBeginTime)>2000) { //if pressed since 2 seconds

				if(function_esc_tc_customizator_enabled){
					ESCandTCinversion=!ESCandTCinversion; //toggle the status
					//if dyno is enabled or its change is in progress, avoid to switch ESP/TC.
					if(DynoModeEnabled || DynoStateMachine!=0xff) ESCandTCinversion=!ESCandTCinversion; //revert the change. won't do both things
				}
				if(ESCandTCinversion && function_show_race_mask){ //if enabled, notify C1 and BH
					uint8_t tmpArr1[2]={C1_Bh_BusID, C1BHcmdShowRaceScreen};
					addToUARTSendQueue(tmpArr1, 2);
				}else{
					uint8_t tmpArr1[2]={C1_Bh_BusID, C1BHcmdStopShowRaceScreen};
					addToUARTSendQueue(tmpArr1, 2);
				}

				onboardLed_blue_on();

				LANEbuttonPressBeginTime=0; //reset the timer, like if it was not pressed
				numberOfLaneButtonClicks=0;
			}
		}else{
			LANEbuttonPressBeginTime=0; //use this value to remember that button is not pressed

			if(currentTime-LANEbuttonFirstClickTime>1000){ // if more than 1 second is passed since first button click
				numberOfLaneButtonClicks=0; //reset also the counter of the number of consecutive clics :-)
			}


			if(numberOfLaneButtonClicks>=2){ //if double click
				numberOfLaneButtonClicks=0; //ensure we don't return here :-)
				//execute action :-)
				if(HAS_function_enabled){
					HAS_buttonPressRequested=5;
					//notify to C1
					uint8_t tmpArr0[2]={C1BusID,C1cmdLaneDoubleTap};
					addToUARTSendQueue(tmpArr0, 2);
				}
				onboardLed_blue_on();

			}
		}


		if(currentDNAmode!=(rx_msg_data[1]& 0x7C)){ //RDNA mode was changed, reset the ESCandTCinversion
			ESCandTCinversion=0;
		}
		//current DNA mode, also called "Drive Style Status" (RDNA mode) is on byte 1 from bit 6 to bit 2 (0x0=Natural [shifted by 2 bits becomes 0x00], 0x2=dynamic [shifted by 2 bits becomes 0x08], 0x4=AllWeather [shifted by 2 bits becomes 0x10], 0xC=race [shifted by 2 bits becomes 0x30]
		currentDNAmode=rx_msg_data[1] & 0x7C; //7C is the mask from bit 6 to 2 (we avoid bit shift to save cpu loops)
		if (ESCandTCinversion){
			//memcpy(&DNA_msg_data, &rx_msg_data, 8);
			if(currentDNAmode==0x30){ //race
				//DNA_msg_data[1]= (DNA_msg_data[1] & ~0x7C) | (0x08 & 0x7C); //set Dynamic mode (0x08) to enable ESC and TC
				rx_msg_data[1]= (rx_msg_data[1] & ~0x7C) | 0x08; //set Dynamic mode (0x08) to enable ESC and TC (msg from body to HAL, ORC, EPS, BSM. this disables controls)
			}else{
				//DNA_msg_data[1] = (DNA_msg_data[1] & ~0x7C) | (0x30 & 0x7C);  //set Race mode (0x30) to disable ESC and TC
				rx_msg_data[1] = (rx_msg_data[1] & ~0x7C) | 0x30;  //set Race mode (0x30) to disable ESC and TC
			}
			//uint8_t tmpCounter=(rx_msg_data[6] & 0x0F)+1;
			//if(tmpCounter>0x0F) tmpCounter=0;
			//rx_msg_data[6]= (rx_msg_data[6] & 0xF0) | tmpCounter;   //increment counter
			//rx_msg_data[7]=calculateCRC(rx_msg_data,rx_msg_header.DLC); //update CRC
			can_tx((CAN_TxHeaderTypeDef *)&rx_msg_header, rx_msg_data); //transmit the modified packet
			//can_process(); //we try to send it ASAP - I commented it since it had no success
			//onboardLed_blue_on();
		}
	#endif

	//Command Ignition Status is on byte0 from bit 3 to 1.
	//Command Ignition Fail Status is on byte 0 bit0 and in byte1 bit7.
	//Drive Style Status (RDNA mode) is on byte 1 from bit 6 to bit 2 (0x0=Natural, 0x2=dynamic, 0x4=AllWeather, 0xC=race)
	//External temperature is on byte 1 from bit 1 to 0 and on byte 2 from bit 7 to bit 1.
	//External temperature fail is on byte2 bit0
	//Low Beam Status is on byte3 bit7
	//Lane Indicator button status (left stalk button) is on byte 3 bit6.
	//Power Mode Status is on byte 3 from bit 5 to 4.
	//Park Brake Status is on byte 3 bit 3.
	//Int. Relay Fail Status is on byte 4 from bit 7 to 6
	//SuspensionLevel is on byte 5 bit0 and byte 6 bit7.


}
