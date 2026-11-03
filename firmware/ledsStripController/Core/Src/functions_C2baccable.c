/*
 * functions_C2baccable.c
 *
 *  Created on: May 2, 2025
 *      Author: GauchoHP
 */
#include "functions_C2baccable.h"
#include "functions_C2baccable.h"

#if defined(C2baccable)
/* @Shadin: C2 ABS response sender moved from abs_sender.c;
 * original protocol checks and UART payload semantics preserved. */
void abs_sender_start(AbsSender *s,uint8_t id){memset(s,0,sizeof(*s));s->id=id;s->status='B';}
void abs_sender_ready(AbsSender *s,uint16_t total,uint8_t kept,const uint8_t codes[][3]){
 if (!s) return;
 /* The UART wire format carries an 8-bit total count. Reject unrepresentable
  * or inconsistent results rather than silently reporting success. */
 if (total > UINT8_MAX || kept != (total > ABS_PROTO_MAX ? ABS_PROTO_MAX : total) ||
     (kept != 0u && codes == NULL)) {
  abs_sender_error(s, 'E');
  return;
 }
 s->kept=kept;s->total=total;s->status='R';
 for(unsigned i=0;i<kept;i++)memcpy(s->codes[i],codes[i],3);
}
void abs_sender_error(AbsSender *s,uint8_t reason){s->status='E';s->reason=reason;}
int abs_sender_get(const AbsSender *s,uint8_t request_id,uint8_t index,uint8_t route,uint8_t out[ABS_PROTO_FRAME]){
 if(!s||request_id!=s->id||index>s->kept)return -1;
 AbsReply r={0};r.id=s->id;r.status=s->status;r.reason=s->reason;
 r.total=s->total>255?255:(uint8_t)s->total;
 r.kept=s->kept;r.index=index;
 if(r.status=='R'&&index<s->kept)memcpy(r.code,s->codes[index],3);
 return abs_encode(out,route,&r);
}



	//The ABS faults are read here, on the C2 bus, for Read Faults on C1 (protocol in uart.h, FAULTS ABS PROTOCOL). C1 asks the
	//reading with C2cmdAbsFaultsStart and collects the result with C2cmdAbsFaultsGet; C2 answers only inside its normal reply
	//window, with messages whose payload is made of letters and hex digits only. The decoding of the ABS answers is in
	//processingExtendedMessage.c. The elm327 bridge is never used.

	//1 while the ABS is used by dyno or front brake (same ECU 0x18DA28F1): the reading is not started, or is stopped
	static uint8_t absFaultsAbsBusy(void){
		return (DynoStateMachine!=0xff || DynoModeEnabled || front_brake_forced!=0) ? 1 : 0;
	}

	/* @Shadin: Legacy two-DTC UART sender was replaced by the transaction-ID
	 * protocol. C1 receives queued frames via abs_c1_inbox and polls via
	 * abs_c1_service; C2 serves Start/Get using abs_sender.
	 * This comment documents wiring only; build and vehicle validation remain
	 * required for this refactored package. */

	static AbsSender absC2Sender;
	static uint8_t absC2FailureReason = 'E';
	static void absFaultsProcess(void){
		if(absFaultsStartRequest){
			abs_sender_start(&absC2Sender, absFaultsRequestId);
			absC2FailureReason = 'E';
			absFaultsStartRequest = 0;
			absFaultsDTCcount   = 0;
			absFaultsDTCtotal   = 0;
			absFaultsRxReceived = 0;
			absFaultsRxExpected = 0;
			absFaultsRecordFill = 0;
			absFaultsRxNextSN   = 1;
			absFaultsResponsePending = 0;
			absFaultsTimer      = currentTime;
			if(absFaultsAbsBusy()){
				absC2FailureReason = 'B';
				absFaultsState = 4; //ABS busy with dyno / front brake: C1 shows the faults of BODY and ECM
			}else{
				absFaultsTxHeader.DLC = 3;
				absFaultsTxData[0]    = 0x02; // PCI: SF 2 byte
				absFaultsTxData[1]    = 0x10; // SID: DiagnosticSessionControl
				absFaultsTxData[2]    = 0x03; // sub: extendedDiagnosticSession
				can_tx(&absFaultsTxHeader, absFaultsTxData);
				absFaultsState = 0;
			}
		}
		if(absFaultsState<3){
			if(absFaultsAbsBusy()){
				absC2FailureReason = 'B';
				absFaultsState = 4; //dyno or front brake started meanwhile: they own the ABS
			}else if(currentTime-absFaultsTimer > (absFaultsResponsePending ? 5000 : 2000)){
				absC2FailureReason = 'T';
				absFaultsState = 4; //no answer from the ABS
			}
		}
		if(absFaultsState!=snifferAbsFaultsLastState){

			snifferAbsFaultsLastState=absFaultsState;
		}
		if(absFaultsState == 3 && absC2Sender.status == 'B') {
			abs_sender_ready(&absC2Sender, absFaultsDTCtotal,
			                    absFaultsDTCcount, (const uint8_t (*)[3])absFaultsDTCbytes);
		} else if(absFaultsState == 4 && absC2Sender.status != 'E') {
			abs_sender_error(&absC2Sender, absC2FailureReason);
		}
		if(absFaultsGetIndex!=0xFF){
			uint8_t index=absFaultsGetIndex;
			absFaultsGetIndex=0xFF;
			uint8_t frame[ABS_PROTO_FRAME];
			if (abs_sender_get(&absC2Sender, absFaultsGetRequestId,
			                   index, C1BusID, frame) == 0) {
				addToUARTSendQueue(frame, ABS_PROTO_FRAME);
			}
		}
	}
	//readFaults ABS 10/10/2026 - END

	void C2PeriodicCheck(){
        absFaultsProcess();
		if(DynoStateMachine!=0xff){ //if state machine in progress
			if(currentTime-DynoStateMachineLastUpdateTime> 4000){ //if older than 4 sec
				DynoStateMachine=0xff; //timeout. stop any sequence
				//send message to master to inform about the status of Dyno
				uint8_t tmpArr2[2]={C1BusID,C1cmdDynoNotActive};
				if(DynoModeEnabled) tmpArr2[1]=C1cmdDynoActive;
				addToUARTSendQueue(tmpArr2, 2);
			}
		}


		if(DynoModeEnabled){
			//send tester presence each 450msec if dyno is enabled
			if(currentTime-last_sent_tester_presence_msg_time>500){ //enter here once each 500msec
				last_sent_tester_presence_msg_time=currentTime;
				DYNO_msg_header.DLC=DYNO_msg_data[4][0]+1;
				can_tx(&DYNO_msg_header, DYNO_msg_data[4]); //add to the transmission queue

			}
		}
		if(front_brake_forced==255){ //request to disable Front brake
			front_brake_forced=0;
			//just reply to C1 baccable
			uint8_t tmpArr[2]={C1BusID,C1cmdNormalFrontBrake};
			addToUARTSendQueue(tmpArr, 2);
			can_tx(&rearBrakeMsgHeader[0], rearBrakeMsgData[0]); //send message to return control to ECU


		}

		if(front_brake_forced==5){
			front_brake_forced=4;
			//send reply via serial line to C1 to inform that front brake is going to be forced
			uint8_t tmpArr[2]={C1BusID,C1cmdForceFrontBrake};
			addToUARTSendQueue(tmpArr, 2);
		}

		if(front_brake_forced>0){ //force front brake
			//we shall send msg sequence
			if(currentTime-last_sent_rear_brake_msg_time>500){ //enter here once each 500msec
				last_sent_rear_brake_msg_time=currentTime;
				onboardLed_blue_on();
				can_tx(&rearBrakeMsgHeader[front_brake_forced-1], rearBrakeMsgData[front_brake_forced-1]); //send message to force front brakes

				switch(front_brake_forced){
					case 4:
					case 3:
						front_brake_forced--;
						break;
					case 2:
						front_brake_forced++;
						break;
					default:
						break;
				}
			}
		}
		
		// @netzmark PDC DISABLE code - simulated push of the park sensors button, followed by our own release.
		// Not done in case 0x5B0 because that frame is repeated every 1-2sec and the PDC reacts on the release.
		//=========================================================================
		//TOGGLE PDC SHOT followed with Release button simulation (to get faster reaction)
		// this made intentionally NOT in case 0x000005B0 of processingStandardMessages.c because:
		// the system frame 0x5B0 containing the button status is repeated very slowly (1-2sec interval),
		// PDC system reacts on Release D1:0x00 after Push D1:0x20
		// so the speed of react would be random and depend on the moment we sent the disable between the system frames
		// that's why we follow push with our independent release code
		//=========================================================================

		if (parkSensorsMuteFunctionEnabled){
		    if (requestToTogglePDC == 1) {
		        if (pdc_send_counter == 0) {
		            pdc_send_counter = 1;
		            last_pdc_shot_time = currentTime;
		            pdcMsgData[1] = 0x20;  // push button
		            can_tx(&pdcMsgHeader, pdcMsgData); // sent
		        }

		        // Prepare to send pdc button release code
		        if (pdc_send_counter == 1 && (currentTime - last_pdc_shot_time > 50)) { //changing the time we can allow for short beep before PDC disabling
		            pdc_send_counter = 0;
		            pdcMsgData[1] = 0x00;  // release button
		            can_tx(&pdcMsgHeader, pdcMsgData); // sent
		            requestToTogglePDC = 0; // set after push and release done
		        }
		    }
		}

//		=========================================================================
//		Simplified version with no release simulation
//		=========================================================================
//		if (parkSensorsMuteFunctionEnabled){
//			if (requestToTogglePDC == 1) {
//				requestToTogglePDC = 0;
//				pdcMsgData[1] = 0x20;  // push button// push button sent
//				can_tx(&pdcMsgHeader, pdcMsgData);
//			}
//		}
		// @netzmark PDC DISABLE code - end
	}		


	void dynoToggle(){
		if(DynoStateMachine == 0xff){ // there is no dyno Start sequence in progress
			DynoStateMachine=0; //state machine
			ESCandTCinversion=0; //do not change ESC and TC if dynomode is requested
			DYNO_msg_header.DLC=DYNO_msg_data[DynoStateMachine][0]+1; //length of DIAGNOSTIC SESSION msg
			can_tx(&DYNO_msg_header, DYNO_msg_data[DynoStateMachine]); //add to the transmission queue
			onboardLed_blue_on();
			DynoStateMachineLastUpdateTime=currentTime;//save last time seen
			//wait the feedback from ECU
		}
	}

#endif
