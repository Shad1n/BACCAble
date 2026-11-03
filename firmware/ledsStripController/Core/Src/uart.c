#include "uart.h"
/* @Shadin: Shared C1/C2 ABS UART protocol moved from abs_proto.c.
 * Original encoder/decoder implementation preserved unchanged.
 */
#include "uart.h" /* @Shadin: ABS protocol declarations now in uart.h. */
#include <string.h>
static int count_ok(unsigned total,unsigned kept){return kept==((total>ABS_PROTO_MAX)?ABS_PROTO_MAX:total);}
static char h(unsigned x){return "0123456789ABCDEF"[x&15u];}
static int v(uint8_t c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static void put(uint8_t *p,uint8_t x){p[0]=h(x>>4);p[1]=h(x);}
static int get(const uint8_t *p){int a=v(p[0]),b=v(p[1]);return (a<0||b<0)?-1:(a<<4)|b;}
int abs_encode(uint8_t out[ABS_PROTO_FRAME],uint8_t route,const AbsReply *r){
 if(!out||!r||(r->status=='R'&&!count_ok(r->total,r->kept))||r->kept>(int)ABS_PROTO_MAX||r->index>r->kept||(r->status!='R'&&r->status!='B'&&r->status!='E'))return -1;
 memset(out,' ',ABS_PROTO_FRAME);out[0]=route;out[1]=ABS_PROTO_REPLY;put(out+2,r->id);out[4]=r->status;out[5]=r->reason;put(out+6,r->total);put(out+8,r->kept);put(out+10,r->index);
 if(r->status=='R'&&r->index<r->kept){for(unsigned i=0;i<3;i++)put(out+12+2*i,r->code[i]);}return 0;
}
int abs_decode(const uint8_t in[ABS_PROTO_FRAME],uint8_t route,AbsReply *r){
 if(!in||!r||in[0]!=route||in[1]!=ABS_PROTO_REPLY||in[18]!=' ')return -1;
 int id=get(in+2),total=get(in+6),kept=get(in+8),index=get(in+10);
 if(id<0||total<0||kept<0||index<0||kept>(int)ABS_PROTO_MAX||index>kept|| (in[4]!='R'&&in[4]!='B'&&in[4]!='E'))return -1;
 if(in[4]=='R'&&!count_ok((unsigned)total,(unsigned)kept))return -1;
 AbsReply tmp={0};tmp.id=id;tmp.status=in[4];tmp.reason=in[5];tmp.total=total;tmp.kept=kept;tmp.index=index;
 if(tmp.status=='R'&&tmp.index<tmp.kept){for(unsigned i=0;i<3;i++){int x=get(in+12+2*i);if(x<0)return -1;tmp.code[i]=x;}}
 else {for(unsigned i=12;i<18;i++)if(in[i]!=' ')return -1;}
 *r=tmp;return 0;
}

#if defined(C1baccable)
#include "uart.h"
#include "functions_C1baccable.h"
#include "functions_C1baccable.h"
#endif

/* @Shadin: C1 ABS UART inbox implementation moved here from abs_c1_inbox.c.
 * Preserve single-producer ISR / single-consumer FIFO semantics and IRQ state.
 */
#if defined(C1baccable)
static uint8_t frames[ABS_C1_INBOX_CAPACITY][ABS_PROTO_FRAME];
static volatile uint8_t head, tail;
static volatile uint16_t dropped;
void abs_c1_inbox_reset(void) {
#if defined(C1baccable)
    /* Preserve the caller's interrupt state, including calls made with IRQs masked. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
#endif
    head = 0u;
    tail = 0u;
    dropped = 0u;
#if defined(C1baccable)
    __set_PRIMASK(primask);
#endif
}
int abs_c1_inbox_push_isr(const uint8_t *frame) {
    uint8_t next;
    if (!frame) return 0;
    next=(uint8_t)((head+1u)%ABS_C1_INBOX_CAPACITY);
    if (next==tail) { ++dropped; return 0; }
    memcpy(frames[head],frame,ABS_PROTO_FRAME);
    head=next;
    return 1;
}
int abs_c1_inbox_pop(uint8_t frame[ABS_PROTO_FRAME]) {
    if (!frame || tail==head) return 0;
    memcpy(frame,frames[tail],ABS_PROTO_FRAME);
    tail=(uint8_t)((tail+1u)%ABS_C1_INBOX_CAPACITY);
    return 1;
}
uint16_t abs_c1_inbox_dropped(void) { return dropped; }

#endif /* C1baccable */

/* Decode two uppercase ASCII hex characters without changing UART framing. */
#if defined(C2baccable)
static int abs_uart_parse_id(const uint8_t *p, uint8_t *value) {
    uint8_t a, b;
    if (!p || !value) return 0;
    a = (p[0] >= '0' && p[0] <= '9') ? p[0]-'0' :
        (p[0] >= 'A' && p[0] <= 'F') ? p[0]-'A'+10 : 255;
    b = (p[1] >= '0' && p[1] <= '9') ? p[1]-'0' :
        (p[1] >= 'A' && p[1] <= 'F') ? p[1]-'A'+10 : 255;
    if (a > 15 || b > 15) return 0;
    *value = (uint8_t)((a << 4) | b);
    return 1;
}
#endif /* C2baccable */
#include "stm32f0xx_hal.h"
#include "main.h"
//#include "globalVariables.h"
//extern void Error_Handler(void);

#define QUEUE_SIZE 10  // max queue size

typedef struct {
    uint8_t tx_buffer[QUEUE_SIZE][UART_BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} SendQueue;

SendQueue queue_instance= {0};
SendQueue *tx_queue = &queue_instance;

/* USART2 HAL interrupt TX must not reference a reusable queue slot. */
static volatile uint8_t uart2_direct_tx_owned = 0u;
#if defined(C1baccable) || defined(C2baccable) || defined(BHbaccable)
static uint8_t uart2_tx_snapshot[UART_BUFFER_SIZE];
static uint8_t uart2_hal_tx_start(const uint8_t *source) {
    uint8_t started = 0u;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (!uart2_direct_tx_owned && __HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) &&
        (huart2.State == HAL_UART_STATE_READY || huart2.State == HAL_UART_STATE_BUSY_RX)) {
        memcpy(uart2_tx_snapshot, source, UART_BUFFER_SIZE);
        started = (HAL_UART_Transmit_IT(&huart2, uart2_tx_snapshot, UART_BUFFER_SIZE) == HAL_OK);
    }
    __set_PRIMASK(mask);
    return started;
}
#endif /* C1baccable || C2baccable || BHbaccable */




uint32_t last_sent_serial_msg_time=0;

uint32_t lastMsgSentToC2Time=0;
uint32_t lastMsgSentToBHTime=0;

uint8_t rxBuffer[UART_BUFFER_SIZE]; //buffer to receive the message from uart

#if defined(C1baccable)
	uint32_t last_sent_serial_to_schizzaForte_msg_time=0;
	uint8_t rxBufferUart1[UART1_BUFFER_SIZE]; //buffer to receive the message from uart (schizzaForte)
	SendQueue queue_instance_uart1= {0};
	SendQueue *tx_queue_uart1 = &queue_instance_uart1;
	/* Stable storage for the duration of interrupt-driven USART1 TX. */
	static uint8_t uart1_tx_snapshot[UART1_BUFFER_SIZE];

#endif

uint8_t rxIndex = 0;
uint8_t syncObtained=0;

//weCanSendAMessageReply identifies last time a message was received by BH and C2 baccable (used by BH and C2 baccable)
//requestToSendOneFrame is Set to 1 to send one frame on dashboard
//uartTxMsg array contains the serial message to send
//clearFaultsRequest, if enabled, sends  messages to clear faults

void uart_init(){

    __HAL_RCC_GPIOA_CLK_ENABLE(); //enable clock for Uart2

    // Configure PA14 as Half-Duplex TX/RX
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;      // Open-Drain to support both TX and RX on the same pin
    GPIO_InitStruct.Pull = GPIO_PULLUP;          // Necessary to avoid floating states
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	#if defined(C1baccable)
		__HAL_RCC_GPIOB_CLK_ENABLE(); //enable clock for Uart1 (to schizzaforte)

		// Configure PB6 as Half-Duplex TX/RX (to schizzaforte)
		GPIO_InitStruct.Pin = GPIO_PIN_6;
		//GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		//GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
		GPIO_InitStruct.Alternate = GPIO_AF0_USART1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);

	#endif

	HAL_Delay(4); //let's wait that electric signal on serial line get stabilized.
	//HAL_Delay(100);onboardLed_red_on();
	__HAL_RCC_USART2_CLK_ENABLE(); //enable clock for usart2

	// Configure USART2 in Half-Duplex mode
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 38400; //38400;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;  // Enable both TX and RX
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_HalfDuplex_Init(&huart2) != HAL_OK) Error_Handler(500); //error

    // **Enable the interrupt in the NVIC (if not already set in CubeMX)**
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    // **Enable reception in interrupt mode**
    HAL_UART_Receive_IT(&huart2, &rxBuffer[0], 1);  // Start receiving one byte

	#if defined(C1baccable)
    	__HAL_RCC_USART1_CLK_ENABLE(); //enable clock for usart1 (to schizzaforte)

		huart1.Instance = USART1;
		huart1.Init.BaudRate = 9600;
		huart1.Init.WordLength = UART_WORDLENGTH_8B;
		huart1.Init.StopBits = UART_STOPBITS_1;
		huart1.Init.Parity = UART_PARITY_NONE;
		huart1.Init.Mode = UART_MODE_TX_RX;   // richiesto anche in half-duplex
		huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
		huart1.Init.OverSampling = UART_OVERSAMPLING_16;
		huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

		//volatile uint32_t tmp = huart1.Instance->CR1; //fake reading, just to let uart1 be prepared
		//(void)tmp;

		//for (voltile int i = 0; i < 10000; i++); //just for test, to allow electric signal to stabilize

		if (HAL_HalfDuplex_Init(&huart1) != HAL_OK) Error_Handler(750);

		// **Enable the interrupt in the NVIC (if not already set in CubeMX)**
		HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(USART1_IRQn);

		// **Enable reception in interrupt mode**
		HAL_UART_Receive_IT(&huart1, &rxBufferUart1[0], 1);  // Start receiving one byte


	#endif

}

//interrupt called when message is received
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	#if defined(C1baccable)
		if (huart->Instance == USART1) { //message from schizzaForte
			//record current mode
			switch(rxBufferUart1[0]){ // 0x10=Bypass, 0x59=All weather, 0xa2=Natural, 0xeb=Dynamic, 0x34=Race
				case 0x10:
					currentSchizzaforteMap='B';
					break;
				case 0x59:
					currentSchizzaforteMap='A';
					break;
				case 0xa2:
					currentSchizzaforteMap='N';
					break;
				case 0xeb:
					currentSchizzaforteMap='D';
					break;
				case 0x34:
					currentSchizzaforteMap='R';
					break;
				default:
					currentSchizzaforteMap='?';
					break;
			}
			// rxBufferUart1[0]
			onboardLed_blue_on();
			HAL_UART_Receive_IT(&huart1, &rxBufferUart1[0], 1); //receive one char

			#if defined(ACT_AS_SCHIZZAFORTE_SERIAL_CONTROLLER) //if schizzaforte gateway is enabled
				CDC_Transmit_FS(rxBufferUart1, (uint16_t)1);
			#endif
		}

	#endif

    if (huart->Instance == USART2) { //message from other baccable chips
		// evaluate received message
		//elm327 function 28/08/2026 - the diagnostic bridge uses its own destinations (0x0E/0x0F/0x10), which
		//continue the numbering past C1_C2_BusID (0x0D): the upper bound of the valid-start-byte check is raised
		//to ELMLINK_TO_MASTER so the sync state machine assembles the whole 19-byte bridge frame before we touch
		//it. The bridge block is handed to elmlink_on_uart_frame() only once the frame is complete (inside the
		//syncObtained branch below): that is what makes the single-wire framing reliable.
		#if defined(C1baccable) || defined(C2baccable) || defined(BHbaccable)
			#define UART_LAST_MSG_ID ELMLINK_TO_MASTER
		#else
			#define UART_LAST_MSG_ID C1_C2_BusID
		#endif
    	if((rxBuffer[0]>=C1BusID) && (rxBuffer[0]<=UART_LAST_MSG_ID)){ //if the received char indicates the beginning of a message
			if(syncObtained){ //if we were sync, we can process the message, since the first char is correct and the sync indicates that te remaining part too is complete
				#if defined(ACT_AS_CANABLE)
					onboardLed_blue_on();
				#endif

				//elm327 function 28/08/2026 - a complete, synchronised bridge frame is now in rxBuffer. Return 2 means
				//the checksum is wrong, i.e. the framing slipped by a byte on the single-wire line: drop the sync and
				//re-acquire one byte at a time, otherwise the link stays desynced forever and every remote bus dies
				//after the first busy session. Return 1 means the block was ours and is consumed here.
				#if defined(C1baccable) || defined(C2baccable) || defined(BHbaccable)
				{
					uint8_t br = elmlink_on_uart_frame((const uint8_t *)rxBuffer);
					if(br == 2){
						syncObtained = 0;
						HAL_UART_Receive_IT(&huart2, &rxBuffer[0], 1);
						return;
					}
					if(br){
						HAL_UART_Receive_IT(&huart2, &rxBuffer[0], UART_BUFFER_SIZE);
						return;
					}
				}
				#endif

				switch(rxBuffer[0]){
					case C1BusID: //message directed to baccable connected to C1 bus

						#if defined(C1baccable)
							switch(rxBuffer[1]){
                                case C1cmdAbsFaultsReply:
                                    /* Do not decode DTCs in the UART interrupt.
                                     * Stage C2 integration: queued only; not yet activated. */
                                    (void)abs_c1_inbox_push_isr(rxBuffer);
                                    break;

								case C1cmdForceFrontBrake:
									front_brake_forced=1; //update status
									launch_assist_enabled=1; //enable launch assist
									break;
								case C1cmdNormalFrontBrake:
									front_brake_forced=0;
									launch_assist_enabled=0; //disable launch assist
									break;
								case C1cmdDynoActive:
									DynoModeEnabledOnMaster=1; //dyno active
									break;
								case C1cmdDynoNotActive:
									DynoModeEnabledOnMaster=0; //dyno not active
									break;
								case C1cmdLaneDoubleTap:
									if(HAS_function_enabled) HAS_buttonPressRequested=5; //press HAS for 5 times (5 messages)
									break;
								case C1cmdLaneSingleTap: //request to C2 to execute the ESC/TC toggle
									//onboardLed_red_on();
									uint8_t tmpArr2[2]={C2BusID,C2cmdtoggleEscTc};
									addToUARTSendQueueDuringInterrupt(tmpArr2, 2);
									break;
								//sniffer function 24/08/2026 - BEGIN
								//tracked per slave: the sniffer enable command is broadcast to C2 and BH together,
								//so the one with no cable attached always times out, and with a single shared
								//command its "disconnected" would clear the state the other slave had just
								//legitimately reported (and C1 would sleep with a session still in use).
								case C1usbConnectedFromC2: //usb connected to C2. avoid sleep
									usbConnectedToC2=1;
									usbConnectedToSlave=1;
									break;
								case C1usbDisconnectedFromC2: //C2 usb no longer configured
									usbConnectedToC2=0;
									usbConnectedToSlave=usbConnectedToBH;
									break;
								case C1usbConnectedFromBH: //usb connected to BH. avoid sleep
									usbConnectedToBH=1;
									usbConnectedToSlave=1;
									break;
								case C1usbDisconnectedFromBH: //BH usb no longer configured
									usbConnectedToBH=0;
									usbConnectedToSlave=usbConnectedToC2;
									break;
								//sniffer function 24/08/2026 - END
								default:
									break;
							}
							onboardLed_blue_on();
						#endif
						break;
					case C2BusID: //message directed to baccable connected to C2 bus
						#if defined(C2baccable)
							switch (rxBuffer[1]){
								case C2cmdtoggleDyno: //dyno request
									if(front_brake_forced==0) dynoToggle();
									break;
								case C2cmdtoggleEscTc: // ESC/TC request
									//if we can, enable it
									if(function_esc_tc_customizator_enabled){
										if(!(DynoModeEnabled || DynoStateMachine!=0xff)) ESCandTCinversion=!ESCandTCinversion;
										uint8_t tmpArr1[2]={C1_Bh_BusID, C1BHcmdShowRaceScreen};
										if(ESCandTCinversion && function_show_race_mask){ //if enabled and race screen requested, notify C1 and BH
											addToUARTSendQueueDuringInterrupt(tmpArr1, 2);
										}else{
											tmpArr1[1]=C1BHcmdStopShowRaceScreen;
											addToUARTSendQueueDuringInterrupt(tmpArr1, 2);
										}
									}
									break;
								case C2cmdForceFrontBrake: //force front brake
									front_brake_forced=5;
									break;
								case C2cmdNormalFrontBrake: //release the brake - we set it to 255, just to trigger serial msg sending in the main (sync) and not here (async), since async may cause concurrent variables write
									front_brake_forced=255;
									break;
								case C2cmdRaceMaskDefault: //do not request to show race mask
									function_show_race_mask=0;
									break;
								case C2cmdShowRaceMask: //request to show race mask
									function_show_race_mask=1;
									break;
								case C2cmdToggleHas: //request to press HAS button for more consecutive messages
									HAS_buttonPressRequested=5;
									break;
								case C2cmdAbsFaultsStart: {
					/* Transaction id is ASCII hex at offsets 2..3. */
					uint8_t parsedId;
					if (abs_uart_parse_id(rxBuffer + 2, &parsedId)) {
						absFaultsRequestId = parsedId;
						absFaultsStartRequest = 1;
						/* Invalidate pending Get from previous transaction. */
						absFaultsGetIndex = 0xFF;
					}
					break;
				}
				case C2cmdAbsFaultsGet: {
					/* The sender checks the transaction again before replying. */
					uint8_t parsedId, parsedIndex;
					if (abs_uart_parse_id(rxBuffer + 2, &parsedId) &&
						abs_uart_parse_id(rxBuffer + 4, &parsedIndex) &&
						parsedIndex <= ABS_FAULTS_DTC_MAX &&
						parsedId == absFaultsRequestId) {
						absFaultsGetRequestId = parsedId;
						absFaultsGetIndexRequest = parsedIndex;
						absFaultsGetIndex = parsedIndex;
					}
					break;
				}
				case C2cmdGetStatus:
									//nothing to do, since we just need to set weCanSendAMessageReply
									break;
								case C2cmdFunctParkSensorsMuteDisabled: //park sensors mute disabled
									parkSensorsMuteFunctionEnabled = 0;
									break;
								case C2cmdFunctParkSensorsMuteEnabled: //park sensors mute enabled
									parkSensorsMuteFunctionEnabled = 1;
									break;
								default:
									break;
							}


							onboardLed_blue_on();
							weCanSendAMessageReply=currentTime;
						#endif

						break;
					case BhBusID: //message directed to baccable connected to BH bus
						#if defined(BHbaccable)
							switch(rxBuffer[1]){
								case BHcmdOdometerBlinkDisable: //odometer blink disable request
									disable_odometer_blink=1;
									break;
								case BHcmdOdometerBlinkDefault: //odometer blink default request
									disable_odometer_blink=0;
									break;
								case BHcmdFunctParkMirrorDisabled: //park mirror disable
									function_park_mirror=0;
									parkMirrorOperativePositionNotStored=1;
									break;
								case BHcmdFunctParkMirrorEnabled: //park mirror enabled
									function_park_mirror=1;
									break;
								case BHcmdFunctParkMirrorStoreCurPos: //park mirror enabled and transfer current mirror position
									function_park_mirror=1;
									storeCurrentParkMirrorPosition=1;
									break;
								case BHcmdReverseMuteEnabled:
                                    reverseAutoMuteEnabled=1;
                                    break;
                                case BHcmdReverseMuteDisabled:
                                    reverseAutoMuteEnabled=0;
                                    break;
                                case BHcmdHuTextRestore:
									bhHuTextRestoreRequest = 1;
									break;
							case BHcmdDnaMemoryRestore:
									if(rxBuffer[2]=='A'||rxBuffer[2]=='N'||rxBuffer[2]=='D'||rxBuffer[2]=='R') dnaRestoreRequest=rxBuffer[2];
									break;
								default:
							}
							weCanSendAMessageReply=currentTime;
							onboardLed_blue_on();
						#endif
						break;

					case BhBusIDparamString: //message directed to baccable connected to BH bus in order to transfer a parameter to print
						#if defined(BHbaccable)
							memcpy(&dashboardPageStringArray[0], &rxBuffer[1], DASHBOARD_MESSAGE_MAX_LENGTH); //copy array that we will use in the main

							if(ESCandTCinversion){ //if esc/tc is active, don't show baccable menu
								requestToSendOneFrame=0;
							}else{
								// Keep only the current transmission and one latest-page refresh.
                                // A new page must not queue multiple obsolete full display cycles.
                                // The BH snapshot is refreshed at the next frame-zero boundary.
                                if (requestToSendOneFrame == 0) requestToSendOneFrame = 1;
                                else if (requestToSendOneFrame == 1) requestToSendOneFrame = 2;
							}
							weCanSendAMessageReply=currentTime;
							onboardLed_blue_on();
						#endif
						break;

					case BhBusIDgetStatus:
						#if defined(BHbaccable)
							weCanSendAMessageReply=currentTime;
							onboardLed_blue_on();
						#endif
						break;

					case BhBusChimeRequest:
						#if defined(BHbaccable)
							weCanSendAMessageReply=currentTime;
							requestToPlayChime=1;
							onboardLed_blue_on();
						#endif
						break;

					case AllSleep: //message directed to all the modules, in order to request low consumption
						//Not used for now..
						break;
					case C2BusIDAllSleepAck: //message directed to baccable on C1 bus, generated by C2 baccable, to inform that he received the sleep request
						//if we are the baccable on C1 bus
						break;
					case BHBusIDAllSleepAck: //message directed to baccable on C1 bus, generated by BH baccable, to inform that he received the sleep request
						//if we are the baccable on C1 bus
						//Not used for now..
						break;
					case AllResetFaults: //message received by baccable on BH and C2 bus. we shall reset all faults
						#if (defined(C2baccable) || defined(BHbaccable))
							clearFaultsRequest=255;

							#if defined(C2baccable)
								weCanSendAMessageReply=currentTime; //we decided that only C2 is allowed to reply
							#endif

							onboardLed_blue_on();
						#endif
						break;

					case C1_C2_BusID: //message received by C1 and C2 baccable.
						#if (defined(C1baccable) || defined(C2baccable))
							if(rxBuffer[1]==C1_C2_cmdLaneDoubleTap){
								if(HAS_function_enabled) HAS_buttonPressRequested=5; //press HAS for 5 times (5 messages)
							}
							onboardLed_blue_on();
						#endif
						break;

					case C1_Bh_BusID: //message received by C1 and BH baccable.
						#if (defined(C1baccable) || defined(BHbaccable))
						//#if defined(C1baccable)
							if(rxBuffer[1]==C1BHcmdShowRaceScreen){
								if(function_esc_tc_customizator_enabled) ESCandTCinversion=1;
							}
							if(rxBuffer[1]==C1BHcmdStopShowRaceScreen){
								ESCandTCinversion=0;
							}
							#if defined(C1baccable)
							if(rxBuffer[1]==C1BHcmdDnaManualSelection) dna_stageg_note_manual_selection();
							#endif
							onboardLed_blue_on();
						#endif
						break;

					case C2_Bh_BusID: //message directed to baccable connected to C2 and BH bus
						#if defined(C2baccable) || defined(BHbaccable)
							switch(rxBuffer[1]){
								case C2_Bh_cmdSetPedalBoostStatus:
									//third byte contains the pedal booster status
									function_pedal_booster_enabled=rxBuffer[2];
									break;
								case C2_Bh_cmdFunctHAS_Disabled:
									HAS_function_enabled=0;
									break;
								case C2_Bh_cmdFunctHAS_Enabled:
									HAS_function_enabled=1;
									break;
								case C2_Bh_cmdFunction_ESC_TC_Disabled:
									function_esc_tc_customizator_enabled=0;
									ESCandTCinversion=0;
									break;
								case C2_Bh_cmdFunction_ESC_TC_Enabled:
									function_esc_tc_customizator_enabled=1;
									break;
								case C2_Bh_cmdFunction_Save_Log_to_File:
									saveToFilesystem();
									break;
								case C2_Bh_cmdSnifferDisabled: //sniffer function 24/08/2026
									snifferStop();
									break;
								case C2_Bh_cmdSnifferEnabled: //sniffer function 24/08/2026 - only ram is touched here, the usb bring up is served by the main loop
									snifferStart();
									break;
								default:
							}

							onboardLed_blue_on();
							#if defined(C2baccable)
								weCanSendAMessageReply=currentTime; //we decided that only C2 can reply, otherwise errors may arise
							#endif
						#endif
						break;

					default:
						//not expected to end up here
						break;
				}

				HAL_UART_Receive_IT(&huart2, &rxBuffer[0], UART_BUFFER_SIZE); //receive next frame  //commented for test
			}else{ //otherwise we were not sync, therefore we need to receive the remaining part of the message
				syncObtained=1;
				HAL_UART_Receive_IT(&huart2, &rxBuffer[1], UART_BUFFER_SIZE-1); //receive remaining part of the frame  //commented for test
			}
    	}else { //we did not receive the begin of the message. discard it
			syncObtained=0; //we lost sync
			//onboardLed_blue_on();
			HAL_UART_Receive_IT(&huart2, &rxBuffer[0], 1); //receive one char  //commented for test
		}
    }
}

// interrupt called when message send is complete
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){

	if (huart->Instance == USART2){
        //message sent. what do we do?
    	onboardLed_blue_on(); //successfully sent
    }

	#if defined(C1baccable)
		if(huart->Instance == USART1){
			//onboardLed_blue_on(); //successfully sent
			CLEAR_BIT(huart1.Instance->CR1, USART_CR1_TE);		//disable TX
			SET_BIT(huart1.Instance->CR1, USART_CR1_RE);		//enable RX
			HAL_UART_Receive_IT(&huart1, &rxBufferUart1[0], 1); //restart RX
		}
	#endif
}

//pause uart
void pauseUart(UART_HandleTypeDef *huart){
	__HAL_UART_DISABLE_IT(huart, UART_IT_RXNE); //disable RX interrupt (RXNE)
    __HAL_UART_DISABLE_IT(huart, UART_IT_ERR); //disable error interrupts (ORE overrun, FE framing, NE noise)

    // deletes all UARTS errors
    __HAL_UART_CLEAR_FLAG(huart,UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF | UART_CLEAR_PEF);

	__HAL_UART_FLUSH_DRREGISTER(huart); // Clear RXNE by flushing the data register

	//__HAL_UART_DISABLE(huart);
}

/* Cancel only an idle receive operation; never overwrite an active HAL TX. */
static uint8_t uart_rx_reset_idle(UART_HandleTypeDef *huart) {
    if (huart->State == HAL_UART_STATE_BUSY_TX ||
        huart->State == HAL_UART_STATE_BUSY_TX_RX) return 0u;
    __HAL_UART_DISABLE_IT(huart, UART_IT_RXNE);
    __HAL_UART_DISABLE_IT(huart, UART_IT_PE);
    __HAL_UART_DISABLE_IT(huart, UART_IT_ERR);
    huart->RxXferCount = 0u;
    huart->RxXferSize = 0u;
    huart->pRxBuffPtr = NULL;
    huart->State = HAL_UART_STATE_READY;
    return 1u;
}

//wake up serial line after pause
void restartUart(UART_HandleTypeDef *huart){
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if ((huart->Instance == USART2 && uart2_direct_tx_owned) ||
        !uart_rx_reset_idle(huart)) {
        __set_PRIMASK(mask);
        return;
    }
    syncObtained = 0;
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF | UART_CLEAR_PEF);
    __HAL_UART_FLUSH_DRREGISTER(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;
#if defined(C1baccable)
    if (huart->Instance == USART1) {
        (void)HAL_UART_Receive_IT(huart, &rxBufferUart1[0], 1);
        last_sent_serial_to_schizzaForte_msg_time = currentTime;
    }
#endif
    if (huart->Instance == USART2) {
        (void)HAL_UART_Receive_IT(huart, &rxBuffer[0], 1);
        last_sent_serial_msg_time = currentTime;
    }
    __set_PRIMASK(mask);
}


//elm327 function 26/08/2026 - BEGIN
#if defined(C1baccable) || defined(C2baccable) || defined(BHbaccable)
	//Sends one bridge block by writing the data register directly, with reception off while transmitting.
	//HAL_UART_Transmit() is deliberately NOT used: it takes the same lock and the same State field as
	//HAL_UART_Receive_IT(), so if the rx interrupt completed a block mid transmission the re-arm would fail
	//with HAL_BUSY and the port would stay deaf for good. It also waits out any transmission already started
	//by the normal queue (HAL_UART_Transmit_IT), otherwise the two messages would interleave.
	//Every wait has a guard counter: this must never be able to block the main loop forever.
    /* Direct ELM writer and HAL interrupt TX share USART2. Reserve it
     * before waiting for an already running HAL transfer to complete. */
    uint8_t uart_link_send(const uint8_t *frame, uint8_t len) {
        uint32_t mask, guard;
        uint8_t ok = 1u;
        if (!frame || !len || len > UART_BUFFER_SIZE || __get_IPSR() != 0u) return 0u;
        mask = __get_PRIMASK(); __disable_irq();
        if (uart2_direct_tx_owned) { __set_PRIMASK(mask); return 0u; }
        uart2_direct_tx_owned = 1u;
        __set_PRIMASK(mask);
        guard = 200000u;
        while ((huart2.State == HAL_UART_STATE_BUSY_TX || huart2.State == HAL_UART_STATE_BUSY_TX_RX) && --guard) {}
        if (!guard) { ok = 0u; goto done; }
        guard = 200000u;
        while (!__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) && --guard) {}
        if (!guard) { ok = 0u; goto done; }
        mask = __get_PRIMASK(); __disable_irq();
        if (!uart_rx_reset_idle(&huart2)) {
            __set_PRIMASK(mask);
            ok = 0u;
            goto done;
        }
        __set_PRIMASK(mask);
        CLEAR_BIT(huart2.Instance->CR1, USART_CR1_RE);
        SET_BIT(huart2.Instance->CR1, USART_CR1_TE);
        for (uint8_t i = 0u; i < len; i++) {
            guard = 200000u;
            while (!__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) && --guard) {}
            if (!guard) { ok = 0u; break; }
            huart2.Instance->TDR = frame[i];
        }
        if (ok) {
            guard = 200000u;
            while (!__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) && --guard) {}
            if (!guard) ok = 0u;
        }
        if (!ok) CLEAR_BIT(huart2.Instance->CR1, USART_CR1_TE);
        syncObtained = 0;
        __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF | UART_CLEAR_PEF);
        __HAL_UART_FLUSH_DRREGISTER(&huart2);
        SET_BIT(huart2.Instance->CR1, USART_CR1_RE);
        /* RX was cancelled before direct TX; HAL now owns the new RX descriptor. */
        mask = __get_PRIMASK(); __disable_irq();
        huart2.ErrorCode = HAL_UART_ERROR_NONE;
        if (HAL_UART_Receive_IT(&huart2, &rxBuffer[0], 1) != HAL_OK) ok = 0u;
        __set_PRIMASK(mask);
    done:
        mask = __get_PRIMASK(); __disable_irq();
        uart2_direct_tx_owned = 0u;
        __set_PRIMASK(mask);
        return ok;
    }

#endif
//elm327 function 26/08/2026 - END

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){

	if (( currentTime - lastUartErrorCallback)>1000) onboardLed_red_on();
	lastUartErrorCallback = currentTime;

	/*
	if (huart->ErrorCode & HAL_UART_ERROR_ORE) {
		// Overrun error (full buffer, data loss)
		//onboardLed_red_on();
	}
	if (huart->ErrorCode & HAL_UART_ERROR_FE) {
		// Framing error (08/11/25: when we go to sleep a framing error is arised. why?)
		//onboardLed_red_on();
	}
	if (huart->ErrorCode & HAL_UART_ERROR_NE) {
		// Noise detected
		//onboardLed_red_on();
	}
	if (huart->ErrorCode & HAL_UART_ERROR_PE) {
		// Parity error
		//onboardLed_red_on();
	}

	if (huart->ErrorCode == HAL_UART_ERROR_NONE) {
		// Error not classified
		//onboardLed_red_on();
	}

	if (huart->ErrorCode & HAL_UART_ERROR_DMA) {
		//onboardLed_red_on();
	}

	*/
	pauseUart(huart);
	if ((lowConsumeIsActive==0) || (huart->Instance == USART1)) restartUart(huart);

}
/*
void enter_standby_mode(void) {
	onboardLed_blue_on();
    // Abilita SLEEPDEEP nel registro SCR
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    // Abilita la modalità Standby
    PWR->CR |= PWR_CR_PDDS;

    // Abilita il wake-up pin (opzionale)
    //PWR->CSR |= PWR_CSR_EWUP;

    // Entra in Standby Mode
    __WFI();
}

void resetOtherProcessorsSleepStatus(){
	baccableC2_IsSleeping=0;
	baccableBH_IsSleeping=0;
}

uint8_t getOtherProcessorsSleepingStatus(){
	return (baccableC2_IsSleeping && baccableBH_IsSleeping);
}
*/


/* Stage C2: checked, interrupt-state-preserving enqueue for ABS commands.
 * Do not clear the shared queue: it also carries unrelated functions. */
int uart_try_enqueue_abs(const uint8_t *data, size_t length) {
#if defined(C1baccable)
    if (data == NULL || length == 0u || length > UART_BUFFER_SIZE) return 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    int ok = 0;
    if (tx_queue->count < QUEUE_SIZE) {
        memset(tx_queue->tx_buffer[tx_queue->tail], 0x20, UART_BUFFER_SIZE);
        memcpy(tx_queue->tx_buffer[tx_queue->tail], data, length);
        tx_queue->tail = (tx_queue->tail + 1u) % QUEUE_SIZE;
        tx_queue->count++;
        ok = 1;
    }
    __set_PRIMASK(primask);
    return ok;
#else
    (void)data; (void)length;
    return 0;
#endif
}

/* Atomic admission check for the Stage G restore request.
 * Success means queued, not physically received by BH. */
uint8_t tryAddToUARTSendQueue(const uint8_t *data, size_t length) {
    uint8_t ok=0u;
    uint32_t primask=__get_PRIMASK();
    __disable_irq();
    if (tx_queue->count < QUEUE_SIZE) {
        addToUARTSendQueueDuringInterrupt(data,length);
        ok=1u;
    }
    __set_PRIMASK(primask);
    return ok;
}

void addToUARTSendQueue(const uint8_t *data, size_t length) {
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	addToUARTSendQueueDuringInterrupt(data,length);
	__set_PRIMASK(primask);
}




void addToUARTSendQueueDuringInterrupt(const uint8_t *data, size_t length) {

	if (tx_queue->count < QUEUE_SIZE) {  // Controlla se la coda è piena
		memset(tx_queue->tx_buffer[tx_queue->tail], 0x20, UART_BUFFER_SIZE);
		if (length>UART_BUFFER_SIZE) length= UART_BUFFER_SIZE;
		memcpy(tx_queue->tx_buffer[tx_queue->tail], data, length);
		tx_queue->tail = (tx_queue->tail + 1) % QUEUE_SIZE;
		tx_queue->count++;
	} else {
		// queue full, ignore the request
		onboardLed_red_on();

	}
}


#if(defined(C1baccable))

	void addToUART1SendQueue(const uint8_t *data, size_t length) { //add to queue message to schizzaforte
		uint32_t primask = __get_PRIMASK();
		__disable_irq();
		addToUART1SendQueueDuringInterrupt(data,length);
		__set_PRIMASK(primask);
	}

	void addToUART1SendQueueDuringInterrupt(const uint8_t *data, size_t length) {

		if (tx_queue_uart1->count < QUEUE_SIZE) {  // Controlla se la coda è piena
			memset(tx_queue_uart1->tx_buffer[tx_queue_uart1->tail], 0x20, UART1_BUFFER_SIZE);
			if (length>UART1_BUFFER_SIZE) length= UART1_BUFFER_SIZE;
			memcpy(tx_queue_uart1->tx_buffer[tx_queue_uart1->tail], data, length);
			tx_queue_uart1->tail = (tx_queue_uart1->tail + 1) % QUEUE_SIZE;
			tx_queue_uart1->count++;
		} else {
			// queue full, ignore the request
			onboardLed_red_on();

		}
	}

	void processUART1(void) { //process uart toward schizzaForte
		if(currentTime<TIMING__ALL___SERIAL_IGNORE_WINDOW_MS) return;  //give time to schizzaforte to start
		//process messages to schizzaForte

		if (tx_queue_uart1->count == 0) return; // queue empty

		if(currentTime-last_sent_serial_to_schizzaForte_msg_time>TIMING__C1____SCHIZZAFORTE_SERIAL_TIMEOUT_REPLY_MS){ //each 250msec send a message (so that we left the time to receiver to reply)
    		last_sent_serial_to_schizzaForte_msg_time=currentTime;

    		CLEAR_BIT(huart1.Instance->CR1, USART_CR1_RE);	// disable RX
    		SET_BIT(huart1.Instance->CR1, USART_CR1_TE);	// enable TX
    		__HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_TCF); //clear any error
    		//onboardLed_red_on();
			/* Never overwrite the snapshot while an interrupt-driven TX is active. */
			if (huart1.State != HAL_UART_STATE_READY &&
			    huart1.State != HAL_UART_STATE_BUSY_RX) {
				CLEAR_BIT(huart1.Instance->CR1, USART_CR1_TE);
				SET_BIT(huart1.Instance->CR1, USART_CR1_RE);
				return;
			}
			memcpy(uart1_tx_snapshot, tx_queue_uart1->tx_buffer[tx_queue_uart1->head], UART1_BUFFER_SIZE);
			if (HAL_UART_Transmit_IT(&huart1, uart1_tx_snapshot, UART1_BUFFER_SIZE) == HAL_OK){
				// update head index in a circular way
				tx_queue_uart1->head = (tx_queue_uart1->head + 1) % QUEUE_SIZE;
				tx_queue_uart1->count--;

			}else{
				/* No TX was started: restore half-duplex receive direction. */
				CLEAR_BIT(huart1.Instance->CR1, USART_CR1_TE);
				SET_BIT(huart1.Instance->CR1, USART_CR1_RE);
				/* Re-arm only when no receive is currently active. */
				if (huart1.State == HAL_UART_STATE_READY) {
					HAL_UART_Receive_IT(&huart1, &rxBufferUart1[0], 1);
				}
				onboardLed_red_on();
			}
		}

	}

#endif

void processUART(void) {

	if(currentTime<TIMING__ALL___SERIAL_IGNORE_WINDOW_MS) return;  //too early

	#if(defined(C1baccable))
		if(lowConsumeIsActive)  return; //other chips are under reset, avoid to send messages   //|| (allProcessorsWakeupTime<TIMING__C1____DELAY_BEFORE_SERIAL_PROCESS_AFTER_OTHER_CHIP_WAKE_MS))
		/* Only schedule ABS UART traffic when normal C1 UART processing is allowed. */
		abs_c1_service_poll(currentTime);
		//onboardLed_blue_on();

		if(currentTime-allProcessorsWakeupTime>TIMING__C1____DELAY_BEFORE_OTHER_CHIP_STATUS_REQUEST_MS){
			if(currentTime-lastMsgSentToC2Time>TIMING__C1____C2_STATUS_REQUEST_TIMEOUT_MS){ //1010
				lastMsgSentToC2Time=currentTime;
				//get status from C2
				//send request thu serial line
				uint8_t tmpArr1[2]={C2BusID,C2cmdGetStatus};
				addToUARTSendQueue(tmpArr1, 2); //commented for test
				onboardLed_blue_on();
			}
			if(currentTime-lastMsgSentToBHTime>TIMING__C1____BH_STATUS_REQUEST_TIMEOUT_MS){ //1260
				lastMsgSentToBHTime=currentTime;
				//get status from BH
				//send request thu serial line
				uint8_t tmpArr2[1]={BhBusIDgetStatus};
				addToUARTSendQueue(tmpArr2, 1);
				onboardLed_blue_on();
			}
		}
	#endif

	if (tx_queue->count == 0) return; // queue empty

    //if we are C2 or BH baccable, we can reply only if we received a message directed to us few milliseconds ago
	#if (defined(C2baccable) || defined(BHbaccable))

		if(currentTime-weCanSendAMessageReply<TIMING__C2_BH_SERIAL_TIMEOUT_REPLY_MS){ //if less than 200msec has passed since last message received from master baccable, send a message

			if( __HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) ){ //&& ((huart2.State == HAL_UART_STATE_READY || huart2.State == HAL_UART_STATE_BUSY_RX))
				if(tx_queue->tx_buffer[tx_queue->head][1]==C1cmdLaneSingleTap) onboardLed_red_on(); //just for test
				if (uart2_hal_tx_start(tx_queue->tx_buffer[tx_queue->head])){

					// update head index in a circular way
					tx_queue->head = (tx_queue->head + 1) % QUEUE_SIZE;
					tx_queue->count--;
					//onboardLed_blue_on();
				}else{
					onboardLed_red_on();
				}
			}else{
				onboardLed_red_on();
			}
		}
	#endif

    //if we are C1 baccable, we can send a message each 250msec
	#if defined(C1baccable)
        /* Drop only obsolete ABS commands at the queue head. Do not touch
           non-ABS traffic or reorder the shared FIFO. */
        while (tx_queue->count != 0u &&
               tx_queue->tx_buffer[tx_queue->head][0] == C2BusID &&
               (tx_queue->tx_buffer[tx_queue->head][1] == C2cmdAbsFaultsStart ||
                tx_queue->tx_buffer[tx_queue->head][1] == C2cmdAbsFaultsGet) &&
               !abs_c1_service_tx_is_current(tx_queue->tx_buffer[tx_queue->head][1],
                   tx_queue->tx_buffer[tx_queue->head][2],
                   tx_queue->tx_buffer[tx_queue->head][3])) {
            tx_queue->head = (tx_queue->head + 1u) % QUEUE_SIZE;
            tx_queue->count--;
        }
        if (tx_queue->count == 0u) return;

    	if(currentTime-last_sent_serial_msg_time>TIMING__C1____SERIAL_TIMEOUT_REPLY_MS){ //each 250msec send a message (so that we left the time to receiver to reply)
			last_sent_serial_msg_time=currentTime;
			if( __HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) ){ //&& ((huart2.State == HAL_UART_STATE_READY || huart2.State == HAL_UART_STATE_BUSY_RX))
				if (uart2_hal_tx_start(tx_queue->tx_buffer[tx_queue->head])){
                if (tx_queue->tx_buffer[tx_queue->head][0] == C2BusID &&
                    (tx_queue->tx_buffer[tx_queue->head][1] == C2cmdAbsFaultsStart ||
                     tx_queue->tx_buffer[tx_queue->head][1] == C2cmdAbsFaultsGet)) {
                    abs_c1_service_tx_started(tx_queue->tx_buffer[tx_queue->head][1],
                        tx_queue->tx_buffer[tx_queue->head][2],
                        tx_queue->tx_buffer[tx_queue->head][3], currentTime);
                }
                
					//save time
					switch(tx_queue->tx_buffer[tx_queue->head][0]){
						case C2BusID:
						case C2_Bh_BusID: //only C2 is allowed to reply when this msg is received
						case AllResetFaults: //only C2 is allowed to reply when this msg is received
							lastMsgSentToC2Time=currentTime;
							break;
						case BhBusIDparamString:
						case BhBusIDgetStatus:
						case BhBusChimeRequest:
						case BhBusID:
							lastMsgSentToBHTime=currentTime;
							break;
						default:
					}

					// update head index in a circular way
					tx_queue->head = (tx_queue->head + 1) % QUEUE_SIZE;
					tx_queue->count--;
				}else{
					onboardLed_red_on();


				}
			}
		}



	#endif
}
