/*
 * processingMessage0x000002FA.c
 *
 *  Created on: May 3, 2025
 *      Author: GauchoHP
 */

#include "debug.h"
#include "processingMessage0x000002FA.h"
#include "processingExtendedMessage.h"

/* Menu restructuring: legacy IDs remain unchanged; 16=Features, 17=Exit. */
#if defined(C1baccable)
static uint8_t baccableFeaturesActive = 0;
static uint8_t baccableMenuAllowed(uint8_t id) {
    switch(id) {
		/* @Shadin: Refer to the existing menu IDs instead of repeating numeric literals. */
        case MENU_READ_FAULTS: return function_read_faults_enabled != 0;
        case MENU_CLEAR_FAULTS: return function_clear_faults_enabled != 0;
        case MENU_DYNO: return function_dyno_mode_master_enabled != 0;
        case MENU_FRONT_BRAKE: return function_front_brake_forcer_master != 0;
        case MENU_4WD: return function_4wd_disabler_enabled != 0;
        case MENU_HAS: return HAS_function_enabled != 0;
        case MENU_EXHAUST: return QV_exhaust_flap_function_enabled != 0;
        default: return 1;
    }
}
static void baccableMenuStep(int direction) {
    const uint8_t *order = baccableFeaturesActive ? baccableFeatureOrder : baccableMainOrder;
    const uint8_t count = baccableFeaturesActive ? baccableFeatureMenuCount : baccableMainMenuCount;
    uint8_t position = 0;
    for(uint8_t i=0;i<count;i++) if(order[i]==main_dashboardPageIndex) { position=i; break; }
    for(uint8_t tries=0;tries<count;tries++) {
        position=(uint8_t)((position + count + direction)%count);
        if(baccableMenuAllowed(order[position])) { main_dashboardPageIndex=order[position]; break; }
    }
}
#endif

	/* @netzmark to speed-up clicks reaction */
#if defined(C1baccable)

    extern uint32_t lastDisplayUpdateTime;
    extern uint16_t dynamicInterval;
    extern uint8_t group_step;

	void resetDashboardTimers(void) {
	    // 1. We reserve quiet on Serial after click
	    lastDisplayUpdateTime = currentTime - 500;

	    // 2. Set default interval to avoid 0 - the same like in C1baccable.c
	    dynamicInterval = 200;

	    // 3. Set UDS request after 10ms (fluent new page start)
	    last_sent_uds_parameter_request_Time = currentTime - dynamicInterval;

	    // 4. Clean UDS cache
	    for(int i = 0; i < MAX_UDS_PARAMS; i++) {
	        uds_values_cache[i] = NAN;
	    }

	    // Clean UART to BH data
	    dashboardParamCouple[0] = NAN;
	    dashboardParamCouple[1] = NAN;

	    group_step = 0;
	}

#endif
    /* @netzmark - end */

void processingMessage0x000002FA(){
	// Button is pressed on left area of the wheel
	// These Buttons are detected only if the main panel of the car is on.

	#if defined(C2baccable) || defined(C1baccable)
		if(HAS_buttonPressRequested){ //Has button press was requested
			HAS_buttonPressRequested--;
			if(rx_msg_data[0]==0x10){ //if no button was pressed on cruise control pad
				rx_msg_data[1] = rx_msg_data[1] | 0x10; //simulate HAS button presses
				rx_msg_data[2] = calculateCRC(rx_msg_data,rx_msg_header.DLC); //CRC
				can_tx((CAN_TxHeaderTypeDef *)&rx_msg_header, rx_msg_data); //send message to simulate RES button press

			}
		}
	#endif

	#if defined(C1baccable)
	
	  	//function ACC Virtual Pad
		/* @Shadin: 0x2FA ACC frame must fit the fixed 3-byte ACC_msg_data buffer. */
		if(function_acc_virtual_pad_enabled==1 && rx_msg_header.DLC==sizeof ACC_msg_data){
			switch (rx_msg_data[0]){
				case 0x12: //CC on
					newWheelPressedButtonID=0x12;
					memcpy(ACC_msg_data, &rx_msg_data, rx_msg_header.DLC);
					ACC_msg_data[0] = 0x11; //ACC On
					ACC_msg_data[1] = (ACC_msg_data[1] & 0xF0) | (((ACC_msg_data[1] & 0x0F) + 1) % 16); //increase the counter
					ACC_msg_data[2] = calculateCRC(ACC_msg_data,rx_msg_header.DLC); //update checksum
					can_tx(&ACC_msg_header, ACC_msg_data); //send msg
					onboardLed_blue_on();
					break;
				case 0x90: //RES pressed
					if (newWheelPressedButtonID==0x10 && ACC_engaged) ACC_WAS_ENGAGED_WHEN_RES_WAS_PRESSED=1; //if begin to press button RES and ACC is Engaged, set ACC_WAS_ENGAGED_WHEN_RES_WAS_PRESSED
					newWheelPressedButtonID=0x90; //store the new RES button status (pressed)
					if (ACC_engaged && ACC_WAS_ENGAGED_WHEN_RES_WAS_PRESSED){
						//simulate the distance button press
						memcpy(ACC_msg_data, &rx_msg_data, rx_msg_header.DLC);
						ACC_msg_data[0] = 0x50; //ACC distance change
						ACC_msg_data[1] = (ACC_msg_data[1] & 0xF0) | (((ACC_msg_data[1] & 0x0F) + 1) % 16); //increase the counter
						ACC_msg_data[2] = calculateCRC(ACC_msg_data,rx_msg_header.DLC); //update checksum
						can_tx(&ACC_msg_header, ACC_msg_data); //send msg
						onboardLed_blue_on();
					}
					break;
				case 0x10: //button released
					newWheelPressedButtonID=0x10; //button released (I use another variable to distinguish from the one used in show params function
					ACC_WAS_ENGAGED_WHEN_RES_WAS_PRESSED=0;
					break;
				default:
			}
		}

			//Gaucho ori code
//		if(function_acc_autostart){
//			if(ACC_engaged){
//				if(carSteadyCounter==200 && brakeIntervention_ACC_ESC_ASR){ //if car is steady and brake is pressed by ACC
//					if(rx_msg_data[0]==0x10){ //if no button was pressed on cruise control pad
//						if (currentTime-lastSentAutostartMsg>500){
//							rx_msg_data[0] = 0x90; //Res button press
//
//							if(function_acc_autostart==2){
//								rx_msg_data[0] = 0x08; //ACC gently up button press
//							}
//							rx_msg_data[1] = (rx_msg_data[1] & 0xF0) | (((rx_msg_data[1] & 0x0F) + 1) % 16); //increase the counter
//							rx_msg_data[2]=calculateCRC(rx_msg_data,rx_msg_header.DLC); //update checksum
//
//							can_tx((CAN_TxHeaderTypeDef *)&rx_msg_header, rx_msg_data); //send message to simulate RES button press
//							rx_msg_data[0]=0x10; //restore value 10 to avoid unwanted behaviours with subsequent pieces of code
//							//increase a counter
//							AutostartMsgCounter++;
//							if (AutostartMsgCounter>= 5){
//								AutostartMsgCounter=0;
//								lastSentAutostartMsg=currentTime;
//							}
//						}
//					}
//				}
//			}
//		}

		//@netzmark corrected code
		if(function_acc_autostart){
		    if(ACC_engaged){

		        if(carSteadyCounter == 200 && brakeIntervention_ACC_ESC_ASR == 1){

		            if (autostartActiveBurst == 0) {
		                if (currentTime - lastSentAutostartMsg > 450) { //real 450ms pause
		                    autostartActiveBurst = 1;
		                }
		            }

		            // 5 frames repetitions synchronised with original 0x10
		            if (autostartActiveBurst == 1) {
		                if(rx_msg_data[0] == 0x10){

		                    rx_msg_data[0] = 0x90; // RES button press
		                    if(function_acc_autostart == 2){
		                        rx_msg_data[0] = 0x08; // ACC gently up button press
		                    }

		                    rx_msg_data[1] = (rx_msg_data[1] & 0xF0) | (((rx_msg_data[1] & 0x0F) + 1) % 16);
		                    rx_msg_data[2] = calculateCRC(rx_msg_data, rx_msg_header.DLC);

		                    can_tx((CAN_TxHeaderTypeDef *)&rx_msg_header, rx_msg_data);

		                    rx_msg_data[0] = 0x10;

		                    AutostartMsgCounter++;

		                    if (AutostartMsgCounter >= 5) {
		                        AutostartMsgCounter = 0;
		                        lastSentAutostartMsg = currentTime;
		                        autostartActiveBurst = 0;
		                    }
		                }
		            }
		        }

		        // Just for any case
		        if(carSteadyCounter < 200 || brakeIntervention_ACC_ESC_ASR == 0){
		            AutostartMsgCounter = 0;
		            autostartActiveBurst = 0;
		        }

		    }
		}



		if(cruiseControlDisabled && ACC_Disabled){ //if we are allowed to press buttons, use them in baccable menu
			switch(rx_msg_data[0]){
				case 0x18://if cruise control speed reduction button was pressed, user wants to see next page
					if(wheelPressedButtonID==0x10 && baccableDashboardMenuVisible){ //if button released, use pressed button
						wheelPressedButtonID=0x18; //avoid to return here
						resetDashboardTimers(); // @netzmark to speed-up clicks reaction
						if(commandsMenuEnabled){
							switch(dashboard_menu_indent_level){
								case 0: //main menu
                                    baccableMenuStep(1);
                                    sendMainDashboardPageToSlaveBaccable();
                                    break;

								case 1:
									if(main_dashboardPageIndex==1){ //we are in show params submenu
										dashboardPageIndex += 1; //set next page
										if(function_is_diesel_enabled==1){
											if(dashboardPageIndex>=total_pages_in_dashboard_menu_diesel)  dashboardPageIndex=0; // make a rotative menu
										}else{
											if(dashboardPageIndex>=total_pages_in_dashboard_menu_gasoline)  dashboardPageIndex=0; // make a rotative menu
										}

										dashboardPageIndex=getNextVisibleParam(dashboardPageIndex);
										//onboardLed_blue_on();
										dashboardParamCouple[0]=NAN;//zeroize params
										dashboardParamCouple[1]=NAN;//zeroize params

										sendDashboardPageToSlaveBaccable(); //send dashboard page to BH
									}
									if(main_dashboardPageIndex==9){ //we are in setup menu
										setup_dashboardPageIndex+=1;//set next page
										//if(setup_dashboardPageIndex==2) setup_dashboardPageIndex++; //future growth

										//if(setup_dashboardPageIndex==8) setup_dashboardPageIndex++; //future growth

										if(setup_dashboardPageIndex>=total_pages_in_setup_dashboard_menu)  setup_dashboardPageIndex=0; // make a rotative menu
										#if !defined(LED_STRIP_CONTROLLER_ENABLED)
											/* @Shadin: Skip the absent LED setup item, retaining the original menu indices. */
											if(setup_dashboardPageIndex==3) setup_dashboardPageIndex=4;
										#endif

										//onboardLed_blue_on();
										sendSetupDashboardPageToSlaveBaccable(); //send
									}
									if(main_dashboardPageIndex==10){ //we are in params setup menu
										params_setup_dashboardPageIndex+=1;//set next page
										if(params_setup_dashboardPageIndex>total_pages_in_params_setup_dashboard_menu)  params_setup_dashboardPageIndex=0; // make a rotative menu
										sendParamsSetupDashboardPageToSlaveBaccable(); //send
									}
									/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
if(main_dashboardPageIndex==2 && dashboard_menu_indent_level==1 && faultsStateMachine==3) {
    stageC1cPageNext();
    sendMainDashboardPageToSlaveBaccable();
}
#else
if(main_dashboardPageIndex==2 && faultsStateMachine==3 && faultsDTCcount>0){ //readFaults 12/08/2026
										faultsDTCsubmenuIndex++;
										if(faultsDTCsubmenuIndex>=faultsDTCcount) faultsDTCsubmenuIndex=0;
										sendMainDashboardPageToSlaveBaccable();
									}
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
									break;
								default:
									break; //unexpected
							}
						}
					}
					break;
				case 0x20://if cruise control speed strong reduction button was pressed, user wants to jump 10 pages forward
						if(wheelPressedButtonID==0x18 && baccableDashboardMenuVisible){ //if button released, use pressed button
							wheelPressedButtonID=0x20; //avoid to return here
							resetDashboardTimers(); // @netzmark to speed-up clicks reaction
							if(commandsMenuEnabled){
								switch(dashboard_menu_indent_level){
/*									case 0: //main menu
										main_dashboardPageIndex+= 1; //set next page
										if(function_read_faults_enabled==0){
											if(main_dashboardPageIndex==2) main_dashboardPageIndex++;
										}
										if(function_clear_faults_enabled==0){
											if(main_dashboardPageIndex==3) main_dashboardPageIndex++;
										}

										if(function_dyno_mode_master_enabled==0){
											if(main_dashboardPageIndex==5) main_dashboardPageIndex++;
										}

										if(function_esc_tc_customizator_enabled==0){
											if(main_dashboardPageIndex==6) main_dashboardPageIndex++;
										}

										if(function_front_brake_forcer_master==0){
											if(main_dashboardPageIndex==7) main_dashboardPageIndex++;
										}

										if(function_4wd_disabler_enabled==0){
											if(main_dashboardPageIndex==8) main_dashboardPageIndex++;
										}

										if(HAS_function_enabled==0){
											if(main_dashboardPageIndex==11) main_dashboardPageIndex++;
										}

										if(QV_exhaust_flap_function_enabled==0){
											if(main_dashboardPageIndex==12) main_dashboardPageIndex++;
										}

										if(main_dashboardPageIndex>=dashboard_main_menu_array_len)  main_dashboardPageIndex=0; // make a rotative menu
										//onboardLed_blue_on();
										sendMainDashboardPageToSlaveBaccable();//send dashboard page to BH
										break;
*/   
									case 1:
										if(main_dashboardPageIndex==1){ //we are in show params submenu
											dashboardPageIndex += 10; //set 10 pages forward (+1 in gentle command)
											if(function_is_diesel_enabled==1){
												if(dashboardPageIndex>=total_pages_in_dashboard_menu_diesel)  dashboardPageIndex=0; // make a rotative menu
											}else{
												if(dashboardPageIndex>=total_pages_in_dashboard_menu_gasoline)  dashboardPageIndex=0; // make a rotative menu
											}
											dashboardPageIndex=getNextVisibleParam(dashboardPageIndex);
												//onboardLed_blue_on();
											dashboardParamCouple[0]=NAN;//zeroize params
											dashboardParamCouple[1]=NAN;//zeroize params
											sendDashboardPageToSlaveBaccable(); //send dashboard page to BH
										}
										if(main_dashboardPageIndex==9){ //we are in setup menu
											setup_dashboardPageIndex+=10;//set next page
											//if(setup_dashboardPageIndex==2) setup_dashboardPageIndex++; //future growth
											//if(setup_dashboardPageIndex==8) setup_dashboardPageIndex++; //future growth
											if(setup_dashboardPageIndex>=total_pages_in_setup_dashboard_menu)  setup_dashboardPageIndex=0; // make a rotative menu
										#if !defined(LED_STRIP_CONTROLLER_ENABLED)
											/* @Shadin: Skip the absent LED setup item, retaining the original menu indices. */
											if(setup_dashboardPageIndex==3) setup_dashboardPageIndex=4;
										#endif
											//onboardLed_blue_on();
											sendSetupDashboardPageToSlaveBaccable(); //send
										}
										if(main_dashboardPageIndex==10){ //we are in params setup menu
											params_setup_dashboardPageIndex+=10;//set next page
											if(params_setup_dashboardPageIndex>total_pages_in_params_setup_dashboard_menu)  params_setup_dashboardPageIndex=0; // make a rotative menu
											//onboardLed_blue_on();
											sendParamsSetupDashboardPageToSlaveBaccable(); //send
										}
										/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
if(main_dashboardPageIndex==2 && dashboard_menu_indent_level==1 && faultsStateMachine==3) {
    stageC1cPageNext();
    sendMainDashboardPageToSlaveBaccable();
}
#else
if(main_dashboardPageIndex==2 && faultsStateMachine==3 && faultsDTCcount>0){ //readFaults 12/08/2026
											faultsDTCsubmenuIndex++;
											if(faultsDTCsubmenuIndex>=faultsDTCcount) faultsDTCsubmenuIndex=0;
											sendMainDashboardPageToSlaveBaccable();
										}
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
										break;
									default:
										break; //unexpected
								}
							}
						}

						break;
				case 0x08: //if cruise control speed increase button was pressed, user wants to see previous page
					if(wheelPressedButtonID==0x10 && baccableDashboardMenuVisible){ //if button released, use pressed button
						wheelPressedButtonID=0x08; //avoid to enter again here
						resetDashboardTimers(); // @netzmark to speed-up clicks reaction
						if(commandsMenuEnabled){
							switch(dashboard_menu_indent_level){
								case 0: //main menu
                                    baccableMenuStep(-1);
                                    sendMainDashboardPageToSlaveBaccable();
                                    break;

								case 1:
									if(main_dashboardPageIndex==1){ //we are in show params submenu
										dashboardPageIndex -= 1; //set previous page
										if(function_is_diesel_enabled==1){
											if(dashboardPageIndex>=total_pages_in_dashboard_menu_diesel)  dashboardPageIndex=total_pages_in_dashboard_menu_diesel-1; // make a rotative menu
										}else{
											if(dashboardPageIndex>=total_pages_in_dashboard_menu_gasoline)  dashboardPageIndex=total_pages_in_dashboard_menu_gasoline-1; // make a rotative menu
										}
										dashboardPageIndex=getPreviousVisibleParam(dashboardPageIndex);
										//onboardLed_blue_on();
										dashboardParamCouple[0]=NAN;//zeroize params
										dashboardParamCouple[1]=NAN;//zeroize params
										sendDashboardPageToSlaveBaccable(); //send dashboard page to BH
									}
									if(main_dashboardPageIndex==9){ //we are in setup menu
										setup_dashboardPageIndex-=1;//set next page
										//if(setup_dashboardPageIndex==8) setup_dashboardPageIndex--; //future growth
										//if(setup_dashboardPageIndex==2) setup_dashboardPageIndex--; //future growth

										if(setup_dashboardPageIndex>=total_pages_in_setup_dashboard_menu)  setup_dashboardPageIndex=total_pages_in_setup_dashboard_menu-1; // make a rotative menu
										#if !defined(LED_STRIP_CONTROLLER_ENABLED)
											/* @Shadin: Skip the absent LED setup item when moving backwards. */
											if(setup_dashboardPageIndex==3) setup_dashboardPageIndex=2;
										#endif
										//onboardLed_blue_on();
										sendSetupDashboardPageToSlaveBaccable(); //send
									}
									if(main_dashboardPageIndex==10){ //we are in params setup menu
										params_setup_dashboardPageIndex-=1;//set next page
										if(params_setup_dashboardPageIndex>total_pages_in_params_setup_dashboard_menu)  params_setup_dashboardPageIndex=total_pages_in_params_setup_dashboard_menu; // make a rotative menu
										//onboardLed_blue_on();
										sendParamsSetupDashboardPageToSlaveBaccable(); //send
									}
									/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
if(main_dashboardPageIndex==2 && dashboard_menu_indent_level==1 && faultsStateMachine==3) {
    stageC1cPagePrev();
    sendMainDashboardPageToSlaveBaccable();
}
#else
if(main_dashboardPageIndex==2 && faultsStateMachine==3 && faultsDTCcount>0){ //readFaults 12/08/2026
										if(faultsDTCsubmenuIndex==0) faultsDTCsubmenuIndex=faultsDTCcount-1;
										else faultsDTCsubmenuIndex--;
										sendMainDashboardPageToSlaveBaccable();
									}
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
									break;
								default:
									break; //unexpected
							}
						}
					}
					break;
				case 0x00: //if cruise control speed strong increase button was pressed, user wants to jump 10 pages before
						if(wheelPressedButtonID==0x08 && baccableDashboardMenuVisible){
							wheelPressedButtonID=0x00; //avoid to return here
							resetDashboardTimers(); // @netzmark to speed-up clicks reaction
							if(commandsMenuEnabled){
								switch(dashboard_menu_indent_level){
/*									case 0: //main menu
										main_dashboardPageIndex-= 1; //set next page
										if(main_dashboardPageIndex>=dashboard_main_menu_array_len)  main_dashboardPageIndex=dashboard_main_menu_array_len-1; // make a rotative menu

										if(QV_exhaust_flap_function_enabled==0){
											if(main_dashboardPageIndex==12) main_dashboardPageIndex--;
										}

										if(HAS_function_enabled==0){
											if(main_dashboardPageIndex==11) main_dashboardPageIndex--;
										}

										if(function_4wd_disabler_enabled==0){
											if(main_dashboardPageIndex==8) main_dashboardPageIndex--;
										}
										if(function_front_brake_forcer_master==0){
											if(main_dashboardPageIndex==7) main_dashboardPageIndex--;
										}
										if(function_esc_tc_customizator_enabled==0){
											if(main_dashboardPageIndex==6) main_dashboardPageIndex--;
										}
										if(function_dyno_mode_master_enabled==0){
											if(main_dashboardPageIndex==5) main_dashboardPageIndex--;
										}

										if(main_dashboardPageIndex==4) main_dashboardPageIndex--;

										if(function_clear_faults_enabled==0){
											if(main_dashboardPageIndex==3) main_dashboardPageIndex--;
										}
										if(function_read_faults_enabled==0){
											if(main_dashboardPageIndex==2) main_dashboardPageIndex--;
										}
										//onboardLed_blue_on();
										sendMainDashboardPageToSlaveBaccable();//send dashboard page via usb
										break;
*/  
									case 1:
										if(main_dashboardPageIndex==1){ //we are in show params submenu
											dashboardPageIndex -= 10; //set 10 pages backward

											if(function_is_diesel_enabled==1){
												if(dashboardPageIndex>=total_pages_in_dashboard_menu_diesel)  dashboardPageIndex=0; // stay at zero.
											}else{
												if(dashboardPageIndex>=total_pages_in_dashboard_menu_gasoline)  dashboardPageIndex=0; // stay at zero.
											}
											dashboardPageIndex=getPreviousVisibleParam(dashboardPageIndex);


											//onboardLed_blue_on();
											dashboardParamCouple[0]=NAN;//zeroize params
											dashboardParamCouple[1]=NAN;//zeroize params
											sendDashboardPageToSlaveBaccable(); //send dashboard page to BH
										}
										if(main_dashboardPageIndex==9){ //we are in setup menu
											setup_dashboardPageIndex-=10;//set prev page
											//if(setup_dashboardPageIndex==8) setup_dashboardPageIndex--; //future growth
											//if(setup_dashboardPageIndex==2) setup_dashboardPageIndex--; //future growth
											if(setup_dashboardPageIndex>=total_pages_in_setup_dashboard_menu)  setup_dashboardPageIndex=0; // make a rotative menu
											#if !defined(LED_STRIP_CONTROLLER_ENABLED)
												/* @Shadin: Skip the absent LED setup item on fast reverse navigation. */
												if(setup_dashboardPageIndex==3) setup_dashboardPageIndex=2;
												#endif
											//onboardLed_blue_on();
											sendSetupDashboardPageToSlaveBaccable(); //send
										}
										if(main_dashboardPageIndex==10){ //we are in params setup menu
											params_setup_dashboardPageIndex-=10;//set prev page
											if(params_setup_dashboardPageIndex>total_pages_in_params_setup_dashboard_menu)  params_setup_dashboardPageIndex=0; // make a rotative menu
											//onboardLed_blue_on();
											sendParamsSetupDashboardPageToSlaveBaccable(); //send
										}
										/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
if(main_dashboardPageIndex==2 && dashboard_menu_indent_level==1 && faultsStateMachine==3) {
    stageC1cPagePrev();
    sendMainDashboardPageToSlaveBaccable();
}
#else
if(main_dashboardPageIndex==2 && faultsStateMachine==3 && faultsDTCcount>0){ //readFaults 12/08/2026
											if(faultsDTCsubmenuIndex==0) faultsDTCsubmenuIndex=faultsDTCcount-1;
											else faultsDTCsubmenuIndex--;
											sendMainDashboardPageToSlaveBaccable();
										}
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
										break;
									default:
										break; //unexpected
								}
							}
						}
						break;
				case 0x10: // button released
					if(wheelPressedButtonID==0x89 && baccableDashboardMenuVisible==1){ //we pressed RES for at least one instant, then we released before 2 seconds, therefore we want to enter inside dashboard menu (will work only if menu is visible)

						if(dashboard_menu_indent_level==0){
							switch(main_dashboardPageIndex){
                                case 16: // Enter Features submenu
                                    baccableFeaturesActive=1;
                                    main_dashboardPageIndex=17;
                                    if(!baccableMenuAllowed(main_dashboardPageIndex)) baccableMenuStep(1);
                                    sendMainDashboardPageToSlaveBaccable();
                                    break;
                                case 17: // Exit Features submenu
                                    baccableFeaturesActive=0;
                                    main_dashboardPageIndex=16;
                                    sendMainDashboardPageToSlaveBaccable();
                                    break;
								case 1: //show params
									dashboard_menu_indent_level++;
									dashboardParamCouple[0]=NAN;//zeroize params
									dashboardParamCouple[1]=NAN;//zeroize params
									sendDashboardPageToSlaveBaccable();
									break;
								case 2: // Read Faults: long OK only; short OK must not start diagnostics
									break;
								case 3: //clear faults
                                    dtcLogManualClearBegin(); // @Shadin: mark REQUESTED, not confirmed clear
									clearFaultsRequest=255;
									break;
								case 4: //Immo
									// nothing to do
									break;
								case 5: // toggle dyno status
									//send request thu serial line
									if(carSteadyCounter>=100){ //car is steady since at least 1 second
										uint8_t tmpArr1[2]={C2BusID,C2cmdtoggleDyno};
										addToUARTSendQueue(tmpArr1, 2);
									}else{
										printStopTheCar=2;//print message "stop the car"
									}
									break;
								case 6: //toggle ESC/TC
									//send request thu serial line
									uint8_t tmpArr2[2]={C2BusID,C2cmdtoggleEscTc};
									addToUARTSendQueue(tmpArr2, 2);
									break;
								case 7: //toggle front brake

									if(front_brake_forced>0){ //toggle front brake
										if(launch_assist_enabled==1){ //if assist is enabled (by default it is enabled)
											launch_assist_enabled=0; //disable assist
										}else{ //launch assist is not enabled, but brakes are forced
											//send serial message to C2 baccable, to RELEASE front brakes
											uint8_t tmpArr3[2]={C2BusID,C2cmdNormalFrontBrake};
											addToUARTSendQueue(tmpArr3, 2);
										}
									}else{
										if(currentSpeed_km_h==0){
											if(!DynoModeEnabledOnMaster){
												printEnableDyno=2; //print message Enable DYNO
											}else{
												/* This commented part is a test... not working now


												//if dyno function is enabled but dyno is not enabled, enable dyno
												if(function_dyno_mode_master_enabled){
													if(DynoModeEnabledOnMaster==0){
														//send serial message to C2 baccable, to enable dyno
														uint8_t tmpArr4[2]={C2BusID,C2cmdForceFrontBrake};
														addToUARTSendQueue(tmpArr4, 2);
													}
												}

												//if 4wd function is enabled but 4wd is not disabled, disable 4wd
												if(function_4wd_disabler_enabled){
													if(_4wd_disabled==0){
														//seend messages to disable	4wd
														//TBDone.......
													}
												}
												*/
												//send serial message to C2 baccable, to force front brakes
												uint8_t tmpArr5[2]={C2BusID,C2cmdForceFrontBrake};
												addToUARTSendQueue(tmpArr5, 2);
											}
										}else{
											printStopTheCar=2;//print message "stop the car"
										}
									}


									break;
								case 8: //toggle 4wd
									if(_4wd_disabled>0){ //toggle 4dw disable
										_4wd_disabled=0;
										//update text
										dashboard_main_menu_array[main_dashboardPageIndex][4]=' '; //enabled
										dashboard_main_menu_array[main_dashboardPageIndex][5]='E';
										dashboard_main_menu_array[main_dashboardPageIndex][6]='n';
										commandsMenuEnabled=1;//enable menu commands
									}else{
										if(carSteadyCounter>=100){ //car is steady since at least one second
											_4wd_disabled=4;
											//update text
											dashboard_main_menu_array[main_dashboardPageIndex][4]='D'; //disabled
											dashboard_main_menu_array[main_dashboardPageIndex][5]='i';
											dashboard_main_menu_array[main_dashboardPageIndex][6]='s';
											//commandsMenuEnabled=0;//disable menu commands // 4wd constraint relax change 24/08/2026
										}else{
											printStopTheCar=2;//print message "stop the car"
										}
									}
									break;
									//* @netzmark change
//								case 9: //setup menu
//								case 10: //params setup menu
//									dashboard_menu_indent_level++;
//									break;
								case 9: //setup menu
								    dashboard_menu_indent_level++;
								    resetDashboardTimers(); //@netzmark to speed-up clicks reaction
								    sendSetupDashboardPageToSlaveBaccable();
								    break;
								case 10: //params setup menu
								    dashboard_menu_indent_level++;
								    resetDashboardTimers(); //@netzmark to speed-up clicks reaction
								    sendParamsSetupDashboardPageToSlaveBaccable();
								    break;
								    // @netzmark change
								case 11: //toggle HAS function
									HAS_buttonPressRequested=5;
									//inform Slave baccable C2
									uint8_t tmpArr3[2]={C2BusID,C2cmdToggleHas};
									addToUARTSendQueue(tmpArr3, 2);
									break;
								case 12: //toggle QV Exhaust Valve
									exhaustValvesToggle();
									break;
                                case 13: // @Shadin: Clear DTC log requires LONG OK, short OK does nothing
                                    break;
								case 14: //reset statistics
									resetStatisticsOnFlash();
									break;
								case 15: //Max Hold toggle
									maxHold_enabled=!maxHold_enabled;
									if(maxHold_enabled){
										//force fresh start: next parameter update will initialise max hold from scratch
										dashboardParamCouple[0]=NAN;
										dashboardParamCouple[1]=NAN;
									}
									break;
								default:
									break;
							}

						}else{ //indent level >0
							switch(main_dashboardPageIndex){
								case 9: //setup menu
									switch(setup_dashboardPageIndex){
										case 0: //{'S','A','V','E','&','E','X','I','T',},
											//if some change occurred
											if(	((uint16_t)function_smart_disable_start_stop_enabled	!= readFromFlash(2)) 	|| //S&S enable status
												((uint16_t)function_led_strip_controller_enabled		!= readFromFlash(3)) 	|| //LED_STRIP_CONTROLLER_ENABLED
												((uint16_t)function_shift_indicator_enabled				!= readFromFlash(4)) 	|| //SHIFT_INDICATOR_ENABLED
												((uint16_t)shift_threshold								!= readFromFlash(5))	|| //SHIFT_THRESHOLD
												((uint16_t)function_ipc_my23_is_installed				!= readFromFlash(6))	|| //IPC_MY23_IS_INSTALLED
												((uint16_t)function_route_msg_enabled					!= readFromFlash(7))	|| //ROUTE_MSG
												((uint16_t)function_dyno_mode_master_enabled			!= readFromFlash(8))	|| //DYNO_MODE_MASTER
												((uint16_t)function_acc_virtual_pad_enabled				!= readFromFlash(9))	|| //ACC_VIRTUAL_PAD
												((uint16_t)function_front_brake_forcer_master			!= readFromFlash(10))	|| //FRONT_BRAKE_FORCER_MASTER
												((uint16_t)function_4wd_disabler_enabled				!= readFromFlash(11))	|| //_4WD_DISABLER
												((uint16_t)function_remote_start_Enabled				!= readFromFlash(12))	|| //REMOTE_START_ENABLED
												((uint16_t)function_clear_faults_enabled				!= readFromFlash(13))	|| //CLEAR_FAULTS_ENABLED
												((uint16_t)function_esc_tc_customizator_enabled			!= readFromFlash(14))	|| //ESC_TC_CUSTOMIZATOR_MASTER
												((uint16_t)function_read_faults_enabled					!= readFromFlash(15))	|| //READ_FAULTS_ENABLED
												((uint16_t)function_is_diesel_enabled					!= readFromFlash(16))	|| //IS_DIESEL
												((uint16_t)function_regeneration_alert_enabled			!= readFromFlash(17))	|| //REGENERATION_ALERT_ENABLED
												((uint16_t)launch_torque_threshold						!= readFromFlash(18))	|| //LAUNCH_ASSIST_THRESHOLD
												((uint16_t)function_seatbelt_alarm_enabled				!= readFromFlash(19))	|| //SEATBELT_ALARM_DISABLED
												((uint16_t)function_pedal_booster_enabled				!= readFromFlash(20))	|| //PEDAL_BOOSTER_ENABLED
												((uint16_t)function_disable_odometer_blink				!= readFromFlash(21))	|| //DISABLE_ODOMETER_BLINK
												((uint16_t)function_show_race_mask						!= readFromFlash(22))	|| //SHOW_RACE_MASK
												((uint16_t)function_park_mirror							!= readFromFlash(23))	|| //PARK_MIRROR
												((uint16_t)function_acc_autostart						!= readFromFlash(24))	|| //ACC_AUTOSTART
												((uint16_t)function_close_windows_with_door_lock		!= readFromFlash(25))	|| //CLOSE_WINDOWS
												((uint16_t)function_open_windows_with_door_lock			!= readFromFlash(26))	|| //OPEN_WINDOWS
												((uint16_t)HAS_function_enabled							!= readFromFlash(27))	|| //HAS_VIRTUAL_PAD
												((uint16_t)QV_exhaust_flap_function_enabled				!= readFromFlash(28))	|| //QV_EXHAUST_FLAP_FUNCTION_ENABLED
												((uint16_t)(uint8_t)pedal_map_power						!= readFromFlash(29))	|| //PEDAL_MAP_POWER
												((uint16_t)parkSensorsMuteFunctionEnabled				!= readFromFlash(30))	|| //FRONT_PARK_MUTE
												((uint16_t)snifferFunctionEnabled						!= readFromFlash(31))	|| //SNIFFER //sniffer function 24/08/2026
												((uint16_t)elm327FunctionEnabled						!= readFromFlash(32))	 || //ELM327 //elm327 function 26/08/2026
												((uint16_t)dnaMemoryFunctionEnabled != readFromFlash(33)) ||
                                                ((uint16_t)reverseMuteFunctionEnabled != readFromFlash(34)) ) {
													/* @Shadin: Preserve the original Save&Exit user experience.
													 * Transactional Flash validation stays in config_store_save();
													 * no hidden LED-only error state or menu lock on failure. */
													const uint8_t snifferChanged = snifferFunctionEnabled != (uint8_t)readFromFlash(31);
													const uint8_t elm327Changed = elm327FunctionEnabled != (uint8_t)readFromFlash(32);
													(void)saveOnflash();
													//sniffer function 24/08/2026 - BEGIN
													//the only setting here with a real hardware side effect (usb re-enumeration), so unlike every
													//other setting above it is deliberately applied here, at save&exit, rather than immediately on
													//toggle: this way the usb bring up (and its host detection window) only ever starts
													//once the user has actually committed to leaving this menu with the new value.
													if(snifferChanged){
														if(snifferFunctionEnabled){
															snifferStart();
														}else{
															snifferStop();
														}
														uint8_t tmpArrSniffer[2]={C2_Bh_BusID,C2_Bh_cmdSnifferDisabled};
														if(snifferFunctionEnabled) tmpArrSniffer[1]=C2_Bh_cmdSnifferEnabled;
														addToUARTSendQueue(tmpArrSniffer, 2);
													}
													//sniffer function 24/08/2026 - END

													//elm327 function 26/08/2026 - BEGIN
													//same reasoning as the sniffer above: applied here, at save&exit, not on toggle.
													//Starting it only brings the usb port up and opens the detection window - the
													//baccable keeps working normally until a host actually shows up.
													if(elm327Changed){
														if(elm327FunctionEnabled){
															elm327Start();
														}else{
															elm327Stop();
														}
													}
													//elm327 function 26/08/2026 - END
													/* @Shadin: Saved above; no second Flash write. */
											}
											dashboard_menu_indent_level=0;
											break;
										case 1: //{'[',' ',']','S','t','a','r','t','&','S','t','o','p'},
											function_smart_disable_start_stop_enabled=!function_smart_disable_start_stop_enabled;
											requestToDisableStartAndStop=0;
											break;
										case 2: //{'L','a','u','n','c','h','T','o','r','q','u','e',' ','1','0','0','N,'m',},
											launch_torque_threshold=launch_torque_threshold+25;
											if(launch_torque_threshold>600) launch_torque_threshold=25;
											break;
										case 3: //{'[',' ',']','L','e','d',' ','C','o','n','t','r','o','l','l','e','r',},
											#if defined(LED_STRIP_CONTROLLER_ENABLED)
											function_led_strip_controller_enabled = !function_led_strip_controller_enabled;
										#else
											/* @Shadin: No LED toggle in builds without LED support. */
											function_led_strip_controller_enabled = 0;
										#endif
											break;
										case 4: //{'[',' ',']','S','h','i','f','t',' ','I','n','d','i','c','a','t','o','r'},
											function_shift_indicator_enabled=!function_shift_indicator_enabled;
											break;
										case 5: //{'S','h','i','f','t',' ','R','P','M',' ','3','0','0','0',},
											shift_threshold=shift_threshold+250;
											if(shift_threshold>6000) shift_threshold=1500;
											break;
										case 6: //{'[',' ',']','M','y','2','3',' ','I','P','C', },
											function_ipc_my23_is_installed=!function_ipc_my23_is_installed;
											break;
										case 7: //{'O',' ',' ','R','e','g','e','n','.',' ','A','l','e','r','t',' ',' ',' '},
											function_regeneration_alert_enabled=!function_regeneration_alert_enabled;
											//just for debug
											//uint8_t tmpArr3[1]={BhBusChimeRequest}; //play sound
											//addToUARTSendQueue(tmpArr3, 1);

											break;
										case 8: //{'O',' ',' ','S','e','a','t','b','e','l','t',' ','A','l','a','r','m',' '},
											function_seatbelt_alarm_enabled=!function_seatbelt_alarm_enabled;
											break;
										case 9: //{'[',' ',']','R','o','u','t','e',' ','M','e','s','s','a','g','e','s', },
											function_route_msg_enabled=!function_route_msg_enabled;
											break;
										case 10: //{'[',' ',']','E','S','C','/','T','C',' ','C','u','s','t','o','m','.',},
											function_esc_tc_customizator_enabled = !function_esc_tc_customizator_enabled;

											//send messages to slave boards
											uint8_t tmpArr0[2]={C2_Bh_BusID,C2_Bh_cmdFunction_ESC_TC_Enabled};
											if(!function_esc_tc_customizator_enabled){
												ESCandTCinversion=0; //ensure ESCTC inversion get disabled
												tmpArr0[1]=C2_Bh_cmdFunction_ESC_TC_Disabled;
											}
											addToUARTSendQueue(tmpArr0, 2);
											break;
										case 11: //{'[',' ',']','D','y','n','o',},
											function_dyno_mode_master_enabled=!function_dyno_mode_master_enabled;
											break;
										case 12: //{'[',' ',']','A','C','C',' ','V','i','r','t','u','a','l',' ','P','a','d'},
											function_acc_virtual_pad_enabled=!function_acc_virtual_pad_enabled;
											break;
										case 13: //{'[',' ',']','B','r','a','k','e','s',' ','O','v','e','r','r','i','d','e'},
											function_front_brake_forcer_master=!function_front_brake_forcer_master;
											break;
										case 14: //{'[',' ',']','4','W','D',' ','D','i','s','a','b','l','e','r',},
											function_4wd_disabler_enabled=!function_4wd_disabler_enabled;
											if(!function_4wd_disabler_enabled) show_4wd_disabled_overlay=0; // 4wd constraint relax change 24/08/2026

											break;
										case 15: //{'[',' ',']','C','l','e','a','r',' ','F','a','u','l','t','s',},
											function_clear_faults_enabled=!function_clear_faults_enabled;
											break;
										case 16: //{'[',' ',']','R','e','a','d',' ',' ','F','a','u','l','t','s',},
											function_read_faults_enabled=!function_read_faults_enabled;
											break;
										case 17: //{'[',' ',']','R','e','m','o','t','e',' ','S','t','a','r','t',},
											function_remote_start_Enabled=!function_remote_start_Enabled;
											break;
										case 18: //{'Ø',' ',' ','D','i','e','s','e','l',' ',' ',' ','P','a','r','a','m','s'},
											function_is_diesel_enabled=!function_is_diesel_enabled;
											total_pages_in_params_setup_dashboard_menu = function_is_diesel_enabled ? total_pages_in_dashboard_menu_diesel : total_pages_in_dashboard_menu_gasoline;
											break;
										case 19: // {'O',' ',' ','O','d','o','m','e','t','e','r',' ','B','l','i','n','k',' '},
											function_disable_odometer_blink=!function_disable_odometer_blink;
											//Now let's inform the BH Baccable
											uint8_t tmpArr2[2]={BhBusID,BHcmdOdometerBlinkDefault};
											if(function_disable_odometer_blink) tmpArr2[1]=BHcmdOdometerBlinkDisable;
											addToUARTSendQueue(tmpArr2, 2);
											break;
										case 20: //{'Ø',' ',' ','P','e','d','a','l',' ','B','o','o','s','t','e','r',' ',' '},
											function_pedal_booster_enabled++;
											if (function_pedal_booster_enabled>8) function_pedal_booster_enabled=0; //rotative selection 0=disabled, 1=auto, 2=Bypass, 3=All Weather map, 4=Natural Map, 5=Dynamic Map, 6=Race Map, 7=Hybrid Align, 8=Kids Limiter

											if(function_pedal_booster_enabled==0) setSchizzaforteMap(2); //set bypass map, before to stop to send messages to schizzaForte
										if(function_pedal_booster_enabled==8){ //Kids Limiter: set A map with minimum pedal power at selection
											int8_t saved_pedal_map_power=pedal_map_power;
											pedal_map_power=-10; //force minimum power
											setSchizzaforteMap(3); //A map
											pedal_map_power=saved_pedal_map_power; //restore user setting
										}

											//Now let's inform the C2 and BH Baccable
											uint8_t tmpArr1[3]={C2_Bh_BusID,C2_Bh_cmdSetPedalBoostStatus,function_pedal_booster_enabled};
											addToUARTSendQueue(tmpArr1, 3);
											break;
										case 21: // 'P','e','d','a','l',' ','P','o','w','e','r',':',' ','0',' ',' ',' '
											pedal_map_power=pedal_map_power+2;
											if(pedal_map_power>10) pedal_map_power=-10;
											currentSchizzaforteMap='-';//forget previous setting

											break;
										case 22: // {'O',' ',' ','P','a','r','k',' ','M','i','r','r','o','r',' ',' ',' ',' '},
											function_park_mirror=!function_park_mirror;
											//Now let's inform the BH Baccable
											uint8_t tmpArr4[2]={BhBusID,BHcmdFunctParkMirrorDisabled};
											if(function_park_mirror) tmpArr4[1]=BHcmdFunctParkMirrorStoreCurPos;
											addToUARTSendQueue(tmpArr4, 2);
											break;
										case 23: //{'O',' ',' ','A','C','C','+',' ','A','u','t','o','s','t','a','r','t',' '},
											function_acc_autostart++;
											if(function_acc_autostart>2) function_acc_autostart=0;
											break;
										case 24: //{'O',' ',' ','C','l','o','s','e',' ','W','i','n','d','o','w','s',' ',' '},
											function_close_windows_with_door_lock++;
											if(function_close_windows_with_door_lock>2) function_close_windows_with_door_lock=0;
											closeWindowsRequest=0;
											doorLocksRequestsCounter=0;
											//openWindowsRequest=0;
											//doorUnlocksRequestsCounter=0;
											break;
										case 25: //{'O',' ',' ','O','p','e','n',' ',' ','W','i','n','d','o','w','s',' ',' '},
											function_open_windows_with_door_lock++;
											if(function_open_windows_with_door_lock>2) function_open_windows_with_door_lock=0;
											openWindowsRequest=0;
											doorUnlocksRequestsCounter=0;
											break;
										case 26: //{'O',' ',' ','H','A','S',' ','V','i','r','t','u','a','l',' ','P','a','d'},
											HAS_function_enabled=!HAS_function_enabled;
											//notify to C2 and BH HAS function status
											uint8_t tmpArr5[2]={C2_Bh_BusID,C2_Bh_cmdFunctHAS_Disabled};
											if(HAS_function_enabled) tmpArr5[1]=C2_Bh_cmdFunctHAS_Enabled;
											addToUARTSendQueue(tmpArr5, 2);

											break;
										case 27: //{'O',' ',' ','Q','V',' ','E','x','h','a','u','s','t',' ','F','l','a','p'},
											QV_exhaust_flap_function_enabled=(QV_exhaust_flap_function_enabled+1)%3;
											if(!QV_exhaust_flap_function_enabled) exhaustValvesOpenRequested=0;
											break;
										case 28: //{'O',' ',' ','F','r','o','n','t',' ','P','a','r','k',' ','M','u','t','e'}
											parkSensorsMuteFunctionEnabled=!parkSensorsMuteFunctionEnabled;
											uint8_t tmpArrPkMute[2]={C2BusID,C2cmdFunctParkSensorsMuteDisabled};
											if(parkSensorsMuteFunctionEnabled) tmpArrPkMute[1]=C2cmdFunctParkSensorsMuteEnabled;
											addToUARTSendQueue(tmpArrPkMute, 2);
											break;
										case 29: // Reverse mute: BH CAN function, separate from Front Park Mute.
                                            reverseMuteFunctionEnabled=!reverseMuteFunctionEnabled;
                                            uint8_t tmpArrReverse[2]={BhBusID, reverseMuteFunctionEnabled ? BHcmdReverseMuteEnabled : BHcmdReverseMuteDisabled};
                                            addToUARTSendQueue(tmpArrReverse, 2);
                                            break;
										case 30: dnaMemoryFunctionEnabled=!dnaMemoryFunctionEnabled; break;
                                        case 31: //{'O',' ',' ','S','N','I','F','F','E','R'} //sniffer function 24/08/2026
											//just a plain preference toggle now, like every other setting on this menu: the actual usb
											//bring up (or shutdown) is deferred to SAVE&EXIT, see case 0 below
											snifferFunctionEnabled=!snifferFunctionEnabled;
											//elm327 function 26/08/2026 - the two share the single usb cdc port: enabling one visibly
											//clears the other, rather than leaving a tick that would silently do nothing
											if(snifferFunctionEnabled) elm327FunctionEnabled=0;
											break;
										case 32: //{'O',' ',' ','E','L','M','3','2','7'} //elm327 function 26/08/2026
											elm327FunctionEnabled=!elm327FunctionEnabled;
											if(elm327FunctionEnabled) snifferFunctionEnabled=0;
											break;
																				default:
											break;
									}
									resetDashboardTimers(); //@netzmark to speed-up clicks reaction XX
									sendSetupDashboardPageToSlaveBaccable();
									break;
								case 2: //read faults - ritorna al menu principale //readFaults 12/08/2026
									faultsStateMachine = 0xFF; // interrompe eventuale sequenza UDS in corso
									dashboard_menu_indent_level = 0;
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
                                    // Read Faults overwrites the persistent menu row with ECU status.
                                    // Restore the original label before rendering the main menu.
                                    // Clear the entire row to avoid stale characters on LARGE_DISPLAY.
                                    memset(dashboard_main_menu_array[2], ' ', DASHBOARD_MESSAGE_MAX_LENGTH);
                                    memcpy(dashboard_main_menu_array[2], "Read faults", sizeof("Read faults") - 1u);
                                    resetDashboardTimers();
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
									sendMainDashboardPageToSlaveBaccable();
									break;
								case 10: //PARAMS SETUP MENU
									switch(params_setup_dashboardPageIndex){
										case 0: //{'S','A','V','E','&','E','X','I','T',},
											/* @Shadin: Keep the original Save&Exit behavior. */
											(void)saveShownParamsOnflash();
											dashboard_menu_indent_level=0;
											resetDashboardTimers(); //@netzmark to speed-up clicks reaction XX
											sendMainDashboardPageToSlaveBaccable(); //@netzmark to speed-up clicks reaction XX
											break;
										default: //toggle hidden variable of current param
											shownParamsArray[params_setup_dashboardPageIndex-1]=!shownParamsArray[params_setup_dashboardPageIndex-1];
											resetDashboardTimers();  //@netzmark to speed-up clicks reaction XX
											sendParamsSetupDashboardPageToSlaveBaccable();  //@netzmark to speed-up clicks reaction XX
											break;
									}
									break;
								default:
									//we want to return main menu
									dashboard_menu_indent_level=0;
									resetDashboardTimers();  //@netzmark to speed-up clicks reaction XX
									sendMainDashboardPageToSlaveBaccable(); //print menu
							}
						}



					}
					wheelPressedButtonID=0x10; //button released
					lastPressedWheelButtonDuration=0;
					lastPressedSpeedUpWheelButtonDuration=0;
					break;
				case 0x90: //RES button was pressed
				case 0x50: //distance selector, used like RES, to manage the menu
					#ifndef HIDE_DASHBOARD_MENU
						lastPressedWheelButtonDuration++;

						if (wheelPressedButtonID==0x10 && wheelPressedButtonID!=0x90){//we pressed RES for at least one instant
							wheelPressedButtonID=0x89; //avoid returning here until button is not released
						}
						if (wheelPressedButtonID==0x89 && (lastPressedWheelButtonDuration>50)){//we pressed RES for around 2 seconds, therefore we want to enable/disable Baccable menu on dashboard
							wheelPressedButtonID=0x90; //avoid returning here until button is not released

if (baccableDashboardMenuVisible && dashboard_menu_indent_level==0 && main_dashboardPageIndex==2 && function_read_faults_enabled==1) {
                                stageC1cDtcBegin();
                                dtcLogManualReadBegin(); // @Shadin: one log per manual read
                                dashboard_menu_indent_level++;
                                sendMainDashboardPageToSlaveBaccable();
                            } else if (baccableDashboardMenuVisible &&
                                       dashboard_menu_indent_level==0 && main_dashboardPageIndex==13) {
                                /* @Shadin: Clear only BH journal; menu visibility is unchanged. */
                                dtcLogRequestClear();
                            } else {
                            baccableDashboardMenuVisible=!baccableDashboardMenuVisible; //toggle visualizazion of the menu

							//LOG("Dashboard vis: %d\r\n", baccableDashboardMenuVisible);

							if(!baccableDashboardMenuVisible) baccableFeaturesActive=0;
                            if(!baccableDashboardMenuVisible){ //if menu needs to be hidden, print spaces to clear the string on dashboard
								clearDashboardBaccableMenu();
							// Request a one-shot HU text refresh after the clear frame.
							// BH will independently check gear, mute feedback and timeouts.
							uint8_t huRestoreCmd[2] = {BhBusID, BHcmdHuTextRestore};
							addToUARTSendQueue(huRestoreCmd, 2);
							}else{
								//dashboardPageIndex=0; //reset the page, just to be sure to show initial Baccable print
								//main_dashboardPageIndex=0; //shows initial baccable version
								//dashboard_menu_indent_level=0;
							}



                            } // normal long-OK menu toggle
						}
					#endif

					break;
				case 0x12: //Cruise Control Disabled/Enabled
					break;
				default:
			}
		}

		#ifndef PERMANENTLY_DISABLE_IMMO
			if(cruiseControlDisabled && ACC_Disabled ){ //if we are allowed to use the buttons of the cruise control
				if (currentRpmSpeed>400){ //if motor is on
					if((currentGear==0) || ((currentGear==0x0F) && (carSteadyCounter==200))){ //neutral, or unavailable gear with vehicle stationary for >=2 s (MIL case)
						if((rx_msg_data[0]==0x08) && ((wheelPressedButtonID==0x10) || (wheelPressedButtonID==0x08))){ //user is pressing CC soft speed up button and it was previously released (or pressed by baccable menu up here)
							lastPressedSpeedUpWheelButtonDuration++;
							if(lastPressedSpeedUpWheelButtonDuration>422){ 	// @netzmark: originally it was 1267 (around 30s), I do it faster around 10s.
								//avoid to return here
								wheelPressedButtonID=0xF8; //invent a new status to differentiate it from 0x08 used in baccable menu few lines of code up here
								lastPressedSpeedUpWheelButtonDuration=0; //unuseful here since it is done when button is released. just to be superstitious :-D.
								immobilizerEnabled=!immobilizerEnabled;//toggle immobilizer status
								floodTheBus=0; //ensure to reset this even if probably it is not needed
								if(saveOnflash()!=0u){ // @Shadin: Any nonzero save result is an error; revert immobilizer as before.
									immobilizerEnabled=!immobilizerEnabled;//toggle immobilizer status to the original status and avoid to report the user anything
									onboardLed_red_on(); //a problem occurred
								}else{
									onboardLed_blue_on(); //everything goes fine. change saved on flash
									if(immobilizerEnabled){ //if immo enabled
										executeDashboardBlinks=6; //blinks the dashboard brightness 3 times
									}else{
										executeDashboardBlinks=12; //blinks the dashboard brightness 6 times
									}
								}


							}
						}
						if(rx_msg_data[0]==0x10){ //user released the button
							lastPressedSpeedUpWheelButtonDuration=0;
							wheelPressedButtonID=0x10; //button released
						}
					}

				}
			}
		#endif
	#endif

}
