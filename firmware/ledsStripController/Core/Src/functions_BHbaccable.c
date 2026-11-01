/*
 * functions_BHbaccable.c
 *
 *  Created on: May 2, 2025
 *      Author: GauchoHP
 */

#include "functions_BHbaccable.h"

#if defined(BHbaccable)

	// Stage D4: conservative, reversible display pacing experiment.
	// Retain atomic page snapshots and the existing frame sequence.
	#define BH_DASHBOARD_FRAME_INTERVAL_MS 40u

	// Stable source for a complete multi-frame dashboard transmission.
	static uint8_t dashboardTxSnapshot[DASHBOARD_MESSAGE_MAX_LENGTH];

	void BHbaccableInitCheck(){
		//let's open the can bus because we may need data
		can_set_bitrate(CAN_BITRATE_125K);//set can speed to 125kpbs
		can_enable(); //enable can port

		//prepare msg to send:
		//total frame number is on byte 0 from bit 7 to 3
		telematic_display_info_msg_data[0]=(telematic_display_info_msg_data[0] & ~0xF8) | ((telematic_display_info_field_totalFrameNumber<<3) & 0xF8);
		//infoCode is on byte1 from bit 5 to 0 (0x12=phone connected, 0x13=phone disconnected, 0x15=call in progress, 0x17=call in wait, 0x18=call terminated, 0x11=clear display, ...)
		telematic_display_info_msg_data[1]=(telematic_display_info_msg_data[1] & ~0x3F) | ((telematic_display_info_field_infoCode) & 0x3F);
		//I don't use UTF chars, but ascii, so bytes 2,4,6 can be set to zero
		telematic_display_info_msg_data[2]=0;
		telematic_display_info_msg_data[4]=0;
		telematic_display_info_msg_data[6]=0;

		//load stored params
		leftParkMirrorHorizontalPos		=(uint8_t)readFromFlashBH(1);
		leftParkMirrorVerticalPos		=(uint8_t)readFromFlashBH(2);
		rightParkMirrorHorizontalPos	=(uint8_t)readFromFlashBH(3);
		rightParkMirrorVerticalPos		=(uint8_t)readFromFlashBH(4);
		storeCurrentParkMirrorPosition	=(uint8_t)readFromFlashBH(5);
		leftMirrorHorizontalOperativePos=(uint8_t)readFromFlashBH(6);
		leftMirrorVerticalOperativePos	=(uint8_t)readFromFlashBH(7);
		rightMirrorHorizontalOperativePos=(uint8_t)readFromFlashBH(8);
		rightMirrorVerticalOperativePos =(uint8_t)readFromFlashBH(9);

	}


	//DNA memory 18/10/2026 - BEGIN
	//C1 asks a drive style (BHcmdDnaMemoryRestore) at the first engine start after the car slept. The DNA selector
	//(DSCM) sends 0x256 every ~250 ms: byte 0 = 0x40 up (A->N->D->R), 0x20 down, 0x00 not pressed. The presses are made
	//by sending the frame just received from the selector again, with byte 0 changed, right after it:
	// - short press (every step but D->R): one changed frame, then the next 0x256 is let through ("not pressed");
	// - long press (D->R, always the last step): every 0x256 is changed for DNA_RESTORE_LONG_PRESS_MS, then nothing
	//   (the following frames of the selector carry the release).
	//The whole sequence, from the current style to the wanted one, is worked out first and sent without checking the
	//style meanwhile. DNA_RESTORE_CHECK_DELAY_MS after its end the style is checked: if it is not the wanted one the
	//sequence is worked out again and sent once more, then nothing. Abort at once if the user moves the selector or the
	//engine stops. Afterwards nothing is sent until the next request: the user is free to change the drive style.
	static uint8_t dnaRestorePosition(uint8_t mode){ //0x46C code -> position on the selector, 0xFF unknown
		switch(mode){
			case 0x04: return 0; //A
			case 0x00: return 1; //N
			case 0x02: return 2; //D
			case 0x0C: return 3; //R
			default:   return 0xFF;
		}
	}

	//works out the presses from the current style to dnaRestoreTarget. 0 = nothing to do (or style unknown)
	static uint8_t dnaRestorePlan(void){
		uint8_t cur=dnaRestorePosition(currentDNAmode);
		if(cur==0xFF || cur==dnaRestoreTarget) return 0;
		dnaRestoreLong=0;
		if(dnaRestoreTarget>cur){
			dnaRestoreDir=0x40; //up
			if(dnaRestoreTarget==3){ //R: the last step D->R is the long press
				dnaRestoreShortLeft=(uint8_t)(2-cur);
				dnaRestoreLong=1;
			}else{
				dnaRestoreShortLeft=(uint8_t)(dnaRestoreTarget-cur);
			}
		}else{
			dnaRestoreDir=0x20; //down
			dnaRestoreShortLeft=(uint8_t)(cur-dnaRestoreTarget);
		}
		dnaRestoreSkipNext=0;
		dnaRestoreLongStart=0;
		return 1;
	}

	static void dnaRestoreStop(uint8_t reason){
		dnaRestoreState=0;
	}
	/* Remember manual selector activity after this engine-start edge. This closes
	 * the race where the driver changes DNA after C1's evaluation but before the
	 * UART request reaches BH. A manual choice blocks the whole ignition-cycle
	 * restore. A press made while stopped is retained through the next engine start. */
	static uint8_t dnaRestoreEngineWasRunning;
	static uint8_t dnaRestoreManualInputAfterStart;
	static uint8_t dnaRestoreManualPressActive;
	static void dnaRestoreTrackEngine(void){
		if(currentRpmSpeed<=DNA_RESTORE_ENGINE_RUNNING_RPM){
			if(dnaRestoreEngineWasRunning) dnaRestoreManualInputAfterStart=0u;
			dnaRestoreEngineWasRunning=0u;
		}else if(!dnaRestoreEngineWasRunning){
			dnaRestoreEngineWasRunning=1u;
		}
	}

	//called on every 0x256 received (main loop): sends the presses right after the frame of the selector
	void dnaRestoreOn256(void){
		dnaRestoreTrackEngine();
		if(!(rx_msg_data[0] & 0x60u)) dnaRestoreManualPressActive=0u;
		else if(!dnaRestoreManualPressActive){
			dnaRestoreManualPressActive=1u;
			dnaRestoreManualInputAfterStart=1u;
			uint8_t notice[2]={C1_Bh_BusID,C1BHcmdDnaManualSelection};
			(void)tryAddToUARTSendQueue(notice,2u);
		}
		if(dnaRestoreState!=1) return;
		if(rx_msg_data[0] & 0x60){ dnaRestoreStop(3); return; } //a real press of the user (our own frames are never received)
		if(currentRpmSpeed<=DNA_RESTORE_ENGINE_RUNNING_RPM){ dnaRestoreStop(4); return; }
		uint8_t press=0;
		if(dnaRestoreSkipNext){
			dnaRestoreSkipNext=0; //the selector's "not pressed" goes through
		}else if(dnaRestoreShortLeft>0){
			dnaRestoreShortLeft--;
			press=dnaRestoreDir;
			dnaRestoreSkipNext=1;
		}else if(dnaRestoreLong){
			if(dnaRestoreLongStart==0) dnaRestoreLongStart=currentTime;
			if(currentTime-dnaRestoreLongStart<DNA_RESTORE_LONG_PRESS_MS) press=0x40;
			else dnaRestoreLong=0; //long press over: the next frames of the selector carry the release
		}
		if(press){
			uint8_t d[8];
			memcpy(d, rx_msg_data, 8);
			d[0]=(uint8_t)((d[0] & ~0x60) | press);
			can_tx((CAN_TxHeaderTypeDef *)&rx_msg_header, d); //the frame of the selector again, pressed
			//can_process(); //sent as soon as possible after the original
		}
		if(dnaRestoreShortLeft==0 && !dnaRestoreLong && !dnaRestoreSkipNext){ //sequence complete (after a short press: when its release went through)
			dnaRestoreState=2;
			dnaRestoreEndTime=currentTime;
		}
	}

	//Called from BHperiodicCheck() on every main loop iteration. It does not send any frame by itself: it starts a
	//sequence (state 1), aborts it if the engine stops, and checks the result once a sequence is over (state 2).
	//The presses themselves are sent by dnaRestoreOn256(), right after each 0x256 received from the selector.
	//dnaRestoreState: 0=idle, nothing is sent | 1=sequence running (dnaRestoreOn256 sends the presses) |
	//                 2=sequence over, waiting DNA_RESTORE_CHECK_DELAY_MS before checking the drive style
	static void dnaRestoreProcess(void){
		dnaRestoreTrackEngine();
		//--- 1) a new request arrived from C1 (stored by the uart interrupt as a letter: 'A','N','D' or 'R')
		if(dnaRestoreRequest){ //new request from C1
			char letter=(char)dnaRestoreRequest;
			dnaRestoreRequest=0; //consumed: one request starts at most one restore (first sequence + one repetition)
			dnaRestoreTarget=(letter=='A')?0:(letter=='N')?1:(letter=='D')?2:3; //position on the selector, A=0 ... R=3
			dnaRestorePass=1; //first sequence
			dnaRestoreState=0; //a new request replaces any restore still in progress
			//start only if everything needed is really there:
			// - engine running (the drive style is set only with the engine on, as decided by C1);
			// - 0x46C received recently: the current drive style is known, so the sequence can be worked out;
			// - 0x256 received recently: the DNA selector is on this bus and keeps sending its frames, which are the
			//   trigger for every press (on cars with a different selector, as Ambrotek's, it never arrives).
			//The times are 0 until the first message of that kind has been received since power on.
			if(currentRpmSpeed<=DNA_RESTORE_ENGINE_RUNNING_RPM || dnaRestoreLast46CTime==0 || dnaRestoreLast256Time==0 ||
			   currentTime-dnaRestoreLast46CTime>DNA_RESTORE_INPUT_MAX_AGE_MS || currentTime-dnaRestoreLast256Time>DNA_RESTORE_INPUT_MAX_AGE_MS){
				dnaRestoreStop(1); //selector not on this bus, style unknown or engine off: nothing is sent
			}else if(dnaRestoreManualInputAfterStart){
				dnaRestoreStop(3); //a manual selector press after engine start has priority for this ignition cycle
			}else if(dnaRestorePlan()){ //works out direction, number of short presses and the final long press (D->R)
				dnaRestoreState=1; //from now on dnaRestoreOn256() sends the presses
			}else{
				dnaRestoreStop(2); //already in the wanted drive style (or current style unknown): nothing to do
			}
		}

		//--- 2) the engine stopped while restoring: abort at once, no further frame is sent
		if(dnaRestoreState!=0 && currentRpmSpeed<=DNA_RESTORE_ENGINE_RUNNING_RPM){
			dnaRestoreStop(4);
			return;
		}

		//--- 3) a sequence is over: after DNA_RESTORE_CHECK_DELAY_MS (time for the BCM to update 0x46C) check the style
		if(dnaRestoreState==2 && currentTime-dnaRestoreEndTime>=DNA_RESTORE_CHECK_DELAY_MS){
			if(dnaRestorePosition(currentDNAmode)==dnaRestoreTarget){
				dnaRestoreStop(5); //wanted drive style reached: done
			}else if(dnaRestorePass==1){
				//first sequence did not reach it: work out the presses again, from the drive style the car is in now
				dnaRestorePass=2;
				if(dnaRestorePlan()) dnaRestoreState=1; //sent once more; after it the check only logs the result (5 or 6), nothing more is sent
				else dnaRestoreStop(2);
			}else{
				dnaRestoreStop(6); //second sequence over too: give up, nothing more is sent until the next request
			}
		}
	}
	//DNA memory 18/10/2026 - END

	// HU text refresh on long-OK exit. Skip when HU is already muted to avoid audible unmute.
	// 0=idle, 1=wait for first toggle feedback, 2=wait for restored state,
	// 3=wait for display clear. Never retry a toggle without fresh feedback.
	static uint8_t huRestorePhase = 0;
	static uint8_t huRestoreInitialMuted = 0;
	static uint32_t huRestoreSentAt = 0;
	static uint32_t huRestoreRequestAt = 0;
	static void huRestoreToggle(void) {
		memcpy(centerConsoleTxMsgData, (uint8_t*)centerConsoleRxMsgData, 6);
		centerConsoleTxMsgData[2] = 0xE0;
		can_tx(&centerConsoleTxMsgHeader, centerConsoleTxMsgData);
		huRestoreSentAt = currentTime;
	}
	static void huRestoreProcess(void) {
		if (bhHuTextRestoreRequest) {
			bhHuTextRestoreRequest = 0;
			if (huRestorePhase == 0 && audioSystemMuted == 0 && currentGear != 0x0E &&
			    audioSystemReverseMuted == 0 && audioSystemMuteFeedbackTime != 0 &&
			    (uint32_t)(currentTime - audioSystemMuteFeedbackTime) < 1500u) {
				huRestoreInitialMuted = audioSystemMuted;
				huRestoreRequestAt = currentTime;
				huRestorePhase = 3;
			}
		}
		if (huRestorePhase == 0) return;
		// No audio toggle while reversing or under reverse-mute control.
		if (currentGear == 0x0E || audioSystemReverseMuted != 0) {
			huRestorePhase = 0;
			return;
		}
		if (huRestorePhase == 3) {
			if ((uint32_t)(currentTime - huRestoreRequestAt) >= 400u &&
			    audioSystemMuted == huRestoreInitialMuted &&
			    (uint32_t)(currentTime - audioSystemMuteFeedbackTime) < 1500u) {
				huRestoreToggle();
				huRestorePhase = 1;
			} else if ((uint32_t)(currentTime - huRestoreRequestAt) > 2000u) {
				huRestorePhase = 0;
			}
			return;
		}
		if ((uint32_t)(currentTime - huRestoreSentAt) > 1500u) {
			// Stop rather than send an unconfirmed toggle. The original audio
			// state cannot be guaranteed if a HU command/feedback was lost.
			huRestorePhase = 0;
			return;
		}
		if ((uint32_t)(currentTime - huRestoreSentAt) < 120u ||
		    audioSystemMuteFeedbackTime <= huRestoreSentAt) return;
		if (huRestorePhase == 1 && audioSystemMuted != huRestoreInitialMuted) {
			huRestoreToggle();
			huRestorePhase = 2;
		} else if (huRestorePhase == 2 && audioSystemMuted == huRestoreInitialMuted) {
			huRestorePhase = 0;
		}
	}

	void BHperiodicCheck(){
		huRestoreProcess();
		dnaRestoreProcess();
		if(requestToSendOneFrame>0){ //if requested by a message received from master baccable
			// Send one display frame at a time; Stage D4 tests a 40 ms interval.
			if (currentTime-lastSentTelematic_display_info_msg_Time>BH_DASHBOARD_FRAME_INTERVAL_MS){
				lastSentTelematic_display_info_msg_Time=currentTime;
				// Capture only at the first frame; never mix two menu pages.
				if (paramsStringCharIndex == 0) {
					memcpy(dashboardTxSnapshot, dashboardPageStringArray, DASHBOARD_MESSAGE_MAX_LENGTH);
				}
				//prepare msg to send:
				//frame number is on byte 0 from bit 2 to 0 and byte1 from bit7 to 6
				telematic_display_info_msg_data[0]=(telematic_display_info_msg_data[0] & ~0x07) | ((telematic_display_info_field_frameNumber>>2) & 0x07);
				telematic_display_info_msg_data[1]=(telematic_display_info_msg_data[1] & ~0xC0) | ((telematic_display_info_field_frameNumber<<6) & 0xC0);

				//UTF text 1 is on byte 2 and byte 3
				telematic_display_info_msg_data[3]=dashboardTxSnapshot[paramsStringCharIndex];
				paramsStringCharIndex++; //prepare to send next char
				//UTF text 2 is on byte 4 (set to zero ) and byte 5
				telematic_display_info_msg_data[5]=dashboardTxSnapshot[paramsStringCharIndex];
				paramsStringCharIndex++; //prepare to send next char
				//UTF text 3 is on byte 6 (set to zero) and byte 7
				telematic_display_info_msg_data[7]=dashboardTxSnapshot[paramsStringCharIndex];
				paramsStringCharIndex++; //prepare to send next char
				//send it
				can_tx(&telematic_display_info_msg_header, telematic_display_info_msg_data); //transmit the packet

				telematic_display_info_field_frameNumber++; //prepare for next frame to send
				if( paramsStringCharIndex>=DASHBOARD_MESSAGE_MAX_LENGTH) { //if we sent the entire string
					paramsStringCharIndex=0; //prepare to send first char of the string
					telematic_display_info_field_frameNumber=0; //prepare to send first frame
					requestToSendOneFrame -= 1;
					onboardLed_blue_on();
				}
			}
		}

		//--- @Gaucho original code replaced with @netzmark code below
//		if(function_park_mirror){ //if function parkmirror is enabled
//
//			if(currentGear==0x0E){ //if reverse gear is selected
//				if(!restoreOperativeMirrorsPosition){ //if we are not returning to operative position
//					switch(turnIndicator){
//						case 0x02: //left arrow inserted
//							if(!leftParkMirrorPositionRequired && !rightParkMirrorPositionRequired) storeOperativeMirrorPosition=1;//store current mirror position, if mirror was not previously lowered
//							leftParkMirrorPositionRequired=1; //Enable sending command to move mirror
//							break;
//						case 0x01: //right arrow inserted
//							if(!leftParkMirrorPositionRequired && !rightParkMirrorPositionRequired) storeOperativeMirrorPosition=1;//store current mirror position, if mirror was not previously lowered
//							rightParkMirrorPositionRequired=1; //Enable sending command to move mirror
//							break;
//						default:
//					}
//				}
//			}else{
//				if(leftParkMirrorPositionRequired || rightParkMirrorPositionRequired){ //if mirrors are potentially not in operative position,
//					restoreOperativeMirrorsPosition=1; //request to restore mirrors to their original position
//					restoreOperativeMirrorsPositionRequestTime=currentTime;
//				}
//				leftParkMirrorPositionRequired=0; //stop sending message to set Park position for mirrors
//				rightParkMirrorPositionRequired=0;//stop sending message to set Park position for mirrors
//			}
//
//
//			//Prepare msg to send: set Operative position of the mirrors
//			parkMirrorMsgData[0]= leftMirrorHorizontalOperativePos;
//			parkMirrorMsgData[1]= leftMirrorVerticalOperativePos;
//			parkMirrorMsgData[2]= rightMirrorHorizontalOperativePos;
//			parkMirrorMsgData[3]= rightMirrorVerticalOperativePos;
//
//			//Prepare msg to send: if required, set park position of the mirrors
//			if(leftParkMirrorPositionRequired){
//				parkMirrorMsgData[0]= leftParkMirrorHorizontalPos;
//				parkMirrorMsgData[1]= leftParkMirrorVerticalPos;
//			}
//			if(rightParkMirrorPositionRequired){
//				parkMirrorMsgData[2]= rightParkMirrorHorizontalPos;
//				parkMirrorMsgData[3]= rightParkMirrorVerticalPos;
//			}
//
//
//			if(leftParkMirrorPositionRequired || rightParkMirrorPositionRequired || restoreOperativeMirrorsPosition){ //if required
//				if(!storeOperativeMirrorPosition){ //if operative position was stored
//					if(currentTime-lastParkMirrorMsgTime>900){ //each 1000msec send a packet
//						can_tx(&parkMirrorMsgHeader, parkMirrorMsgData); //send msg
//						lastParkMirrorMsgTime=currentTime;
//					}
//				}
//				if(restoreOperativeMirrorsPosition){
//					if(currentTime-restoreOperativeMirrorsPositionRequestTime>15000){ //after 15 seconds
//						restoreOperativeMirrorsPosition=0;
//					}
//				}
//			}
//		}

		//--- @netzmark park mirror returning delay added - begin//
		if(function_park_mirror){ //if function parkmirror is enabled

			if(currentGear==0x0E){ //if reverse gear is selected 0x0E (masked 7E in fact)
				exitReverseTime = 0; //Reset hysteresis timer while in Reverse

				if(!restoreOperativeMirrorsPosition){ //if we are not returning to operative position
					switch(turnIndicator){
						case 0x02: //left arrow inserted
							if(!leftParkMirrorPositionRequired && !rightParkMirrorPositionRequired) storeOperativeMirrorPosition=1;//store current mirror position, if mirror was not previously lowered
							leftParkMirrorPositionRequired=1; //Enable sending command to move mirror
							break;
						case 0x01: //right arrow inserted
							if(!leftParkMirrorPositionRequired && !rightParkMirrorPositionRequired) storeOperativeMirrorPosition=1;//store current mirror position, if mirror was not previously lowered
							rightParkMirrorPositionRequired=1; //Enable sending command to move mirror
							break;
						default:
					}
				}
			}else if(currentGear == 0x0D || currentGear == 0x00){	//if Parking or Neutral (0x00) is selected we raise the mirrors immediately
																	//protection against leaving the mirrors lowered in case of ignition OFF within added delay time
																	//tested and working reliably on automatic gear box only
																 		//maybe it is not good for manual gear box where maybe N is always set during transition from R>D
																		//in this case remove this else if function
				exitReverseTime = 0; //Reset timer for P/N

				if(leftParkMirrorPositionRequired || rightParkMirrorPositionRequired){ //if mirrors are potentially not in operative position,
					restoreOperativeMirrorsPosition=1; //request to restore mirrors to their original position
					restoreOperativeMirrorsPositionRequestTime=currentTime;
				}
				leftParkMirrorPositionRequired=0; //stop sending message to set Park position for mirrors
				rightParkMirrorPositionRequired=0;//stop sending message to set Park position for mirrors
			}else{ //if Drive (D) or any other gear is selected
				//If mirrors are lowered and timer is not armed yet, capture the exit time
				if((leftParkMirrorPositionRequired || rightParkMirrorPositionRequired) && exitReverseTime == 0){
					exitReverseTime = currentTime; //Capture the moment we switched to D
				}

				//Trigger return only after 10 seconds of constant driving forward
				if(exitReverseTime != 0 && (currentTime - exitReverseTime > 10000)){ //HERE IT IS WHAT WE WANTED TO ADD - 10 seconds hysteresis delay
					if(leftParkMirrorPositionRequired || rightParkMirrorPositionRequired){ //if mirrors are potentially not in operative position,
						restoreOperativeMirrorsPosition=1; //request to restore mirrors to their original position
						restoreOperativeMirrorsPositionRequestTime=currentTime;
					}
					leftParkMirrorPositionRequired=0; //stop sending message to set Park position for mirrors
					rightParkMirrorPositionRequired=0;//stop sending message to set Park position for mirrors
					exitReverseTime = 0; //Reset timer after execution
				}
			}

			//Prepare msg to send: set Operative position of the mirrors
			parkMirrorMsgData[0]= leftMirrorHorizontalOperativePos;
			parkMirrorMsgData[1]= leftMirrorVerticalOperativePos;
			parkMirrorMsgData[2]= rightMirrorHorizontalOperativePos;
			parkMirrorMsgData[3]= rightMirrorVerticalOperativePos;

			//Prepare msg to send: if required, set park position of the mirrors
			if(leftParkMirrorPositionRequired){
				parkMirrorMsgData[0]= leftParkMirrorHorizontalPos;
				parkMirrorMsgData[1]= leftParkMirrorVerticalPos;
			}
			if(rightParkMirrorPositionRequired){
				parkMirrorMsgData[2]= rightParkMirrorHorizontalPos;
				parkMirrorMsgData[3]= rightParkMirrorVerticalPos;
			}


			if(leftParkMirrorPositionRequired || rightParkMirrorPositionRequired || restoreOperativeMirrorsPosition){ //if required
				if(!storeOperativeMirrorPosition){ //if operative position was stored
					if(currentTime-lastParkMirrorMsgTime>900){ //each 1000msec send a packet
						can_tx(&parkMirrorMsgHeader, parkMirrorMsgData); //send msg
						lastParkMirrorMsgTime=currentTime;
					}
				}
				if(restoreOperativeMirrorsPosition){
					if(currentTime-restoreOperativeMirrorsPositionRequestTime>15000){ //after 15 seconds
						restoreOperativeMirrorsPosition=0;
					}
				}
			}
		}
		//--- @netzmark park mirror returning delay added - end//


		// --- REVERSE AUDIO MUTE SYSTEM ---
		if(!reverseAutoMuteEnabled && audioSystemReverseMuted != 0){
            // Release only a mute introduced by BACCAble; never toggle a manual mute.
            if(audioSystemReverseMuted == 1 && audioSystemMuted == 1){
                memcpy(centerConsoleTxMsgData, (uint8_t*)centerConsoleRxMsgData, 6);
                centerConsoleTxMsgData[2] = 0xE0;
                can_tx(&centerConsoleTxMsgHeader, centerConsoleTxMsgData);
            }
            audioSystemReverseMuted = 0;
            exitReverseAudioTime = 0;
        }
		if(reverseAutoMuteEnabled){

			// 1. STATE: REVERSE GEAR (0x0E) - Activate mute or log manual bypass
			if(currentGear == 0x0E) {
				exitReverseAudioTime = 0; // Reset timer while in Reverse

				if(audioSystemReverseMuted == 0) {
					if(audioSystemMuted == 0) {
						// Music is playing -> copy from pure radio buffer and inject Mute button
						memcpy(centerConsoleTxMsgData, (uint8_t*)centerConsoleRxMsgData, 6);
						centerConsoleTxMsgData[2] = 0xE0; // Press Mute
						can_tx(&centerConsoleTxMsgHeader, centerConsoleTxMsgData);

						audioSystemReverseMuted = 1; // Locked by our automation (Safe rygiel)
					} else {
						audioSystemReverseMuted = 2; // User already muted the radio, bypass automation permanently
					}
				}
			}
			// 2. STATE: PARKING (0x0D) OR NEUTRAL (0x00) - Immediate restoration (Only for state 1)
			else if(currentGear == 0x0D || currentGear == 0x00) {
				exitReverseAudioTime = 0;

				if(audioSystemReverseMuted == 1) { // Process ONLY if muted by us
					if(audioSystemMuted == 1) { // Double check if radio is still muted
						memcpy(centerConsoleTxMsgData, (uint8_t*)centerConsoleRxMsgData, 6);
						centerConsoleTxMsgData[2] = 0xE0; // Press Mute again to toggle PLAY
						can_tx(&centerConsoleTxMsgHeader, centerConsoleTxMsgData);
					}
				}
				audioSystemReverseMuted = 0; // Safe reset to idle for both state 1 and 2
			}
			// 3. STATE: DRIVE (D) OR ANY OTHER GEAR - Safe delay only for automation
			else {
				// [NEW SAFE FIX]: If user manually unmuted on R, clear the state instantly on D entry (no delay needed)
				if(audioSystemReverseMuted == 1 && audioSystemMuted == 0) {
					audioSystemReverseMuted = 0;
					exitReverseAudioTime = 0;
				}

				// If bypass state 2 is active, clear it instantly when moving forward (no delay needed)
				if(audioSystemReverseMuted == 2) {
					audioSystemReverseMuted = 0;
					exitReverseAudioTime = 0;
				}

				// If muted by us (state 1) and timer is not armed yet, start countdown
				if(audioSystemReverseMuted == 1 && exitReverseAudioTime == 0) {
					exitReverseAudioTime = currentTime;
				}

				// Trigger return only after 3 seconds of constant driving forward
				if(exitReverseAudioTime != 0 && (currentTime - exitReverseAudioTime > 3000)) {
					if(audioSystemReverseMuted == 1) {
						if(audioSystemMuted == 1) { // Restore only if still muted
							memcpy(centerConsoleTxMsgData, (uint8_t*)centerConsoleRxMsgData, 6);
							centerConsoleTxMsgData[2] = 0xE0; // Toggle back to PLAY
							can_tx(&centerConsoleTxMsgHeader, centerConsoleTxMsgData);
						}
					}
					audioSystemReverseMuted = 0; // Reset state machine to idle
					exitReverseAudioTime = 0; // Reset timer after execution
				}
			}
		}
		//@netzmark MUTE_ON_REVERSE code - end
	}

	uint8_t saveOnFlashBH(){
		//last page to store on flash is 0x0801 F800 (we can store 2 bytes each time)
		// and we shall erase entire page before write. one page size is FLASH_PAGE_SIZE (2048 bytes in st32F072)
		HAL_FLASH_Unlock(); //unlock flash

		//erase flash
		FLASH_EraseInitTypeDef eraseInitStruct;
		uint32_t pageError=0;
		eraseInitStruct.TypeErase= FLASH_TYPEERASE_PAGES;
		eraseInitStruct.PageAddress=LAST_PAGE_ADDRESS; //last page address begin
		eraseInitStruct.NbPages=1;
		if(HAL_FLASHEx_Erase(&eraseInitStruct,&pageError)!=HAL_OK){ //error during erase
			HAL_FLASH_Lock();
			onboardLed_red_blink(8);
			return 254; //error
		}

		//it seems that stm32F072 supports only writing 2byte words
		//write parameter
		uint8_t paramsNumber=9;
		uint16_t params[40] = {
				leftParkMirrorHorizontalPos,
				leftParkMirrorVerticalPos,
				rightParkMirrorHorizontalPos,
				rightParkMirrorVerticalPos,
				parkMirrorOperativePositionNotStored,
				leftMirrorHorizontalOperativePos,
				leftMirrorVerticalOperativePos,
				rightMirrorHorizontalOperativePos,
				rightMirrorVerticalOperativePos,

		};

		for (uint8_t i = 0; i < paramsNumber; i++) {
		    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, LAST_PAGE_ADDRESS + (i * 4), params[i]) != HAL_OK) {
		        HAL_FLASH_Lock();
		        onboardLed_red_blink(9);
		        return 255;
		    }
		}

		//lock the flash
		HAL_FLASH_Lock();
		return 0;

	}

	uint16_t readFromFlashBH(uint8_t paramId){
		if(paramId<1) return 0;
		uint16_t tmpParam=*(volatile uint16_t*)(LAST_PAGE_ADDRESS+((paramId-1)*4));
		return tmpParam;
	}

#endif
