/*
 * C1baccableFunctions.c
 *
 *  Created on: May 2, 2025
 *      Author: GauchoHP
 *      Common functions for C1baccable
 */
#include "stageg_ign_diag.h"
#include "processingExtendedMessage.h" /* Optional ECU status page API */
#include "functions_C1baccable.h"
#include "functions_C1baccable.h"
#include "uart.h"
#include "uart.h"
#include <string.h>
#include "functions_C1baccable.h"
#include "functions_C1baccable.h" 	//<> changed by @netzmark
#include "functions_Common.h" 		// added by @netzmark
 
	extern uint32_t _end;      // Fine della sezione .bss (fornito dal linker)
	extern uint32_t _estack;   // Top dello stack (fornito dal linker)
	//uint8_t tmpArr3[2]={C2BusID,C2cmdRaceMaskDefault};
 
	/* @netzmark: to create multi parameters screens */
#include "uds_parameters.h"
	/* @netzmark: END */
 
#if defined(C1baccable)

/* @Shadin: DNA Journal core integrated into Gaucho C1 functions.
 * Preserve journal record format, CRC, flash addresses and commit sequence.
 * Original implementation and comments retained below.
 */
#include "dna_journal.h"
#include <string.h>
/* Record layout, little endian, 16 bytes:
 * magic(2), sequence(4), mode(2), complement(2), checksum(2), reserved(2), commit(2).
 * Commit is programmed LAST; erased flash is 0xFF.
 */
#define MAGIC 0xD9A1u
#define COMMIT 0xA55Au
static int shadin_dnaj_allowed(uint8_t m) { return m==0x00 || m==0x08 || m==0x10 || m==0x30; }
/* @Shadin: Formatting only; journal record layout and algorithms unchanged. */
static uint16_t shadin_dnaj_get16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
static uint32_t shadin_dnaj_get32(const uint8_t *p) {
    return (uint32_t)shadin_dnaj_get16(p) | ((uint32_t)shadin_dnaj_get16(p+2)<<16);
}
static uint16_t shadin_dnaj_crc(const uint8_t *p) {
    uint16_t c=0xFFFF;
    for(unsigned i=0;i<10;i++) {
        c^=(uint16_t)p[i]<<8;
        for(unsigned b=0;b<8;b++) c=(c&0x8000) ? (uint16_t)((c<<1)^0x1021) : (uint16_t)(c<<1);
    }
    return c;
}
static int shadin_dnaj_read_record(const dna_flash_ops *o,uint32_t a,uint32_t *seq,uint8_t *mode) {
    uint8_t b[16];
    if(o->read(a,b,16))return -1;
    if(shadin_dnaj_get16(b)!=MAGIC||shadin_dnaj_get16(b+14)!=COMMIT)return 0;
    uint16_t m=shadin_dnaj_get16(b+6);
    if(m>255||!shadin_dnaj_allowed((uint8_t)m)||shadin_dnaj_get16(b+8)!=(uint16_t)(~m & 0xffff)||shadin_dnaj_get16(b+10)!=shadin_dnaj_crc(b))return 0;
    *seq=shadin_dnaj_get32(b+2);
    *mode=(uint8_t)m;
    return 1;
}
static int shadin_dnaj_newest(const dna_flash_ops *o,dna_journal_state *s,uint32_t *page,uint32_t *slot) {
    s->valid=0;
    s->sequence=0;
    s->mode=0xFF;
    for(unsigned p=0;p<2;p++) {
        uint32_t base=p ? DNA_JOURNAL_PAGE_B : DNA_JOURNAL_PAGE_A;
        for(unsigned i=0;i<DNA_JOURNAL_PAGE_SIZE/DNA_JOURNAL_SLOT_SIZE;i++) {
            uint32_t seq;
            uint8_t mode;
            int r=shadin_dnaj_read_record(o,base+i*16,&seq,&mode);
            if(r<0)return -1;
            if(r==1&&(!s->valid||(int32_t)(seq-s->sequence)>0)) {
                s->valid=1;
                s->sequence=seq;
                s->mode=mode;
                *page=base;
                *slot=i;
            }
        }
    }
    return 0;
}
int dna_journal_load(const dna_flash_ops *o,dna_journal_state *s) {
    uint32_t p=0,i=0;
    if(!o||!s||!o->read)return -1;
    return shadin_dnaj_newest(o,s,&p,&i);
}
static int shadin_dnaj_blank(const dna_flash_ops *o,uint32_t a) {
    uint8_t b[16];
    if(o->read(a,b,16))return -1;
    for(unsigned i=0;i<16;i++)if(b[i]!=0xff)return 0;
    return 1;
}
static int shadin_dnaj_write_record(const dna_flash_ops *o,uint32_t a,uint32_t seq,uint8_t mode) {
    uint8_t b[16];
    memset(b,0xff,sizeof b);
    b[0]=MAGIC&255;
    b[1]=MAGIC>>8;
    for(unsigned i=0;i<4;i++)b[2+i]=(uint8_t)(seq>>(i*8));
    b[6]=mode;
    b[7]=0;
    b[8]=(uint8_t)~mode;
    b[9]=0xff;
    b[10]=shadin_dnaj_crc(b)&255;
    b[11]=shadin_dnaj_crc(b)>>8;
    for(unsigned i=0;i<7;i++)if(o->program_halfword(a+i*2,shadin_dnaj_get16(b+i*2)))return -1;
    return o->program_halfword(a+14,COMMIT);
}
int dna_journal_save(const dna_flash_ops *o,dna_journal_state *s,uint8_t mode) {
    uint32_t page=0,slot=0;
    if(!o||!s||!o->read||!o->erase_page||!o->program_halfword||!shadin_dnaj_allowed(mode))return -1;
    if(shadin_dnaj_newest(o,s,&page,&slot))return -1;
    if(s->valid&&s->mode==mode)return 0;
    uint32_t dest=0;
    if(s->valid) {
        for(unsigned i=slot+1;i<128;i++) {
            int r=shadin_dnaj_blank(o,page+i*16);
            if(r<0)return -1;
            if(r==1) {
                dest=page+i*16;
                break;
            }
        }
    }
    else {
        for(unsigned p=0;p<2&&!dest;p++) {
            uint32_t base=p ? DNA_JOURNAL_PAGE_B : DNA_JOURNAL_PAGE_A;
            for(unsigned i=0;i<128;i++) {
                int r=shadin_dnaj_blank(o,base+i*16);
                if(r<0)return -1;
                if(r==1) {
                    dest=base+i*16;
                    break;
                }
            }
        }
    }
    if(!dest) {
        uint32_t other=(page==DNA_JOURNAL_PAGE_A) ? DNA_JOURNAL_PAGE_B : DNA_JOURNAL_PAGE_A;
        if(o->erase_page(other))return -1;
        dest=other;
    }
    uint32_t next=s->valid ? s->sequence+1 : 1;
    if(shadin_dnaj_write_record(o,dest,next,mode))return -1;
    dna_journal_state verify;
    if(dna_journal_load(o,&verify)||!verify.valid||verify.sequence!=next||verify.mode!=mode)return -1;
    *s=verify;
    return 1;
}

/* @Shadin: End DNA Journal core. */


/* @Shadin: Transactional C1 settings storage integrated from config_store.c.
 * Original storage behavior, Flash addresses, format and comments preserved.
 * Keep config_store.h as the shared public interface.
 */
/* Stage G transactional C1 settings storage. Two 2-KiB pages, legacy read-only fallback.
 * Never erase the old settings page (0x0801F800). Both copies remain independently valid.
 * Commit halfword is written last. CRC covers all 34 parameter halfwords.
 */
#include "stm32f0xx_hal.h"
#include <stddef.h>
#define CFG_A 0x0801C800u
#define CFG_B 0x0801D000u
#define CFG_LEGACY 0x0801F800u
#define CFG_MAGIC 0xC391u
#define CFG_FORMAT 0x0001u
#define CFG_COMMIT 0xA55Au
#define CFG_COMMIT_OFFSET 160u
#define CFG_DATA_OFFSET 16u
#define CFG_SEQ_OFFSET 4u
#define CFG_CRC_OFFSET 8u
#define CFG_FMT_OFFSET 2u
#define CFG_COUNT_OFFSET 10u
#define CFG_TOTAL 34u
#define HW(a) (*(volatile const uint16_t *)(uintptr_t)(a))
static uint16_t shadin_cfg_crc16(const uint16_t *data) {
    uint16_t crc=0xFFFFu;
    for(unsigned i=0;i<CFG_TOTAL;i++) for(unsigned j=0;j<2;j++) {
        uint8_t byte=(uint8_t)(data[i]>>(8u*j)); crc ^= (uint16_t)byte<<8;
        for(unsigned k=0;k<8;k++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1);
    }
    return crc;
}
static int shadin_cfg_read_page(uint32_t base, uint16_t *out, uint32_t *seq) {
    if(HW(base)!=CFG_MAGIC || HW(base+CFG_FMT_OFFSET)!=CFG_FORMAT ||
       HW(base+CFG_COUNT_OFFSET)!=CFG_TOTAL || HW(base+CFG_COMMIT_OFFSET)!=CFG_COMMIT) return 0;
    uint16_t values[CFG_TOTAL];
    for(unsigned i=0;i<CFG_TOTAL;i++) values[i]=HW(base+CFG_DATA_OFFSET+4u*i);
    if(shadin_cfg_crc16(values)!=HW(base+CFG_CRC_OFFSET)) return 0;
    if(out) for(unsigned i=0;i<CFG_TOTAL;i++) out[i]=values[i];
    if(seq) *seq=(uint32_t)HW(base+CFG_SEQ_OFFSET) | ((uint32_t)HW(base+CFG_SEQ_OFFSET+2u)<<16);
    return 1;
}
static uint32_t shadin_cfg_active_page(uint32_t *sequence) {
    uint32_t a=0,b=0; int va=shadin_cfg_read_page(CFG_A,NULL,&a), vb=shadin_cfg_read_page(CFG_B,NULL,&b);
    if(!va && !vb) {if(sequence) *sequence=0; return 0;}
    uint32_t base=(!vb || (va && (int32_t)(a-b)>=0))?CFG_A:CFG_B;
    if(sequence) *sequence=(base==CFG_A)?a:b;
    return base;
}
/* Cache the validated active page for read-only menu access. Previously every
 * readFromFlash() revalidated BOTH 34-word copies with CRC16; the menu's
 * 33-field change check could therefore perform 66 full CRC scans per click.
 * Invalidate after a save so subsequent reads see the newly committed copy.
 */
static uint8_t read_cache_valid;
static uint32_t read_cache_base;
uint16_t config_store_read(uint8_t index) {
    if(index>=CFG_TOTAL) return 0xFFFFu;
    if(!read_cache_valid) {
        read_cache_base=shadin_cfg_active_page(NULL);
        read_cache_valid=1u;
    }
    return HW(read_cache_base ? read_cache_base+CFG_DATA_OFFSET+4u*index : CFG_LEGACY+4u*index);
}
static int shadin_cfg_program(uint32_t address, uint16_t value) {
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,address,value)==HAL_OK && HW(address)==value;
}
uint8_t config_store_save(const uint16_t *values, uint8_t count) {
    if(!values || count!=CFG_TOTAL) return 251;
    uint32_t seq=0, active=shadin_cfg_active_page(&seq);
    uint32_t target=(active==CFG_A)?CFG_B:CFG_A;
    // Do not touch the valid copy until the new copy has been committed and verified.
    FLASH_EraseInitTypeDef erase={0}; uint32_t page_error=0;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.PageAddress=target;
    erase.NbPages=1;
    if(HAL_FLASH_Unlock()!=HAL_OK) return 254;
    if(HAL_FLASHEx_Erase(&erase,&page_error)!=HAL_OK) {HAL_FLASH_Lock();return 254;}
    uint32_t next=seq+1u;
    int ok=shadin_cfg_program(target,CFG_MAGIC) && shadin_cfg_program(target+CFG_FMT_OFFSET,CFG_FORMAT) &&
           shadin_cfg_program(target+CFG_SEQ_OFFSET,(uint16_t)next) &&
           shadin_cfg_program(target+CFG_SEQ_OFFSET+2u,(uint16_t)(next>>16)) &&
           shadin_cfg_program(target+CFG_COUNT_OFFSET,CFG_TOTAL) &&
           shadin_cfg_program(target+CFG_CRC_OFFSET,shadin_cfg_crc16(values));
    for(unsigned i=0;i<CFG_TOTAL && ok;i++) ok=shadin_cfg_program(target+CFG_DATA_OFFSET+4u*i,values[i]);
    if(ok) ok=shadin_cfg_program(target+CFG_COMMIT_OFFSET,CFG_COMMIT);
    HAL_FLASH_Lock();
    if(!ok) return 255;
    uint16_t verify[CFG_TOTAL]; uint32_t check=0;
    if(!shadin_cfg_read_page(target,verify,&check) || check!=next) return 252;
    for(unsigned i=0;i<CFG_TOTAL;i++) if(verify[i]!=values[i]) return 252;
    /* A successful commit switches the active copy: refresh on next read. */
    read_cache_valid=0u;
    return 0;
}
/* @Shadin: End transactional settings storage. */

 
	/* @netzmark: to create multi parameters screens */
#if defined(C1baccable)
    uint32_t lastDisplayUpdateTime = 0;
    uint16_t dynamicInterval = 500;
    uint8_t  group_step = 0;
    uint8_t  actuallyRequestedUdsId = 0;
    uint8_t  requestedId = 0;
    uint8_t  idLeft = 0;
    uint8_t  idRight = 0;

#ifndef LARGE_DISPLAY
    /* Fallback cache for clean configuration build 
     * to can compile without #large_display
     * */
    float uds_values_cache[MAX_UDS_PARAMS] = {0.0f};

    /* Standard type fallback array matching header declaration with UDS_STOP_ID guard */
    const uds_params_group_element uds_params_groups_array[] = {
        { 200, {0, 0, 0, 0} }
    };
#endif
#endif
    /* @netzmark: END */
 
	void C1baccableInitCheck(){
		#ifndef DISABLE_LOW_CONSUME
			lowConsume_init();
		#endif
		immobilizerEnabled = (uint8_t)readFromFlash(1);  //parameter1 stored in ram, so that we can get it. By default Immo is enabled
		if(immobilizerEnabled) executeDashboardBlinks=2; //shows the user that the immobilizer is active (or not)
		function_smart_disable_start_stop_enabled=(uint8_t)readFromFlash(2);  //parameter2 stored in ram, so that we can get it. By default S&S is enabled
		#if defined(LED_STRIP_CONTROLLER_ENABLED)
			function_led_strip_controller_enabled=(uint8_t)readFromFlash(3); //By default led is disabled
		#else
			/* @Shadin: Do not activate a persisted LED setting in a build without LED support. */
			function_led_strip_controller_enabled=0;
		#endif
		function_shift_indicator_enabled=(uint8_t)readFromFlash(4); //By default it is disabled
		shift_threshold= (uint16_t)readFromFlash(5);
		function_ipc_my23_is_installed=	(uint8_t)readFromFlash(6);
		function_route_msg_enabled=(uint8_t)readFromFlash(7);
		function_dyno_mode_master_enabled=(uint8_t)readFromFlash(8);
		function_acc_virtual_pad_enabled=(uint8_t)readFromFlash(9);
		function_front_brake_forcer_master=(uint8_t)readFromFlash(10);
		function_4wd_disabler_enabled=(uint8_t)readFromFlash(11);
		function_remote_start_Enabled=(uint8_t)readFromFlash(12);
		function_clear_faults_enabled=(uint8_t)readFromFlash(13);
		function_esc_tc_customizator_enabled=(uint8_t)readFromFlash(14);
		function_read_faults_enabled=(uint8_t)readFromFlash(15);
		function_is_diesel_enabled=(uint8_t)readFromFlash(16);
		total_pages_in_params_setup_dashboard_menu = function_is_diesel_enabled ? total_pages_in_dashboard_menu_diesel : total_pages_in_dashboard_menu_gasoline;
 
		function_regeneration_alert_enabled=(uint8_t)readFromFlash(17);
		launch_torque_threshold= (uint16_t)readFromFlash(18);
		function_seatbelt_alarm_enabled= (uint16_t)readFromFlash(19);
		function_pedal_booster_enabled= (uint16_t)readFromFlash(20);
		function_disable_odometer_blink= (uint16_t)readFromFlash(21);
		function_show_race_mask= (uint16_t)readFromFlash(22);
		function_park_mirror= (uint16_t)readFromFlash(23);
		function_acc_autostart= (uint16_t)readFromFlash(24);
		function_close_windows_with_door_lock=(uint16_t)readFromFlash(25);
		function_open_windows_with_door_lock=(uint16_t)readFromFlash(26);
		HAS_function_enabled=(uint16_t)readFromFlash(27);
		QV_exhaust_flap_function_enabled=(uint16_t)readFromFlash(28);
		pedal_map_power=(int8_t)(uint8_t)readFromFlash(29);
		parkSensorsMuteFunctionEnabled=(uint8_t)readFromFlash(30);
		snifferFunctionEnabled=(uint8_t)readFromFlash(31); //sniffer function 24/08/2026
		//started here, at boot, so that snifferInUse blocks low consume straight away: lowConsume_process() runs
		//at the top of C1baccablePeriodicCheck() and its inactivity timeout (3500ms) expires 2ms BEFORE the
		//instructSlaveBoardsTriggerEnabled block below (3502ms), so anything that only sets snifferInUse down
		//there would lose that race and C1 would sleep (and pause the inter chip uart) first.
		//The detection window itself is realigned to the slaves later, see that same block. //sniffer function 24/08/2026
		if(snifferFunctionEnabled) snifferStart();
		//elm327 function 26/08/2026 - same lifecycle, but starting it only brings the usb port up and starts the
		//detection window: the baccable keeps working normally until a host actually enumerates us (see
		//elm327CheckActivationTimeout()). The two modes share the single usb cdc port, so they are mutually
		//exclusive: the setup menu already clears one when the other is enabled, this is just the safety net.
		elm327FunctionEnabled=(uint8_t)readFromFlash(32);
		dnaMemoryFunctionEnabled=(uint8_t)readFromFlash(33);
		if(dnaMemoryFunctionEnabled>1) dnaMemoryFunctionEnabled=0;
		dna_stageg_init(dnaMemoryFunctionEnabled);
		/* @Shadin: Reverse Mute uses the same Boolean Flash convention (0/1)
		 * as other ON/OFF features. Keep parameter IDs in numeric order. */
		reverseMuteFunctionEnabled=(uint8_t)readFromFlash(34);
		if(elm327FunctionEnabled && snifferFunctionEnabled==0) elm327Start();
		//arise trigger to notify enabled functions to slave boards with dedicated messages,after some seconds
		allProcessorsWakeupTime=currentTime;
		instructSlaveBoardsTriggerEnabled=1;
 
		readShownParamsFromFlash();
	}
 
	void setSchizzaforteMap(uint8_t map){
		uint8_t tmpArr0[UART1_BUFFER_SIZE]={'#',0xb6,};
 
		// Schizzaforte Operative commands:
		//				First Character: #
		//				Second Character:	0x00=set Bypass map, 0x49=set All weather map, 0x92=set Natural map, 0xdb=set Dynamic map,
		//													0x24=set Race map, 0x6d=get current Map, 0xb6=define a Map
		//				Next Characters:	In case of command 0xb6(Define a Map), more 7 chars are sent:
		//													3)map to write (0x00=set Bypass map, 0x49=set All weather map, 0x92=set Natural map,
		//														0xdb=set Dynamic map, 0x24=set Race map),
		//													4)polynomialcurve[][0]
		//													5)polynomialcurve[][1]
		//													6)polynomialcurve[][2]
		//													7)polynomialcurve[][3]
		//													8)polynomialcurve[][4]
		//													9)checksum (calculated)
		// Current selected Map is sent as Reply: 0x10=Bypass, 0x59=All weather, 0xa2=Natural, 0xeb=Dynamic, 0x34=Race
 
		//map: 2=Bypass, 3=All Weather Map, 4=Natural Map, 5=Dynamic Map, 6=Race Map
 
 
 
		switch(map){
			case 3: //A map
				//prepare power amplification factor
				//input range -10+10, output range -16+16
				if (pedal_map_power >= 0){
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 1.6f+0.5f);
				}else{
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 0.6f-0.5f);
				}
 
				tmpArr0[2]=0x49;	//A map
				tmpArr0[3]=96+pedal_map_power_adapted;	//amplitude
				tmpArr0[4]=128;		//width
				tmpArr0[5]=154;		//position
				break;
			case 4: //N map
				//prepare power amplification factor
				//input range -10+10, output range -16+32
				if (pedal_map_power >= 0) {
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 3.2f + 0.5f);
				} else {
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 1.6f - 0.5f);
				}
 
				tmpArr0[2]=0x92;
				tmpArr0[3]=128+pedal_map_power_adapted;		//amplitude
				tmpArr0[4]=128;		//width
				tmpArr0[5]=154;		//position
				break;
			case 5: //D Map
				//prepare power amplification factor
				//input range -10+10, output range -32+20
				if (pedal_map_power >= 0) {
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 2.0f + 0.5f);
				} else {
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 3.2f - 0.5f);
				}
				tmpArr0[2]=0xdb;
				tmpArr0[3]=192+pedal_map_power_adapted;		//amplitude
				tmpArr0[4]=128;		//width
				tmpArr0[5]=154;		//position
				break;
			case 6: //R map
				//prepare power amplification factor
				//input range -10+10, output range -18+24
				if (pedal_map_power >= 0) {
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 2.4f + 0.5f);
				} else {
					pedal_map_power_adapted = (int8_t)(pedal_map_power * 1.8f - 0.5f);
				}
				tmpArr0[2]=0x24;
				tmpArr0[3]=230+pedal_map_power_adapted;		//amplitude
				tmpArr0[4]=128;		//width
				tmpArr0[5]=154;		//position
				break;
			case 2: //bypass
			default: //bypass
				tmpArr0[2]=0x00;
				break;
		}
 
		tmpArr0[8]=calculateCRC(tmpArr0,UART1_BUFFER_SIZE); //calculate checksum
 
		//send message
		addToUART1SendQueue(tmpArr0, 9);
 
	}
 
 
	//elm327 function 26/08/2026 - BEGIN
	//Only ram is touched here, so this is safe from anywhere: bringing usb up blocks for a few milliseconds and
	//is deferred to elm327UsbStartIfRequested(), exactly like snifferStart() does.
	void elm327Start(void){
		elm327InUse=1;
		elm327ActivationConfirmed=0;
		elm327ActivationTime=currentTime;	//starts the host detection window (SNIFFER_ACTIVATION_TIMEOUT_MS)
		elm327UsbStartRequested=1;
	}
 
	//Leaves the mode for good: interpreter off, bridge disarmed on all three chips, usb port down.
	//elm327_set_enabled(0) does all of that (see elm327.c), including usb_device_detach().
	void elm327Stop(void){
		uint8_t wasConfirmed=elm327ActivationConfirmed;
		elm327InUse=0;
		elm327ActivationConfirmed=0;
		elm327UsbStartRequested=0;
		if(wasConfirmed){
			elm327_set_enabled(0);
		}else{
			usb_device_detach(); //never confirmed: the port was only up waiting for a host, nothing else to undo
		}
		//while the mode was running we were not watching the can bus at all, so the last received message would
		//look minutes old and low consume would reset the other two chips the moment we resume. Start the
		//inactivity count from now instead.
		lastReceivedCanMsgTime=currentTime;
		//the slaves spent that time serving the bridge: resend them every function status, as after a wake up
		allProcessorsWakeupTime=currentTime;
		instructSlaveBoardsTriggerEnabled=1;
	}
 
	//Brings the usb port up. Called from the main loop only: usb_device_attach() waits on HAL_Delay().
	void elm327UsbStartIfRequested(void){
		if(elm327UsbStartRequested==0) return;
		elm327UsbStartRequested=0;
		usb_device_attach(); //idempotent, and it selects the elm327 descriptors on its own
	}
 
	//Called every main loop iteration while the function is on. Mirrors snifferCheckActivationTimeout(), but
	//the interpreter is switched on only once a host has really enumerated us: until then the baccable must
	//keep working normally, because turning ELM327 on freezes the dashboard and takes over the serial line.
	void elm327CheckActivationTimeout(void){
		if(elm327InUse==0) return;
		if(hUsbDeviceFS.dev_state==USBD_STATE_CONFIGURED){
			if(elm327ActivationConfirmed==0){
				elm327ActivationConfirmed=1;
				elm327_set_enabled(1); //only NOW the baccable steps aside and the bridge is armed
			}
			elm327ActivationTime=currentTime;
			return;
		}
		//usb suspend guard 25/10/2026 - dev_state has left CONFIGURED. If the host had already enumerated us and
		//then went quiet (a routine PC side usb suspend, or an unplug: indistinguishable here, no vbus sensing),
		//hold the mode for ELM327_SUSPEND_HOLD_MS before giving up. During the hold elm327InUse stays 1, so
		//lowConsume keeps blocking sleep on its own - no other change is needed. If instead the host never
		//configured us at all, keep the short SNIFFER_ACTIVATION_TIMEOUT_MS give up window as before.
		uint32_t graceWindow = elm327ActivationConfirmed ? ELM327_SUSPEND_HOLD_MS : SNIFFER_ACTIVATION_TIMEOUT_MS;
		if(currentTime-elm327ActivationTime<graceWindow) return; //still within the grace window
		elm327Stop(); //host never showed up, or stayed away past the guard: back to being a plain baccable
	}
	//elm327 function 26/08/2026 - END
 
	//exhaust valves memory 11/10/2026 - double click on the release button / menu item: toggles what the user wants.
	//With the engine running it is applied at once (QV: UDS override sequence, chinese valves: radiocontrol button);
	//with the engine off it is only remembered: C1baccablePeriodicCheck applies it as soon as the engine rotates.
	//applies exhaustValvesOpenRequested, only with the engine running (with the engine off it is applied when it rotates).
	//reassert=1: the chinese valves button is pressed even if they are believed to be already in that position (the
	//drive style may have changed while baccable could not see it) //exhaust valves DR 11/10/2026
	static void exhaustValvesApply(uint8_t reassert){
		if(currentRpmSpeed<=EXHAUST_VALVES_ENGINE_RUNNING_RPM) return; //engine off or cranking: applied when it runs //exhaust valves rpm 400 11/10/2026
		if(exhaustValvesOpenRequested){
			if(ForceQVexhaustValveOpened==0) ForceQVexhaustValveOpened=1; //start override sequence
			if(chineseValveIsOpened==0 || reassert) ChineseExhaustValveRequest='O'; //open request
		}else{
			if(ForceQVexhaustValveOpened!=0) ForceQVexhaustValveOpened=4; //return control to ecu
			if(chineseValveIsOpened || reassert) ChineseExhaustValveRequest='C'; //close request
		}
	}

	void exhaustValvesToggle(void){
		exhaustValvesOpenRequested=!exhaustValvesOpenRequested;
		exhaustValvesApply(0);
	}

	//exhaust valves DR 11/10/2026 - function value 2 ("Exhaust Flap DR"): called for every drive style message (0x384).
	//When the drive style changes the valves follow it: Dynamic or Race open, Natural or All Weather close. With the engine
	//off it is only remembered (applied when the engine rotates, see C1baccablePeriodicCheck). After the engine started
	//again (exhaustValvesReassert) the first drive style received sets the valves again even if it did not change: it may
	//have changed while baccable could not see it. The double click / menu toggle still works, until the next change.
	void exhaustValvesOnDNAmode(uint8_t mode){
		if(QV_exhaust_flap_function_enabled!=2){
			exhaustValvesDNAmode=0xFF; //when value 2 is selected, the current drive style is applied at once
			exhaustValvesReassert=0;
			return;
		}
		uint8_t wantOpen;
		if(mode==0x08 || mode==0x30) wantOpen=1;		//Dynamic, Race
		else if(mode==0x00 || mode==0x10) wantOpen=0;	//Natural, All Weather
		else return;									//unknown value: nothing changes
		if(mode==exhaustValvesDNAmode && !exhaustValvesReassert) return;
		uint8_t reassert=exhaustValvesReassert;
		exhaustValvesReassert=0;
		exhaustValvesDNAmode=mode;
		exhaustValvesOpenRequested=wantOpen;
		exhaustValvesApply(reassert);
	}

	void C1baccablePeriodicCheck(){
		// Stage G diagnostics: detect CAN silence while normal BACCAble is active.
		// Passive RAM logging only; never sends CAN or writes flash.
		stageg_ign_diag_check_timeouts(currentTime);
		// Stage G: persistent restore, power-on armed on USB-powered Canable.
		dna_stageg_set_enabled(dnaMemoryFunctionEnabled);
		uint8_t stageg_target=0xFF;
		if(dna_stageg_tick(currentTime,currentRpmSpeed,currentDNAmode,&stageg_target)==1){
			uint8_t letter=(stageg_target==0x10)?'A':(stageg_target==0x00)?'N':(stageg_target==0x08)?'D':(stageg_target==0x30)?'R':0;
			if(letter){uint8_t msg[3]={BhBusID,BHcmdDnaMemoryRestore,letter};
                if(!tryAddToUARTSendQueue(msg,3)) dna_stageg_restore_enqueue_failed();
            }
		}

		#ifndef DISABLE_LOW_CONSUME
			lowConsume_process();
		#endif
 
		if(QV_exhaust_flap_function_enabled){
 
			// Timeout management for chinese valves radiocontrol
			if (exhaustValveMosfetCommandTime) {
				if(currentTime - exhaustValveMosfetCommandTime > 1000){ //if timeout has passed
					HAL_GPIO_WritePin(Q10mosfet, GPIO_PIN_RESET); //open the transistor (release the button)
					HAL_GPIO_WritePin(Q11mosfet, GPIO_PIN_RESET); //open the transistor (release the button)
					exhaustValveMosfetCommandTime = 0;
				}
			}
 
			// if a request to press the button on the radiocontrol was made
			if (ChineseExhaustValveRequest){ //O=open, C=close, 0x00=none
			    HAL_GPIO_WritePin(Q10mosfet_Port,(ChineseExhaustValveRequest == 'O') ? Q10mosfet_Pin : Q11mosfet_Pin,GPIO_PIN_SET); //close transistor (press button)
			    chineseValveIsOpened = (ChineseExhaustValveRequest == 'O'); //store valves status
			    exhaustValveMosfetCommandTime = currentTime; //store time when button was pressed
			    ChineseExhaustValveRequest = 0; //mark the request as executed
			}
 
			// if valve was open and engine had been shutted off
			if (chineseValveIsOpened && currentRpmSpeed == 0){
			    HAL_GPIO_WritePin(Q11mosfet, GPIO_PIN_SET); //close transistor
			    exhaustValveMosfetCommandTime = currentTime; //store time when button was pressed
			    chineseValveIsOpened = 0; //update valves status
			}
 
 
			//exhaust valves memory 11/10/2026 - with the engine off the valves are closed (above for the chinese valves, in
			//the QV sequence below), so that no message is sent on the bus with the car off, but exhaustValvesOpenRequested
			//is kept: as soon as the engine rotates again the valves are opened again. The chinese valves wait for the
			//radiocontrol button of the closing to be released (exhaustValveMosfetCommandTime==0).
			//exhaust valves DR 11/10/2026 - engine started again: with value 2 the next drive style received sets the valves
			//(open or close) again, and the restore below waits for it (no opening on an old drive style)
			//exhaust valves rpm 400 11/10/2026 - "running" above EXHAUST_VALVES_ENGINE_RUNNING_RPM (not while cranking), "stopped"
			//only at rpm 0, as the closing above: between 1 and 400 rpm nothing changes (no toggling around the threshold)
			if (currentRpmSpeed > EXHAUST_VALVES_ENGINE_RUNNING_RPM && !exhaustValvesEngineRunning){
				exhaustValvesEngineRunning = 1;
				if (QV_exhaust_flap_function_enabled == 2) exhaustValvesReassert = 1;
			}
			if (currentRpmSpeed == 0) exhaustValvesEngineRunning = 0;

			if (exhaustValvesOpenRequested && currentRpmSpeed > EXHAUST_VALVES_ENGINE_RUNNING_RPM && !exhaustValvesReassert){ //exhaust valves DR 11/10/2026
				if (ForceQVexhaustValveOpened == 0){
					ForceQVexhaustValveOpened = 1; //start override sequence
				}
				if (chineseValveIsOpened == 0 && ChineseExhaustValveRequest == 0 && exhaustValveMosfetCommandTime == 0){
					ChineseExhaustValveRequest = 'O'; //open request
				}
			}


			switch(ForceQVexhaustValveOpened){
				case 1: //send connection request
				case 2: //send presence
				case 3: //overwrite param
				case 4: //return control to ECU
					if(currentTime-lastSentQVexhaustValveMsgTime>500){ //each 500msec send a message
						if(currentRpmSpeed==0) ForceQVexhaustValveOpened=4; //return control to ecu
						onboardLed_blue_on();
						//onboardLed_red_on();
						can_tx(&forceQVexhaustValveMsgHeader[ForceQVexhaustValveOpened-1], forceQVexhaustValveMsgData[ForceQVexhaustValveOpened-1]); //send connect message
						lastSentQVexhaustValveMsgTime=currentTime;
						switch(ForceQVexhaustValveOpened){
							case 1: //connection request was sent
							case 2: //presence was sent
								ForceQVexhaustValveOpened++; //prepare to send next
								break;
							case 3: //param overwrite was sent
								ForceQVexhaustValveOpened--; //prepare to send tester presence
								break;
							case 4: //return control to ECU was sent
								ForceQVexhaustValveOpened=0; //sequence end
								break;
							default:  //we will never come here
								break;
						}
 
					}
					break;
				default: //do nothing
					break;
			}
 
		}
 
		if(ESCandTCinversion){
			if(currentRpmSpeed==0) ESCandTCinversion=0;
			if((currentTime-lastSent384)>50){
				lastSent384=currentTime;
				DNA_msg_data[1] = (DNA_msg_data[1] & ~0x7C) | 0x30;  //set Race mode (0x30) to show on IPC the race screen
				//uint8_t tmpCounter=(DNA_msg_data[6] & 0x0F)+1;
				//if(tmpCounter>0x0F) tmpCounter=0;
				//DNA_msg_data[6]= (DNA_msg_data[6] & 0xF0) | tmpCounter;   //increment counter
				DNA_msg_data[7]=calculateCRC(DNA_msg_data,DNA_msg_header.DLC); //update CRC
				can_tx((CAN_TxHeaderTypeDef *)&DNA_msg_header, DNA_msg_data); //transmit the modified packet
			}
		}
 
 
		if(instructSlaveBoardsTriggerEnabled){
			if((currentTime-allProcessorsWakeupTime)>TIMING__C1____DELAY_BEFORE_SERIAL_INSTRUCT_OF_C2BH_AFTER_OTHER_CHIP_WAKE_MS){
				//onboardLed_red_blink(5);
				instructSlaveBoardsTriggerEnabled=0; //avoid to return here
 
 
				//send messages to slave boards
				uint8_t tmpArr0[2]={C2_Bh_BusID,C2_Bh_cmdFunction_ESC_TC_Enabled};
				if(!function_esc_tc_customizator_enabled) tmpArr0[1]=C2_Bh_cmdFunction_ESC_TC_Disabled;
				addToUARTSendQueue(tmpArr0, 2);
 
				//send messages to slave boards
				uint8_t tmpArr1[3]={C2_Bh_BusID,C2_Bh_cmdSetPedalBoostStatus,function_pedal_booster_enabled};
				addToUARTSendQueue(tmpArr1, 3);
 
				//send the message to BH to inform about the status of the function disable_odometer_blink
				uint8_t tmpArr2[2]={BhBusID,BHcmdOdometerBlinkDefault};
				if(function_disable_odometer_blink) tmpArr2[1]=BHcmdOdometerBlinkDisable;
				addToUARTSendQueue(tmpArr2, 2);
 
				//Now let's inform the C2 Baccable
				uint8_t tmpArr3[2]={C2BusID,C2cmdRaceMaskDefault};
				if(function_show_race_mask) tmpArr3[1]=C2cmdShowRaceMask;
				addToUARTSendQueue(tmpArr3, 2);
 
				//Now let's inform the BH Baccable
				uint8_t tmpArr4[2]={BhBusID,BHcmdFunctParkMirrorDisabled};
				if(function_park_mirror) tmpArr4[1]=BHcmdFunctParkMirrorEnabled;
				addToUARTSendQueue(tmpArr4, 2);
 
				//notify to C2 and BH HAS function status
				uint8_t tmpArr5[2]={C2_Bh_BusID,C2_Bh_cmdFunctHAS_Disabled};
				if(HAS_function_enabled) tmpArr5[1]=C2_Bh_cmdFunctHAS_Enabled;
				addToUARTSendQueue(tmpArr5, 2);
 
				//notify to C2 the park sensors mute function status
				uint8_t tmpArr6[2]={C2BusID,C2cmdFunctParkSensorsMuteDisabled};
				if(parkSensorsMuteFunctionEnabled) tmpArr6[1]=C2cmdFunctParkSensorsMuteEnabled;
				addToUARTSendQueue(tmpArr6, 2);
 
				// Sync BH reverse audio mute after startup, including the persisted OFF state.
                uint8_t tmpArrReverse[2]={BhBusID, reverseMuteFunctionEnabled ? BHcmdReverseMuteEnabled : BHcmdReverseMuteDisabled};
                addToUARTSendQueue(tmpArrReverse, 2);

				//notify to C2 and BH the sniffer function status //sniffer function 24/08/2026
				uint8_t tmpArr7[2]={C2_Bh_BusID,C2_Bh_cmdSnifferDisabled};
				if(snifferFunctionEnabled){
					tmpArr7[1]=C2_Bh_cmdSnifferEnabled;
					//restart our own host detection window from this instant, so it runs alongside the one the
					//slaves are starting right now on receiving this very message, instead of being measured from
					//our own earlier boot: otherwise C1 would give up (and allow sleep, pausing the inter chip
					//uart) while a slave is still legitimately waiting for its host. snifferInUse is already 1
					//since boot - see C1baccableInitCheck() - so this does not affect the sleep guard at all.
					snifferActivationTime=currentTime; //sniffer function 24/08/2026
				}
				addToUARTSendQueue(tmpArr7, 2);
			}
		}
 
		if(function_led_strip_controller_enabled==1){
			//don't act as canable. One USB port pin is used to control leds.
			vuMeterInit(); //initialize leds strip controller - this is called many times to divide the operations on more loops
			if(ledsStripIsOn){ //if the strip is on,
				if(currentTime-timeSinceLastReceivedAcceleratorMessage>10000 ){ //if no can interesting message for 10 seconds,  shutdown the leds to save energy
					shutdownLedsStrip();
					ledsStripIsOn=0; //entriamo solo una volta
				}
			}
		}
 
		if(function_4wd_disabler_enabled==1){
			if(_4wd_disabled>0){
				uint8_t tempDeltaTime=0;
				if(_4wd_disabled>1) tempDeltaTime=70;
				if(currentTime-last_sent_drive_train_msg_time>30+tempDeltaTime){ //enter here once each 100msec, then each 30msec
					last_sent_drive_train_msg_time=currentTime;
					onboardLed_blue_on();
					can_tx(&driveTrainControlModuleResetMsgHeader[_4wd_disabled-1], driveTrainControlModuleResetMsgData[_4wd_disabled-1]); //send drive train control module message
					if(_4wd_disabled>1) _4wd_disabled--;
				}
			}
		}
 
		if(executeDashboardBlinks>0){ //if we shall execute a blink to give a feedback to the user
			if(currentTime-last_sent_dashboard_blink_msg_time>500){ //enter here once each halfsecond
				last_sent_dashboard_blink_msg_time=currentTime;//return here after half second
				//change the message
				if(executeDashboardBlinks % 2==0){ //select one time reduced brightness and one time high brightness, so that in any condition the change is visible on the dashboard
					dashboardBlinkMsgData[4]=0x00; //max bright
				}else{
					dashboardBlinkMsgData[4]=0xF0; //reduced bright
				}
				executeDashboardBlinks = executeDashboardBlinks -1 ;//decrease blinks counter
				onboardLed_blue_on();
				//send the message
				can_tx(&dashboardBlinkMsgHeader, dashboardBlinkMsgData);
			}
		}
 
 
		if(immobilizerEnabled){
			//the following it is used only by IMMOBILIZER functionality
			if(floodTheBus){ //WHEN THIS IS ACTIVATED, THE THIEF WILL NOT BE ABLE TO CONNECT TO RFHUB, AND CAR WILL NOT SWITCH ON.
				if(currentTime-floodTheBusLastTimeSent>10){
					can_tx(&panicAlarmStartMsgHeader[0], panicAlarmStartMsgData[0]); //sends the message on can bus that resets the connection to RFHUB
					floodTheBusLastTimeSent=currentTime;
				}
 
				if(!panicAlarmActivated){ //if panic alarm is not activated, we shall activate it after 1 second (1 second to avoid stop and start simultaneous)
					if(currentTime-floodTheBusStartTime>1000){
						for (uint8_t i=0;i<15;i++){
							can_tx(&panicAlarmStartMsgHeader[1], panicAlarmStartMsgData[1]);
						}
						can_tx(&panicAlarmStartMsgHeader[2], panicAlarmStartMsgData[2]);
						panicAlarmActivated=1;
					}
				}
				if(currentTime-floodTheBusStartTime>10000){ //if the bus is flooded since 10 seconds, stop flooding it
					floodTheBus=0; //stop flooding
					//stop the panic alarm
					for (uint8_t i=0;i<15;i++){
						can_tx(&panicAlarmStartMsgHeader[1], panicAlarmStartMsgData[1]);
					}
					can_tx(&panicAlarmStartMsgHeader[2], panicAlarmStartMsgData[2]);
					panicAlarmActivated=0;
				}
 
 
			}
		}
 
		if(function_smart_disable_start_stop_enabled){
			if(startAndStopEnabled){
				if(currentTime>10000){ //first 10 seconds don't do anything to avoid to disturb other startup functions or immobilizer
					if(engineOnSinceMoreThan5seconds>=500){ //if motor is on since at least 5 seconds
 
						if(startAndstopCarStatus==0){//if start & stop was found disabled in car, we don't need to do anything. Avoid to enter here; We enter here in example if board is switched when the car is running and S&S was still manually disabled by the pilot
							startAndStopEnabled=0;
							requestToDisableStartAndStop=0;
						}else{
							if(lastTimeStartAndstopDisablerButtonPressed==0){ //first time we arrive here, go inside
								requestToDisableStartAndStop=1;
							}
						}
					}
				}
			}
		}
 
		/*
		if(seatbeltAlarmDisabled==0xff ){ //if the status is unknown
			if(engineOnSinceMoreThan5seconds>=200){ //if motor is on since at least 2 seconds
				//get the status of the seatbelt alarm
 
				uds_parameter_request_msg_header.ExtId=0x18DA60F1;
				uds_parameter_request_msg_data[0]=0x03;
				uds_parameter_request_msg_data[1]=0x22;
				uds_parameter_request_msg_data[2]=0x55;
				uds_parameter_request_msg_data[3]=0xA0;
				uds_parameter_request_msg_header.DLC=4;
				can_tx(&uds_parameter_request_msg_header, uds_parameter_request_msg_data); //transmit the request
				seatbeltAlarmDisabled=0xfe; //status is in aquisition
				seatbeltAlarmStatusRequestTime=currentTime;
				last_sent_uds_parameter_request_Time=currentTime;
			}
		}
		*/
 
		if(((seatbeltAlarmDisabled==0xfe) || (seatbeltAlarmDisabled==0x10) || (seatbeltAlarmDisabled==0x20) || (seatbeltAlarmDisabled==0x11) || (seatbeltAlarmDisabled==0x21)) && (currentTime-seatbeltAlarmStatusRequestTime>10000)){ //if operations in progress but timeout was reached
			seatbeltAlarmDisabled=0xff;//return to unknown status
		}
 
 
		if(((seatbeltAlarmDisabled==1) || (seatbeltAlarmDisabled==0xff) ) && (function_seatbelt_alarm_enabled==1)){ //the alarm is disabled (or unknown) and the use wants to enable it
			if(engineOnSinceMoreThan5seconds>=200){
				//request to enable Seatbelt Alarm
				//send diag session request
				uds_parameter_request_msg_header.ExtId=0x18DA60F1;
				uds_parameter_request_msg_data[0]=0x02;
				uds_parameter_request_msg_data[1]=0x10;
				uds_parameter_request_msg_data[2]=0x03;
				uds_parameter_request_msg_header.DLC=3;
				can_tx(&uds_parameter_request_msg_header, uds_parameter_request_msg_data); //transmit the diag session request
 
				seatbeltAlarmDisabled=0x20; //request to enable SeatBelt alarm in progress(send write param)
				seatbeltAlarmStatusRequestTime=currentTime;
				last_sent_uds_parameter_request_Time=currentTime;
			}
		}
 
		if((seatbeltAlarmDisabled==0 || seatbeltAlarmDisabled==0xff)  && function_seatbelt_alarm_enabled==0){ //alarm is enabled (or unknown) and user wants to disable it
			if(engineOnSinceMoreThan5seconds>=200){
				//request to disable Seatbelt Alarm
				//send diag session request
				uds_parameter_request_msg_header.ExtId=0x18DA60F1;
				uds_parameter_request_msg_data[0]=0x02;
				uds_parameter_request_msg_data[1]=0x10;
				uds_parameter_request_msg_data[2]=0x03;
				uds_parameter_request_msg_header.DLC=3;
				can_tx(&uds_parameter_request_msg_header, uds_parameter_request_msg_data); //transmit the diag session request
 
				seatbeltAlarmDisabled=0x10; //request to disable SeatBelt alarm in progress(send write param)
				seatbeltAlarmStatusRequestTime=currentTime;
				last_sent_uds_parameter_request_Time=currentTime;
			}
		}
 
		if(function_pedal_booster_enabled){ //if enabled, communicate with schizzaForte each 250msec
			if(currentRpmSpeed<400) currentSchizzaforteMap='-'; //if engine is stopped. Forget the pedal map setting, so that baccable will be forced to set it again on schizzaforte
 
			if(function_pedal_booster_enabled==8){ // Kids Limiter
				// Independent check 1: ensure map A with minimum power is active (handles boot and any drift)
				if(currentRpmSpeed>400 && currentSchizzaforteMap!='A'){
					if((currentTime-last_queued_serial_to_schizzaForte_msg_time)>(4*TIMING__C1____SCHIZZAFORTE_SERIAL_TIMEOUT_REPLY_MS)){
						last_queued_serial_to_schizzaForte_msg_time=currentTime;
						int8_t saved_pedal_map_power=pedal_map_power;
						pedal_map_power=-10;
						setSchizzaforteMap(3); //A map with minimum power
						pedal_map_power=saved_pedal_map_power;
					}
				}
				// Independent check 2: override throttle to 0 when RPM or speed exceed thresholds
				uint32_t kidsRpmLimit = function_is_diesel_enabled ? 3000 : 4000;
				if((currentRpmSpeed>kidsRpmLimit || currentSpeed_km_h>100.0f) && currentRpmSpeed>400){
					if((currentTime-last_queued_serial_to_schizzaForte_msg_time)>400){
						last_queued_serial_to_schizzaForte_msg_time=currentTime;
						uint8_t kidsLimMsg[UART1_BUFFER_SIZE]={'#',0xff,0x00,0,0,0,0,0,0};
						kidsLimMsg[8]=calculateCRC(kidsLimMsg,UART1_BUFFER_SIZE);
						addToUART1SendQueue(kidsLimMsg, 9);
					}
				}
			}else{
				if(mapCommandNotApplied() && (currentRpmSpeed>400)){ //only if the map command was not correctly received and engine is on
					if((currentTime - last_queued_serial_to_schizzaForte_msg_time)>(4 * TIMING__C1____SCHIZZAFORTE_SERIAL_TIMEOUT_REPLY_MS) ){
						last_queued_serial_to_schizzaForte_msg_time=currentTime; //avoid to Return Here
 
						if(function_pedal_booster_enabled==1){ // 1=Auto: set map according to DNA selector
							//function_pedal_booster_enabled: 0=disabled, 1=Automatic Map, 2=Bypass, 3=All Weather Map, 4=Natural Map, 5=Dynamic Map, 6=Race Map, 7=Hybrid Align, 8=Kids Limiter
							switch(currentDNAmode){
								case 0x00: //Natural
									setSchizzaforteMap(4);
									break;
								case 0x08: //Dynamic
									setSchizzaforteMap(5);
									break;
								case 0x10: //All Weather
									setSchizzaforteMap(3);
									break;
								case 0x30: //Race
									setSchizzaforteMap(6);
									break;
								default:
									setSchizzaforteMap(2); //bypass
							}
						}else if(function_pedal_booster_enabled==7){ // 7=Hybrid Align: N map in A/N/D, Race map in Race
							if(currentDNAmode==0x30){
								setSchizzaforteMap(6); //Race map when DNA is in Race
							}else{
								setSchizzaforteMap(4); //Natural map when DNA is in A, N or D
							}
						}else{
							setSchizzaforteMap(function_pedal_booster_enabled); //direct map for values 2-6
						}
					}
				}
			}
		}
 
		if(playMotorJingle){ //if  jingle was requested, check if we have to disable it
			if(currentDNAmode!=0 || currentRpmSpeed< 400){ //engine off or drive style not Natural
				playMotorJingle=0;
			}
		}
 
		if(playMotorJingle){ //if we have to execute jingle
 
 
			//once each 400msec play one note with the engine
			if((currentTime - last_queued_serial_to_schizzaForte_msg_time)>(4 * TIMING__C1____SCHIZZAFORTE_SERIAL_TIMEOUT_REPLY_MS) ){
				last_queued_serial_to_schizzaForte_msg_time=currentTime; //avoid to Return Here
				//play it: force motor RPM for one time slot
 
				// Schizzaforte Operative commands:
				//				First Character: #
				//				Second Character:	0x6d=get current Map, 0xb6=define a Map, 0xff=set accelerator percentage
				//				Next Characters:	In case of command 0xb6(Define a Map), more 7 chars are sent:
				//													3)map to write (0x00=set Bypass map, 0x49=set All weather map, 0x92=set Natural map,
				//														0xdb=set Dynamic map, 0x24=set Race map),
				//													4)polynomialcurve[][0]
				//													5)polynomialcurve[][1]
				//													6)polynomialcurve[][2]
				//													7)polynomialcurve[][3]
				//													8)polynomialcurve[][4]
				//													9)checksum (calculated)
				//				Next Characters:	In case of command 0xff (set Accelerator), more 7 chars are sent:
				//													3)accelerator value (0-255= 0 to 1 percentage)
				//													4)0 //not used
				//													5)0 //not used
				//													6)0 //not used
				//													7)0 //not used
				//													8)0 //not used
				//													9)checksum (calculated)
				// Current selected Map is sent as Reply to any request: 0x10=Bypass, 0x59=All weather, 0xa2=Natural, 0xeb=Dynamic, 0x34=Race
 
				uint8_t tmpArr0[UART1_BUFFER_SIZE]={'#',0xff,};
				tmpArr0[2]=jingleArray[255-(playMotorJingle--)]; //set accelerator value from jingle array
				tmpArr0[8]=calculateCRC(tmpArr0,UART1_BUFFER_SIZE); //calculate checksum
 
				//send message
				addToUART1SendQueue(tmpArr0, 9);
			}
		}
 
 
		if(shutdownDashboardMenuRequestTime>0){
			if(currentTime-shutdownDashboardMenuRequestTime>39000){
				baccableDashboardMenuVisible=0; //stop sending params request when motor is off
				clearDashboardBaccableMenu(); //ripulisci la stringa
				baccabledashboardMenuWasVisible=1; //allows the menu to automatically turn on when motor rotates
				shutdownDashboardMenuRequestTime=0; //avoid to return here
			}
		}
		//send a parameter request each xx msec if dashboard menu shall be visible
		//baccableDashboardMenuVisible=1; //force menu always on, just for debug
		/* ori Gaucho code end
		if((currentTime-last_sent_uds_parameter_request_Time>500) && baccableDashboardMenuVisible ){
			last_sent_uds_parameter_request_Time=currentTime;
 
 
			switch(dashboard_menu_indent_level){
				case 0: //main menu
					sendMainDashboardPageToSlaveBaccable();
					break;
				case 1:
					if(main_dashboardPageIndex==1){ //we are in show params submenu
						clearFaultsRequest=0; //ensure we don't perform more tasks simoultaneously
						currentParamElementSelection=!currentParamElementSelection;
						if(uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]== uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[!currentParamElementSelection]){
							currentParamElementSelection=0; //single param
						}
 
						if(single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].reqId>0xFF){ //if req id is greather than 0xFF it is a standard UDS request.
							//request current parameter to ECU
							uds_parameter_request_msg_header.ExtId=single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].reqId;
 
							memcpy(&uds_parameter_request_msg_data[0],&single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].reqData,single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].reqLen );
							uds_parameter_request_msg_header.DLC=single_uds_params_array[uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]].reqLen;
							//onboardLed_blue_on();
							can_tx(&uds_parameter_request_msg_header, uds_parameter_request_msg_data); //transmit the request
						}else{ //<0xff reqId means special value that we use to get particular values
							float nativeVal=getNativeParam((uint8_t)uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[currentParamElementSelection]);
							if(maxHold_enabled){
								if(isnan(dashboardParamCouple[currentParamElementSelection])){
									dashboardParamMaxHold[currentParamElementSelection]=nativeVal; //first update after navigation: reset max hold
								}else if(nativeVal > dashboardParamMaxHold[currentParamElementSelection]){
									dashboardParamMaxHold[currentParamElementSelection]=nativeVal; //new maximum found
								}
								dashboardParamCouple[currentParamElementSelection]=dashboardParamMaxHold[currentParamElementSelection];
							}else{
								dashboardParamCouple[currentParamElementSelection]=nativeVal;
							}
							sendDashboardPageToSlaveBaccable();  //Send params to BH board
						}
					}
 
					if(main_dashboardPageIndex==9){
						sendSetupDashboardPageToSlaveBaccable();
					}
					if(main_dashboardPageIndex==10){
						sendParamsSetupDashboardPageToSlaveBaccable();
					}
					if(main_dashboardPageIndex==2){ //readFaults 12/08/2026
						// Timeout stati attesa 0/1/2: 2 secondi senza risposta dal Body ECU
						if(faultsStateMachine < 3){
							if(currentTime - faultsTimer > 2000){
								stageC1cDtcTimeout(); // Body timeout or ECM partial result
								faultsTimer = currentTime;
							}
						}
						// Stato 4 TIMEOUT: dopo 1 secondo ritorna al menu principale
						if(faultsStateMachine == 4){
							if(currentTime - faultsTimer > 1000){
								faultsStateMachine = 0xFF;
								dashboard_menu_indent_level = 0;
							}
						}
						sendMainDashboardPageToSlaveBaccable(); // aggiorna display (WAIT / DTC / TIMEOUT)
					}
					break;
				default:
					break; //unexpected
			}
		}
		*/
 
/* @netzmark: to create multi parameters screens */
if(baccableDashboardMenuVisible) {
    switch(dashboard_menu_indent_level) {
        case 0: // Main Menu
            if (currentTime - last_sent_uds_parameter_request_Time >= 500) {
                last_sent_uds_parameter_request_Time = currentTime;
                sendMainDashboardPageToSlaveBaccable();
            }
            break;
 
        case 1: // Parameters & Rotator Page
            if(main_dashboardPageIndex == 1) {
                clearFaultsRequest = 0;
                uint16_t idToRequest = 0xFFFF;
 
                // --- 1. GLOBAL SYNC ---
                idLeft  = uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[0];
                idRight = uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[1];
 
                // --- 2. MODE DETERMINATION (Left rules & Safety Hack) ---
                if (idLeft >= UDS_STOP_ID) {
                    requestedId = idLeft; // Group Mode: Left Rules
                } else if (idRight >= UDS_STOP_ID) {
                    requestedId = idLeft; // Safety: Right is a Group but Left is not? Force Single Mode for Left.
                    idRight = idLeft;     // Optional: Treat as Single Mode (33, 33)
                } else {
                    // Standard Couple/Single Mode
                    requestedId = (idLeft > idRight) ? idLeft : idRight;
                }
 
                //Set Interval: 250ms for Groups, 500ms for Couple/Single
                //dynamicInterval = (requestedId >= UDS_STOP_ID) ? 250 : 500;
                dynamicInterval = 250;
 
                // --- 2. DATA BRIDGE SYNC (The Fridge to Display) ---
                if (idLeft < MAX_UDS_PARAMS) {
                    float nativeVal = uds_values_cache[idLeft];
                    if (maxHold_enabled) {
                        if (isnan(dashboardParamCouple[0]) || nativeVal > dashboardParamCouple[0]) {
                            dashboardParamCouple[0] = nativeVal;
                        }
                    } else {
                        dashboardParamCouple[0] = nativeVal;
                    }
                }
 
                if (idRight < MAX_UDS_PARAMS) {
                    float nativeVal = uds_values_cache[idRight];
                    if (maxHold_enabled) {
                        if (isnan(dashboardParamCouple[1]) || nativeVal > dashboardParamCouple[1]) {
                            dashboardParamCouple[1] = nativeVal;
                        }
                    } else {
                        dashboardParamCouple[1] = nativeVal;
                    }
                }

 
                // --- 3. UDS ROTATOR LOGIC (Turbo Mode) ---
                if (currentTime - last_sent_uds_parameter_request_Time >= dynamicInterval) {
 
                    idToRequest = 0xFFFF; // Reset target for this turn
 
                    if (idLeft >= UDS_STOP_ID) { // --- GROUP MODE (idLeft rules) ---
                        for (int g = 0; g < 10; g++) {
                            if (uds_params_groups_array[g].groupId == UDS_STOP_ID) break;
                            if (uds_params_groups_array[g].groupId == idLeft) {
 
                                // Turbo Loop: Skip Natives and find next UDS
                                for (uint8_t i = 0; i < 4; i++) {
                                    uint8_t cId = uds_params_groups_array[g].udsParamId[group_step % 4];
 
                                    if (cId >= UDS_STOP_ID) { // let's check if it is not our placeholder
                                        group_step = 0;
                                        break;
                                    }
 
                                    if (cId < MAX_UDS_PARAMS) {
                                        if (single_uds_params_array[cId].reqId <= 0xFF) {
                                            // NATIVE: Instant update, move to next slot in same cycle
                                            uds_values_cache[cId] = getNativeParam(cId);
                                            group_step++;
                                        } else {
                                            // UDS: Found CAN target
                                            idToRequest = (uint16_t)cId;
                                            break;
                                        }
                                    } else {
                                        group_step = 0; // Guard/Empty reset
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    } else { // --- COUPLE/SINGLE MODE ---
                        currentParamElementSelection = (idLeft == idRight) ? 0 : !currentParamElementSelection;
                        uint8_t coupleTarget = (currentParamElementSelection == 0) ? idLeft : idRight;
 
                        if (coupleTarget < MAX_UDS_PARAMS) {
                            if (single_uds_params_array[coupleTarget].reqId <= 0xFF) {
                                // NATIVE: Update and force next check soon
                                uds_values_cache[coupleTarget] = getNativeParam(coupleTarget);
                                last_sent_uds_parameter_request_Time = currentTime - (dynamicInterval - 50);
                            } else {
                                idToRequest = (uint16_t)coupleTarget;
                            }
                        }
                    }
 
                    // --- 4. EXECUTE CAN REQUEST ---
                    if (idToRequest < MAX_UDS_PARAMS) {
                        actuallyRequestedUdsId = (uint8_t)idToRequest;
 
                        uds_parameter_request_msg_header.ExtId = single_uds_params_array[idToRequest].reqId;
                        memcpy(&uds_parameter_request_msg_data, &single_uds_params_array[idToRequest].reqData, single_uds_params_array[idToRequest].reqLen);
                        uds_parameter_request_msg_header.DLC = single_uds_params_array[idToRequest].reqLen;
 
                        can_tx(&uds_parameter_request_msg_header, uds_parameter_request_msg_data);
 
                        last_sent_uds_parameter_request_Time = currentTime;
                        if (idLeft >= UDS_STOP_ID) group_step++;
                    } else if (idLeft >= UDS_STOP_ID) {
                        // Keep the loop alive if only Natives were processed
                        last_sent_uds_parameter_request_Time = currentTime;
                    }
                } //end of if (currentTime - last_sent_uds_parameter_request_Time >= dynamicInterval)
            } // end of if(main_dashboardPageIndex == 1)
            break;
    } //end of the switch(dashboard_menu_indent_level)
 
    // --- 5. DISPLAY REFRESH ---
    if (currentTime - lastDisplayUpdateTime >= 500) {
        if (dashboard_menu_indent_level > 0) {
            switch (main_dashboardPageIndex) {
                case 1:  sendDashboardPageToSlaveBaccable(); break;             //parameters screens
                case 2:  // Stage C1c: DTC display/timeout must run in active Netzmark loop.
                    if (function_read_faults_enabled == 1) {
                        if (faultsStateMachine < 3 && currentTime - faultsTimer > stageC1dDtcTimeoutMs()) {
                            stageC1cDtcTimeout();
                            faultsTimer = currentTime;
                        }
                        if (faultsStateMachine == 4 && currentTime - faultsTimer > 1000) {
                            faultsStateMachine = 0xFF;
                            dashboard_menu_indent_level = 0;
                        } else {
                            sendMainDashboardPageToSlaveBaccable();
                        }
                    }
                    break;
                case 9:  sendSetupDashboardPageToSlaveBaccable(); break;        //setup menu
                case 10: sendParamsSetupDashboardPageToSlaveBaccable(); break;  //params setup menu
            }
            lastDisplayUpdateTime = currentTime;
        }
    }
}
/* @netzmark: END */
 
		//pumpForce test25/07/2026 - BEGIN
		// -----------------------------------------------------------------------
		// PUMP FORCE — Innesco sequenza UDS e invio periodico IO Control
		//
		// Abilitato da function_pump_force_enabled = 1.
		// Dopo 10 secondi dal boot (currentTime > 10000) la macchina a stati viene
		// avviata inviando una richiesta di sessione diagnostica estesa (10 03) all'ECM.
		// Le risposte ECM (50 03, 67 01, 67 02, 7F) sono gestite in processingExtendedMessage.c
		// che aggiorna pumpForceStateMachine e preimposta pumpForceTxHeader/pumpForceTxData.
		//
		// In stato 3 (accesso sicurezza concesso): invia 2F 50 11 03 FF ogni 200ms.
		// Timeout negli stati 0/1/2: se non arriva risposta entro 500ms → abort senza retry.
		// In caso di errore o timeout: function_pump_force_enabled rimane =1 ma
		// pumpForceStateMachine va a 0xFF (inattivo) → nessun re-invio automatico.
		// -----------------------------------------------------------------------
		if (function_pump_force_enabled) {
 
			// Innesco iniziale: prima esecuzione dopo 10s, macchina ferma (stato 0xFF) per test
			/*
			if (pumpForceStateMachine == 0xFF && currentTime > 10000) {
				// Apri sessione diagnostica estesa con l'ECM: invia DiagnosticSessionControl 10 03
				pumpForceTxHeader.DLC = 3;
				pumpForceTxData[0] = 0x02; // PCI: single frame, 2 byte dati
				pumpForceTxData[1] = 0x10; // SID: DiagnosticSessionControl
				pumpForceTxData[2] = 0x03; // sessionType: extendedDiagnosticSession
				can_tx(&pumpForceTxHeader, pumpForceTxData);
				pumpForceStateMachine = 0;           // attendi risposta 50 03 dalla ECM
				lastPumpForceMsgTime = currentTime;  // timestamp di inizio per calcolo timeout
			}
			*/
 
			// Timeout handshake (stati 0, 1, 2): se non arriva risposta entro 500ms → abbandona
			// Il booleano function_pump_force_enabled NON viene reimpostato (nessun retry)
			if (pumpForceStateMachine < 3 && pumpForceStateMachine != 0xFF) {
				if (currentTime - lastPumpForceMsgTime > 500) {
					pumpForceStateMachine = 0xFF; // timeout: sequenza abortita definitivamente
				}
			}
 
			// Stato 3: accesso sicurezza concesso → invia IO Control ogni 200ms
			// pumpForceTxHeader (DLC=6) e pumpForceTxData ([05,2F,50,11,03,FF,...])
			// sono stati impostati in processingExtendedMessage.c alla transizione 2→3
			if (pumpForceStateMachine == 3) {
				if (currentTime - lastPumpForceMsgTime >= 200) {
					can_tx(&pumpForceTxHeader, pumpForceTxData); // 2F 50 11 03 FF
					lastPumpForceMsgTime = currentTime;
				}
			}
		}
		//pumpForce test25/07/2026 - END
	}
 
	//verifies if message sent to schizzaforte was correctly applied
	uint8_t mapCommandNotApplied(void){
 
		switch(function_pedal_booster_enabled){
			case 0: //0=disabled
				//return 0; //not required to set map
				break;
			case 1: //1=Automatic Map
				switch(currentDNAmode){
					case 0x00: //Natural
						if(currentSchizzaforteMap=='N') return 0; //map applied
						break;
					case 0x08: //Dynamic
						if(currentSchizzaforteMap=='D') return 0; //map applied
						break;
					case 0x10: //All Weather
						if(currentSchizzaforteMap=='A') return 0; //map applied
						break;
					case 0x30: //Race
						if(currentSchizzaforteMap=='R') return 0; //map applied
						break;
					default:
						//
				}
 
				break;
			case 2: //2=Bypass
				if(currentSchizzaforteMap=='B') return 0; //map applied
				break;
			case 3: //3=All Weather Map
				if(currentSchizzaforteMap=='A') return 0; //map applied
				break;
			case 4: //4=Natural Map
				if(currentSchizzaforteMap=='N') return 0; //map applied
				break;
			case 5: //5=Dynamic Map
				if(currentSchizzaforteMap=='D') return 0; //map applied
				break;
			case 6: //6=Race Map
				if(currentSchizzaforteMap=='R') return 0; //map applied
				break;
			case 7: //7=Hybrid Align: N map for A/N/D, Race map for Race
				if(currentDNAmode==0x30){ //Race
					if(currentSchizzaforteMap=='R') return 0; //map applied
				}else{ //A, N, D → Natural map
					if(currentSchizzaforteMap=='N') return 0; //map applied
				}
				break;
			case 8: //8=Kids Limiter: throttle override, map shall be bypass
				if(currentSchizzaforteMap=='B') return 0; //map applied
			default:
				//return 1; //not correctly applied
		}
 
		return 1; //not correctly applied
	}
 
	void sendMainDashboardPageToSlaveBaccable(){
		uint8_t tmpStrLen=0;
		uartTxMsg[0]= BhBusIDparamString;//first char shall be a # to talk with slave canable connected to BH can bus
		char tmpfloatString[5]; //temp array
 
		// 4WD disabled overlay management // 4wd constraint relax change 24/08/2026
		// writes go straight into uartTxMsg (the transient send buffer), never into
		// dashboard_main_menu_array: that array is persistent menu label storage, and many
		// switch cases below only patch a few characters or none at all, so overwriting a
		// whole row there would permanently corrupt that page's label after the overlay clears
		if(function_4wd_disabler_enabled && _4wd_disabled > 0) { // 4wd constraint relax change 24/08/2026
			if(currentTime - last_4wd_disabled_overlay_time >= 6000) { // 4wd constraint relax change 24/08/2026
				last_4wd_disabled_overlay_time = currentTime; // 4wd constraint relax change 24/08/2026
				show_4wd_disabled_overlay = 1; // 4wd constraint relax change 24/08/2026
			} // 4wd constraint relax change 24/08/2026
			if(show_4wd_disabled_overlay) { // 4wd constraint relax change 24/08/2026
				if(currentTime - last_4wd_disabled_overlay_time > 1000) { // 4wd constraint relax change 24/08/2026
					show_4wd_disabled_overlay = 0; // 1 sec elapsed. hide overlay // 4wd constraint relax change 24/08/2026
				} // 4wd constraint relax change 24/08/2026
				memset(&uartTxMsg[1], ' ', DASHBOARD_MESSAGE_MAX_LENGTH); // 4wd constraint relax change 24/08/2026
				uint8_t msg[] = ">>4WD DISABLED<<"; // 4wd constraint relax change 24/08/2026
				memcpy(&uartTxMsg[1], msg, 16); // 4wd constraint relax change 24/08/2026
				addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE); // 4wd constraint relax change 24/08/2026
				return; // 4wd constraint relax change 24/08/2026
			} // 4wd constraint relax change 24/08/2026
		} // 4wd constraint relax change 24/08/2026
 
        /* @Shadin: Clear DTC log feedback only on its own menu page. */
        if(main_dashboardPageIndex==13 && dashboard_menu_indent_level==0) {
            const char *label=dtcLogClearOverlay();
            if(label) {
                memset(&uartTxMsg[1],' ',DASHBOARD_MESSAGE_MAX_LENGTH);
                memcpy(&uartTxMsg[1],label,strlen(label));
                addToUARTSendQueue(uartTxMsg,UART_BUFFER_SIZE);
                return;
            }
        }
		//update records if required
		switch(main_dashboardPageIndex){
			case 2: //READ_FAULTS_ENABLED //readFaults 12/08/2026
				if(function_read_faults_enabled==1 && dashboard_menu_indent_level==1){
					static const char hx[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
					memset(dashboard_main_menu_array[2], ' ', DASHBOARD_MESSAGE_MAX_LENGTH);
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI BEGIN */
#if STAGE_C1C_OPTIONAL_ECU_STATUS_UI
                    stageC2AbsUpdate();
                    uint8_t c1cStatusPage = stageC1cStatusPage();
                    if (c1cStatusPage != 0u) {
                        uint8_t ecu = (uint8_t)(c1cStatusPage - 1u);
                        const char *state = "No faults";
                        uint8_t count = stageC1cStatusDtcCount(ecu);
                        char countText[4];
                        uint8_t countLen = 0u;
                        switch (stageC1cStatus(ecu)) {
                            case C1C_ECU_WAIT: state = "Wait..."; break;
                            case C1C_ECU_OK:
                                if (count == 0u) state = "No faults";
                                else {
                                    /* A count is displayed below, followed by Error/Errors. */
                                    state = (count == 1u) ? "fault" : "faults";
                                }
                                break;
                            case C1C_ECU_TIMEOUT: state = "Timeout"; break;
                            case C1C_ECU_ERROR: state = "Error"; break;
                            default: break;
                        }
                        const char *name = (ecu == 2u) ? "ABS " : (ecu ? "ECM " : "BCM ");
                        memcpy(dashboard_main_menu_array[2], name, 4);
                        uint8_t offset = 5u;
                        if (stageC1cStatus(ecu) == C1C_ECU_OK && count != 0u) {
                            if (count >= 100u) countText[countLen++] = (char)('0' + count / 100u);
                            if (count >= 10u) countText[countLen++] = (char)('0' + (count / 10u) % 10u);
                            countText[countLen++] = (char)('0' + count % 10u);
                            for (uint8_t i = 0u; i < countLen && offset < DASHBOARD_MESSAGE_MAX_LENGTH; ++i)
                                dashboard_main_menu_array[2][offset++] = countText[i];
                            if (offset < DASHBOARD_MESSAGE_MAX_LENGTH)
                                dashboard_main_menu_array[2][offset++] = ' ';
                        }
                        while (*state && offset < DASHBOARD_MESSAGE_MAX_LENGTH)
                            dashboard_main_menu_array[2][offset++] = *state++;
                    } else
#endif
/* STAGE_C1C_OPTIONAL_ECU_STATUS_UI END */
					switch(faultsStateMachine){
						case 0: case 1: case 2: // in attesa risposta Body ECU
							dashboard_main_menu_array[2][0]='W';
							dashboard_main_menu_array[2][1]='a';
							dashboard_main_menu_array[2][2]='i';
							dashboard_main_menu_array[2][3]='t';
							dashboard_main_menu_array[2][4]='.';
							dashboard_main_menu_array[2][5]='.';
							dashboard_main_menu_array[2][6]='.';
							break;
						case 3: // lista DTC disponibile
							if(faultsDTCcount==0){
								dashboard_main_menu_array[2][0]='N';
								dashboard_main_menu_array[2][1]='o';
								dashboard_main_menu_array[2][2]=' ';
								dashboard_main_menu_array[2][3]='f';
								dashboard_main_menu_array[2][4]='a';
								dashboard_main_menu_array[2][5]='u';
								dashboard_main_menu_array[2][6]='l';
								dashboard_main_menu_array[2][7]='t';
								dashboard_main_menu_array[2][8]='s';
							}else{
								uint8_t b0=faultsDTCbytes[faultsDTCsubmenuIndex][0];
								uint8_t b1=faultsDTCbytes[faultsDTCsubmenuIndex][1];
								uint8_t b2=faultsDTCbytes[faultsDTCsubmenuIndex][2];
                                // Stage C1c: select ECU label using the committed Body DTC boundary.
                                static const char dtcLetter[4]={'P','C','B','U'};
                                const char *ecuLabel = stageC2AbsIndex(faultsDTCsubmenuIndex) ? "ABS " : (stageC1cDtcIsEcmIndex(faultsDTCsubmenuIndex) ? "ECM " : "BCM ");
                                memcpy(&dashboard_main_menu_array[2][0], ecuLabel, 4);
                                dashboard_main_menu_array[2][4]=' ';
                                dashboard_main_menu_array[2][5]=dtcLetter[b0>>6];
                                dashboard_main_menu_array[2][6]=(char)('0'+((b0>>4)&3));
                                dashboard_main_menu_array[2][7]=hx[b0&15];
                                dashboard_main_menu_array[2][8]=hx[b1>>4];
                                dashboard_main_menu_array[2][9]=hx[b1&15];
                                dashboard_main_menu_array[2][10]='-';
                                dashboard_main_menu_array[2][11]=hx[b2>>4];
                                dashboard_main_menu_array[2][12]=hx[b2&15];
                                // Keep the original 18-character DTC layout on both display sizes.
                                // FAULTS_DTC_MAX is 20: right-align N/M in columns 13..17.
                                uint8_t n=(uint8_t)(faultsDTCsubmenuIndex+1);
                                uint16_t totalDtc = stageC2DtcTotal();
                                uint8_t m = (totalDtc > 99u) ? 99u : (uint8_t)totalDtc;
                                uint8_t pos=17;
                                dashboard_main_menu_array[2][pos--]=(char)('0'+m%10);
                                if(m>=10) dashboard_main_menu_array[2][pos--]=(char)('0'+m/10);
                                dashboard_main_menu_array[2][pos--]='/';
                                dashboard_main_menu_array[2][pos--]=(char)('0'+n%10);
                                if(n>=10) dashboard_main_menu_array[2][pos--]=(char)('0'+n/10);
							}
							break;
						case 4: // TIMEOUT
							dashboard_main_menu_array[2][0]='T';
							dashboard_main_menu_array[2][1]='i';
							dashboard_main_menu_array[2][2]='m';
							dashboard_main_menu_array[2][3]='e';
							dashboard_main_menu_array[2][4]='o';
							dashboard_main_menu_array[2][5]='u';
							dashboard_main_menu_array[2][6]='t';
							break;
						default:
							break;
					}
				}
				break;
			case 3:
				if(function_clear_faults_enabled==1){
					if(clearFaultsRequest>0){
						memcpy(dashboard_main_menu_array[main_dashboardPageIndex], "Wait...     ", 12);
						commandsMenuEnabled=0; //disable menu movement
					}else{
						memcpy(dashboard_main_menu_array[main_dashboardPageIndex], "Clear faults", 12);
						commandsMenuEnabled=1; //enable menu movement
					}
				}
				break;
			case 4: //immo
				if(immobilizerEnabled){
					dashboard_main_menu_array[main_dashboardPageIndex][13]='N'; //on
					dashboard_main_menu_array[main_dashboardPageIndex][14]=' ';
				}else{
					dashboard_main_menu_array[main_dashboardPageIndex][13]='F'; //off
					dashboard_main_menu_array[main_dashboardPageIndex][14]='F';
				}
				break;
			case 5: //dyno
				if(printStopTheCar>0){
					printStopTheCar--;
					uint8_t stopTheCarMsg[13]={BhBusIDparamString,'S','T','O','P',' ','T','H','E',' ','C','A','R'};
					addToUARTSendQueue(stopTheCarMsg, 13);//print message "stop the car"
					return;
				}
				break;
			case 6: //ESC/TC
				break;
			case 7: //front brake
				if(printStopTheCar>0){
					printStopTheCar--;
					uint8_t stopTheCarMsg[13]={BhBusIDparamString,'S','T','O','P',' ','T','H','E',' ','C','A','R'};
					addToUARTSendQueue(stopTheCarMsg, 13);//print message "stop the car"
					return;
				}
 
				if(printEnableDyno>0){
					printEnableDyno--;
					uint8_t enableDynoMsg[12]={BhBusIDparamString,'E','N','A','B','L','E',' ','D','Y','N','O'};
					addToUARTSendQueue(enableDynoMsg, 12);//print message "Enable Dyno"
					return;
				}
				//update the string
				if(front_brake_forced==0){
					//update text {'F','r','o','n','t',' ','B','r','a','k','e',' ','N','o','r','m','a','l' },
					dashboard_main_menu_array[main_dashboardPageIndex][0]='F';
					dashboard_main_menu_array[main_dashboardPageIndex][1]='r';
					dashboard_main_menu_array[main_dashboardPageIndex][2]='o';
					dashboard_main_menu_array[main_dashboardPageIndex][3]='n';
					dashboard_main_menu_array[main_dashboardPageIndex][4]='t';
					dashboard_main_menu_array[main_dashboardPageIndex][5]=' ';
					dashboard_main_menu_array[main_dashboardPageIndex][6]='B';
					dashboard_main_menu_array[main_dashboardPageIndex][7]='r';
					dashboard_main_menu_array[main_dashboardPageIndex][8]='a';
					dashboard_main_menu_array[main_dashboardPageIndex][9]='k';
					dashboard_main_menu_array[main_dashboardPageIndex][10]='e';
					dashboard_main_menu_array[main_dashboardPageIndex][11]=' ';
					dashboard_main_menu_array[main_dashboardPageIndex][12]='N'; //normal
					dashboard_main_menu_array[main_dashboardPageIndex][13]='o'; //
					dashboard_main_menu_array[main_dashboardPageIndex][14]='r'; //
					dashboard_main_menu_array[main_dashboardPageIndex][15]='m';
					dashboard_main_menu_array[main_dashboardPageIndex][16]='a';
					dashboard_main_menu_array[main_dashboardPageIndex][17]='l';
					#if DASHBOARD_MESSAGE_MAX_LENGTH > 18
					dashboard_main_menu_array[main_dashboardPageIndex][18]=' ';
					#endif
				}else{
					if(launch_assist_enabled==1){
						if(torque>50){ //assist active and torque greather than minimum threshold. the minimum thr allows to view the menu "...assist" before the following string is printed
							//prepare variables
 
							//show torque
							dashboard_main_menu_array[main_dashboardPageIndex][0]='L';
							dashboard_main_menu_array[main_dashboardPageIndex][1]='a';
							dashboard_main_menu_array[main_dashboardPageIndex][2]='u';
							dashboard_main_menu_array[main_dashboardPageIndex][3]='n';
							dashboard_main_menu_array[main_dashboardPageIndex][4]='c';
							dashboard_main_menu_array[main_dashboardPageIndex][5]='h';
							dashboard_main_menu_array[main_dashboardPageIndex][6]=' ';
 
							floatToStr(tmpfloatString,(float)torque,0,4);
							switch(strlen(tmpfloatString)){
								case 1:
									dashboard_main_menu_array[main_dashboardPageIndex][7]=' ';
									dashboard_main_menu_array[main_dashboardPageIndex][8]=' ';
									dashboard_main_menu_array[main_dashboardPageIndex][9]=tmpfloatString[0];
									break;
								case 2:
									dashboard_main_menu_array[main_dashboardPageIndex][7]=' ';
									dashboard_main_menu_array[main_dashboardPageIndex][8]=tmpfloatString[0];
									dashboard_main_menu_array[main_dashboardPageIndex][9]=tmpfloatString[1];
									break;
								default:
									dashboard_main_menu_array[main_dashboardPageIndex][7]=tmpfloatString[0];
									dashboard_main_menu_array[main_dashboardPageIndex][8]=tmpfloatString[1];
									dashboard_main_menu_array[main_dashboardPageIndex][9]=tmpfloatString[2];
									break;
							}
 
							dashboard_main_menu_array[main_dashboardPageIndex][10]='N';
							dashboard_main_menu_array[main_dashboardPageIndex][11]='m';
							dashboard_main_menu_array[main_dashboardPageIndex][12]='/';
 
							floatToStr(tmpfloatString,(float)launch_torque_threshold,0,4);
							if(strlen(tmpfloatString)==2){
								dashboard_main_menu_array[main_dashboardPageIndex][13]=' ';
								dashboard_main_menu_array[main_dashboardPageIndex][14]=tmpfloatString[0];
								dashboard_main_menu_array[main_dashboardPageIndex][15]=tmpfloatString[1];
							}else{
								dashboard_main_menu_array[main_dashboardPageIndex][13]=tmpfloatString[0];
								dashboard_main_menu_array[main_dashboardPageIndex][14]=tmpfloatString[1];
								dashboard_main_menu_array[main_dashboardPageIndex][15]=tmpfloatString[2];
							}

							dashboard_main_menu_array[main_dashboardPageIndex][16]='N';
							dashboard_main_menu_array[main_dashboardPageIndex][17]='m';
							#if DASHBOARD_MESSAGE_MAX_LENGTH > 18
							dashboard_main_menu_array[main_dashboardPageIndex][18]=' ';
#endif
						}else{
							//update text
							dashboard_main_menu_array[main_dashboardPageIndex][0]='F';
							dashboard_main_menu_array[main_dashboardPageIndex][1]='r';
							dashboard_main_menu_array[main_dashboardPageIndex][2]='o';
							dashboard_main_menu_array[main_dashboardPageIndex][3]='n';
							dashboard_main_menu_array[main_dashboardPageIndex][4]='t';
							dashboard_main_menu_array[main_dashboardPageIndex][5]=' ';
							dashboard_main_menu_array[main_dashboardPageIndex][6]='B';
							dashboard_main_menu_array[main_dashboardPageIndex][7]='r';
							dashboard_main_menu_array[main_dashboardPageIndex][8]='a';
							dashboard_main_menu_array[main_dashboardPageIndex][9]='k';
							dashboard_main_menu_array[main_dashboardPageIndex][10]='e';
							dashboard_main_menu_array[main_dashboardPageIndex][11]=' ';
							dashboard_main_menu_array[main_dashboardPageIndex][12]='A'; //assist (launch control)
							dashboard_main_menu_array[main_dashboardPageIndex][13]='s'; //
							dashboard_main_menu_array[main_dashboardPageIndex][14]='s'; //
							dashboard_main_menu_array[main_dashboardPageIndex][15]='i';
							dashboard_main_menu_array[main_dashboardPageIndex][16]='s';
							dashboard_main_menu_array[main_dashboardPageIndex][17]='t';
							#if DASHBOARD_MESSAGE_MAX_LENGTH > 18
							dashboard_main_menu_array[main_dashboardPageIndex][18]=' ';
							#endif
						}
					}else{ //launch assist not enabled
						//update text
						dashboard_main_menu_array[main_dashboardPageIndex][0]='F';
						dashboard_main_menu_array[main_dashboardPageIndex][1]='r';
						dashboard_main_menu_array[main_dashboardPageIndex][2]='o';
						dashboard_main_menu_array[main_dashboardPageIndex][3]='n';
						dashboard_main_menu_array[main_dashboardPageIndex][4]='t';
						dashboard_main_menu_array[main_dashboardPageIndex][5]=' ';
						dashboard_main_menu_array[main_dashboardPageIndex][6]='B';
						dashboard_main_menu_array[main_dashboardPageIndex][7]='r';
						dashboard_main_menu_array[main_dashboardPageIndex][8]='a';
						dashboard_main_menu_array[main_dashboardPageIndex][9]='k';
						dashboard_main_menu_array[main_dashboardPageIndex][10]='e';
						dashboard_main_menu_array[main_dashboardPageIndex][11]=' ';
						dashboard_main_menu_array[main_dashboardPageIndex][12]='F'; //forced
						dashboard_main_menu_array[main_dashboardPageIndex][13]='o'; //
						dashboard_main_menu_array[main_dashboardPageIndex][14]='r'; //
						dashboard_main_menu_array[main_dashboardPageIndex][15]='c';
						dashboard_main_menu_array[main_dashboardPageIndex][16]='e';
						dashboard_main_menu_array[main_dashboardPageIndex][17]='d';
						#if DASHBOARD_MESSAGE_MAX_LENGTH > 18
						dashboard_main_menu_array[main_dashboardPageIndex][18]=' ';
						#endif
					}
				}
				break;
			case 8: //4wd
				if(printStopTheCar>0){
					printStopTheCar--;
					uint8_t stopTheCarMsg[13]={BhBusIDparamString,'S','T','O','P',' ','T','H','E',' ','C','A','R'};
					addToUARTSendQueue(stopTheCarMsg, 13);//print message "stop the car"
					return;
				}
				//nothing to do
				break;
			case 15: //Max Hold: update ON/OFF text
				dashboard_main_menu_array[15][10] = maxHold_enabled ? 'N' : 'F';
				dashboard_main_menu_array[15][11] = maxHold_enabled ? ' ' : 'F';
				break;
			default:
				//nothing to do
				break;
		}
 
		//add string to record
		switch(main_dashboardPageIndex){
			case 0:
				tmpStrLen=strlen(FW_VERSION);
				if(tmpStrLen>DASHBOARD_MESSAGE_MAX_LENGTH) tmpStrLen=DASHBOARD_MESSAGE_MAX_LENGTH;
				memcpy(&uartTxMsg[1],FW_VERSION,tmpStrLen);
				if (tmpStrLen < DASHBOARD_MESSAGE_MAX_LENGTH) { //if required pad with spaces
					memset(&uartTxMsg[1+tmpStrLen], ' ', UART_BUFFER_SIZE-(1+tmpStrLen)); //set to zero remaining chars
				}
				break;
			default:
				/* @Shadin: Preserve the UART frame size without reading past the 30-byte menu row. */
				memset(&uartTxMsg[1], 0, UART_BUFFER_SIZE-1);
				memcpy(&uartTxMsg[1], dashboard_main_menu_array[main_dashboardPageIndex],
				       (UART_BUFFER_SIZE-1 < sizeof dashboard_main_menu_array[0]) ?
				       UART_BUFFER_SIZE-1 : sizeof dashboard_main_menu_array[0]);
				break;
		}
 
 
 
		//send to slave baccable
		addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE);
 
	}
 
	void sendSetupDashboardPageToSlaveBaccable(){
		//uint8_t tmpStrLen=0;
		uartTxMsg[0]= BhBusIDparamString;//first char shall be a # to talk with slave canable connected to BH can bus
 
		char tmpfloatString[5]; //temp array for shift and launch threshods
		//update records if required
		switch(setup_dashboardPageIndex){
			case 0: //{'S','A','V','E','&','E','X','I','T',},
				break;
			case 1: //{'[',' ',']','S','t','a','r','t','&','S','t','o','p'},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_smart_disable_start_stop_enabled];
				break;
			case 2: //{'[',' ',']','-','-','-','-','-','-','-','-','-','-','-',},
				floatToStr(tmpfloatString,(float)launch_torque_threshold,0,4);
				if(strlen(tmpfloatString)==2){
					dashboard_setup_menu_array[setup_dashboardPageIndex][14]=' ';
					dashboard_setup_menu_array[setup_dashboardPageIndex][15]=tmpfloatString[0];
					dashboard_setup_menu_array[setup_dashboardPageIndex][16]=tmpfloatString[1];
				}else{
					dashboard_setup_menu_array[setup_dashboardPageIndex][14]=tmpfloatString[0];
					dashboard_setup_menu_array[setup_dashboardPageIndex][15]=tmpfloatString[1];
					dashboard_setup_menu_array[setup_dashboardPageIndex][16]=tmpfloatString[2];
				}
 
 
				break;
			case 3: //{'[',' ',']','L','e','d',' ','C','o','n','t','r','o','l','l','e','r',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_led_strip_controller_enabled];
				break;
			case 4: //{'[',' ',']','S','h','i','f','t',' ','I','n','d','i','c','a','t','o','r'},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_shift_indicator_enabled];
				break;
			case 5: //{'S','h','i','f','t',' ','R','P','M',' ','3','0','0','0',},
				floatToStr(tmpfloatString,(float)shift_threshold,0,5);
				dashboard_setup_menu_array[setup_dashboardPageIndex][10]=tmpfloatString[0];
				dashboard_setup_menu_array[setup_dashboardPageIndex][11]=tmpfloatString[1];
				dashboard_setup_menu_array[setup_dashboardPageIndex][12]=tmpfloatString[2];
				dashboard_setup_menu_array[setup_dashboardPageIndex][13]=tmpfloatString[3];
				break;
			case 6: //{'[',' ',']','M','y','2','3',' ','I','P','C', },
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_ipc_my23_is_installed];
				break;
			case 7: //{'O',' ',' ','R','e','g','e','n','.',' ','A','l','e','r','t',' ',' ',' '},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_regeneration_alert_enabled];
				break;
			case 8: //{'O',' ',' ','S','e','a','t','b','e','l','t',' ','A','l','a','r','m',' '},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_seatbelt_alarm_enabled];
				break;
			case 9: //{'[',' ',']','R','o','u','t','e',' ','M','e','s','s','a','g','e','s', },
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_route_msg_enabled];
				break;
			case 10: //{'[',' ',']','E','S','C','/','T','C',' ','C','u','s','t','o','m','.',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_esc_tc_customizator_enabled];
				break;
			case 11: //{'[',' ',']','D','y','n','o',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_dyno_mode_master_enabled];
				break;
			case 12: //{'[',' ',']','A','C','C',' ','V','i','r','t','u','a','l',' ','P','a','d'},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_acc_virtual_pad_enabled];
				break;
			case 13: //{'[',' ',']','B','r','a','k','e','s',' ','O','v','e','r','r','i','d','e'},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_front_brake_forcer_master];
				break;
			case 14: //{'[',' ',']','4','W','D',' ','D','i','s','a','b','l','e','r',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_4wd_disabler_enabled];
				break;
			case 15: //{'[',' ',']','C','l','e','a','r',' ','F','a','u','l','t','s',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_clear_faults_enabled];
				break;
			case 16: //{'[',' ',']','R','e','a','d',' ',' ','F','a','u','l','t','s',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_read_faults_enabled];
				break;
			case 17: //{'[',' ',']','R','e','m','o','t','e',' ','S','t','a','r','t',},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_remote_start_Enabled];
				break;
			case 18: //{'Ø',' ',' ','D','i','e','s','e','l',' ',' ',' ','P','a','r','a','m','s'},
				if(function_is_diesel_enabled){
					dashboard_setup_menu_array[setup_dashboardPageIndex][3] ='D';
					dashboard_setup_menu_array[setup_dashboardPageIndex][4] ='i';
					dashboard_setup_menu_array[setup_dashboardPageIndex][5] ='e';
					dashboard_setup_menu_array[setup_dashboardPageIndex][6] ='s';
					dashboard_setup_menu_array[setup_dashboardPageIndex][7] ='e';
					dashboard_setup_menu_array[setup_dashboardPageIndex][8] ='l';
					dashboard_setup_menu_array[setup_dashboardPageIndex][9] =' ';
					dashboard_setup_menu_array[setup_dashboardPageIndex][10]=' ';
				}else{
					dashboard_setup_menu_array[setup_dashboardPageIndex][3] ='G';
					dashboard_setup_menu_array[setup_dashboardPageIndex][4] ='a';
					dashboard_setup_menu_array[setup_dashboardPageIndex][5] ='s';
					dashboard_setup_menu_array[setup_dashboardPageIndex][6] ='o';
					dashboard_setup_menu_array[setup_dashboardPageIndex][7] ='l';
					dashboard_setup_menu_array[setup_dashboardPageIndex][8] ='i';
					dashboard_setup_menu_array[setup_dashboardPageIndex][9] ='n';
					dashboard_setup_menu_array[setup_dashboardPageIndex][10]='e';
				}
				break;
			case 19: //odometer blink
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_disable_odometer_blink];
				break;
			case 20: //{'[',' ',']','P','e','d','a','l',' ','B','o','o','s','t','e','r'},
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[!!function_pedal_booster_enabled]; // double ! coerces to bool
				switch(function_pedal_booster_enabled){
					case 0: //off
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='o';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]='o';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='s';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]='t';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='e';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='r';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
 
 
						break;
					case 1: //Auto
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='A';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]='u';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='t';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='o';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 2: //Bypass
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='B';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]='y';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='p';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='a';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]='s';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='s';
						break;
					case 3: //All Weather Map
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='A';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='M';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='a';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]='p';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 4: //Natural Map
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='N';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='M';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='a';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]='p';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 5: //Dynamic Map
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='D';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='M';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='a';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]='p';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 6: //Race Map
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='R';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='M';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='a';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]='p';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 7: //Hybrid Align
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='H';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]='y';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='b';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='r';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]='i';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='d';
						break;
					case 8: //Kids Limiter
						dashboard_setup_menu_array[setup_dashboardPageIndex][10]='.';
						dashboard_setup_menu_array[setup_dashboardPageIndex][11]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][12]='K';
						dashboard_setup_menu_array[setup_dashboardPageIndex][13]='i';
						dashboard_setup_menu_array[setup_dashboardPageIndex][14]='d';
						dashboard_setup_menu_array[setup_dashboardPageIndex][15]='s';
						dashboard_setup_menu_array[setup_dashboardPageIndex][16]=' ';
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					default: //we will never end here
						break;
				}
				break;
			case 21: // 'P','e','d','a','l',' ','P','o','w','e','r',':',' ','0',' ',' ',' ',' '
				floatToStr(tmpfloatString,(float)pedal_map_power,0,4);
				dashboard_setup_menu_array[setup_dashboardPageIndex][12]=tmpfloatString[0];
				dashboard_setup_menu_array[setup_dashboardPageIndex][13]=tmpfloatString[1];
				dashboard_setup_menu_array[setup_dashboardPageIndex][14]=tmpfloatString[2];
				dashboard_setup_menu_array[setup_dashboardPageIndex][15]=tmpfloatString[3];
				break;
			case 22:
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[function_park_mirror];
				break;
			case 23:
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[!!function_acc_autostart];
				switch(function_acc_autostart){
					case 0: //off
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 1: //simulates RES button press
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='R';
						break;
					case 2: //simulates + button press
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='+';
						break;
					default: //we will never end here
						break;
				}
				break;
			case 24:
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[!!function_close_windows_with_door_lock];
				switch(function_close_windows_with_door_lock){
					case 0: //off
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 1: //Close Windows 1
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='1';
						break;
					case 2: //Close Windows 2
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='2';
						break;
					default: //we will never end here
						break;
				}
				break;
			case 25:
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[!!function_open_windows_with_door_lock];
				switch(function_open_windows_with_door_lock){
					case 0: //off
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]=' ';
						break;
					case 1: //Close Windows 1
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='1';
						break;
					case 2: //Close Windows 2
						dashboard_setup_menu_array[setup_dashboardPageIndex][17]='2';
						break;
					default: //we will never end here
						break;
				}
				break;
			case 26:
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[HAS_function_enabled];
				break;
			case 27:
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[!!QV_exhaust_flap_function_enabled];
				dashboard_setup_menu_array[setup_dashboardPageIndex][16]=(QV_exhaust_flap_function_enabled==2) ? 'D' : ' ';
				dashboard_setup_menu_array[setup_dashboardPageIndex][17]=(QV_exhaust_flap_function_enabled==2) ? 'R' : ' ';
				break;
			case 28: //{'O',' ',' ','F','r','o','n','t',' ','P','a','r','k',' ','M','u','t','e'}
				dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[parkSensorsMuteFunctionEnabled];
				break;
			case 29: dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[reverseMuteFunctionEnabled]; break;
			case 30: dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[dnaMemoryFunctionEnabled]; break;
			case 31: dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[snifferFunctionEnabled]; break;
			case 32: dashboard_setup_menu_array[setup_dashboardPageIndex][0]=checkbox_symbols[elm327FunctionEnabled]; break;
			default:
				break;
		}
 
		/* @Shadin: Keep the original frame length; never copy beyond the menu's row. */
		memset(&uartTxMsg[1], 0, UART_BUFFER_SIZE-1);
		memcpy(&uartTxMsg[1], dashboard_setup_menu_array[setup_dashboardPageIndex],
		       (UART_BUFFER_SIZE-1 < sizeof dashboard_setup_menu_array[0]) ?
		       UART_BUFFER_SIZE-1 : sizeof dashboard_setup_menu_array[0]);
		//send to slave baccable
		addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE);
 
 
	}
 
	void sendParamsSetupDashboardPageToSlaveBaccable(){
		uartTxMsg[0]= BhBusIDparamString;//first char shall be a # to talk with slave canable connected to BH can bus
		switch(params_setup_dashboardPageIndex){
			case 0: //{'S','A','V','E','&','E','X','I','T',},
				uartTxMsg[1]='S';
				uartTxMsg[2]='a';
				uartTxMsg[3]='v';
				uartTxMsg[4]='e';
				uartTxMsg[5]='&';
				uartTxMsg[6]='E';
				uartTxMsg[7]='x';
				uartTxMsg[8]='i';
				uartTxMsg[9]='t';
				uartTxMsg[10]=0;
				uartTxMsg[11]=0;
				uartTxMsg[12]=0;
				uartTxMsg[13]=0;
				uartTxMsg[14]=0;
				uartTxMsg[15]=0;
				uartTxMsg[16]=0;
				uartTxMsg[17]=0;
				uartTxMsg[18]=0;

				break;
			default: //hidden or shown params
				uartTxMsg[1]=checkbox_symbols[shownParamsArray[params_setup_dashboardPageIndex-1]];
				uartTxMsg[2]=' ';
				uartTxMsg[3]=' ';
 
				/* @Shadin: Match the source name field, not the smaller UART frame buffer. */
				char tmpName[sizeof uds_params_array[0][0].name];
				memcpy(tmpName,
				       uds_params_array[function_is_diesel_enabled][params_setup_dashboardPageIndex - 1].name,
				       sizeof tmpName);
				tmpName[sizeof tmpName - 1] = '\0';
 
				uint8_t tmpStrLen=removePatterns(tmpName); //remove special patterns from template
				if(tmpStrLen>UART_BUFFER_SIZE-4) tmpStrLen=UART_BUFFER_SIZE-4;
				memcpy(&uartTxMsg[4], tmpName,tmpStrLen); //copy entire string, to fill it with 0 at the end.
				memset(&uartTxMsg[4+tmpStrLen], 0x20, UART_BUFFER_SIZE-4-tmpStrLen);
 
				//uint8_t tmpStrLenName=strlen((char *)uds_params_array[function_is_diesel_enabled][params_setup_dashboardPageIndex-1].name);
				//uint8_t tmpStrLenUnits=strlen((char *)single_uds_params_array[uds_params_array[function_is_diesel_enabled][params_setup_dashboardPageIndex-1].udsParamId[currentParamElementSelection]].replyMeasurementUnit);
				//uint8_t tmpCharsToWrite=UART_BUFFER_SIZE-4-tmpStrLenName; //number of chars that we can still use in the string
				//if(tmpCharsToWrite>tmpStrLenUnits+1) tmpCharsToWrite= tmpStrLenUnits+1; //if we have more space than what we have to write, set the total number of chars to write
				//if(tmpCharsToWrite>0){
				//	uartTxMsg[4+tmpStrLenName]=' '; //add a space
				//	tmpCharsToWrite--;
				//}
				//if(tmpCharsToWrite>0) memcpy(&uartTxMsg[4+tmpStrLenName+1], &single_uds_params_array[uds_params_array[function_is_diesel_enabled][params_setup_dashboardPageIndex-1].udsParamId[currentParamElementSelection]].replyMeasurementUnit,tmpCharsToWrite);
 
				break;
		}
 
		//send to slave baccable
		addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE);
 
 
	}
 
/* Gaucho ori code
	void sendDashboardPageToSlaveBaccable(){
		uartTxMsg[0]= BhBusIDparamString;//first char shall be a # to talk with slave canable connected to BH can bus
 
		// 4WD disabled overlay management // 4wd constraint relax change 24/08/2026
		if(function_4wd_disabler_enabled && _4wd_disabled > 0) { // 4wd constraint relax change 24/08/2026
			if(currentTime - last_4wd_disabled_overlay_time >= 5000) { // 4wd constraint relax change 24/08/2026
				last_4wd_disabled_overlay_time = currentTime; // 4wd constraint relax change 24/08/2026
				show_4wd_disabled_overlay = 1; // 4wd constraint relax change 24/08/2026
			} // 4wd constraint relax change 24/08/2026
			if(show_4wd_disabled_overlay) { // 4wd constraint relax change 24/08/2026
				if(currentTime - last_4wd_disabled_overlay_time > 1000) { // 4wd constraint relax change 24/08/2026
					show_4wd_disabled_overlay = 0; // 1 sec elapsed. hide overlay // 4wd constraint relax change 24/08/2026
				} // 4wd constraint relax change 24/08/2026
				memset(&uartTxMsg[1], ' ', DASHBOARD_MESSAGE_MAX_LENGTH); // 4wd constraint relax change 24/08/2026
				uint8_t msg[] = ">>4WD DISABLED<<"; // 4wd constraint relax change 24/08/2026
				memcpy(&uartTxMsg[1], msg, 16); // 4wd constraint relax change 24/08/2026
				addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE); // 4wd constraint relax change 24/08/2026
				return; // 4wd constraint relax change 24/08/2026
			} // 4wd constraint relax change 24/08/2026
		} // 4wd constraint relax change 24/08/2026
 
		char stringToPrint[25];
		buildLineWithFormat( uds_params_array[function_is_diesel_enabled][dashboardPageIndex].name ,  dashboardParamCouple, uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId, stringToPrint); //build string to print
 
		uint8_t tmpStrLen=strlen(stringToPrint);
		if(tmpStrLen>DASHBOARD_MESSAGE_MAX_LENGTH) tmpStrLen=DASHBOARD_MESSAGE_MAX_LENGTH; //truncate it. no space left
		memcpy(&uartTxMsg[1], &stringToPrint,tmpStrLen); //prepare name of parameter
 
		if (tmpStrLen < DASHBOARD_MESSAGE_MAX_LENGTH) { //if required pad with zeros
			memset(&uartTxMsg[1+tmpStrLen], ' ', UART_BUFFER_SIZE-(1+tmpStrLen)); //set to zero remaining chars
		}
		addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE);
	}
 
	*/
 
	/* @netzmark: to create multi parameters screens */
	void sendDashboardPageToSlaveBaccable() {
	    // Set header so the Slave (BH) recognizes this as a dashboard string command
	    uartTxMsg[0] = BhBusIDparamString;
 
	    // 4WD disabled overlay management // 4wd constraint relax change 24/08/2026
	    if(function_4wd_disabler_enabled && _4wd_disabled > 0) { // 4wd constraint relax change 24/08/2026
	        if(currentTime - last_4wd_disabled_overlay_time >= 5000) { // 4wd constraint relax change 24/08/2026
	            last_4wd_disabled_overlay_time = currentTime; // 4wd constraint relax change 24/08/2026
	            show_4wd_disabled_overlay = 1; // 4wd constraint relax change 24/08/2026
	        } // 4wd constraint relax change 24/08/2026
	        if(show_4wd_disabled_overlay) { // 4wd constraint relax change 24/08/2026
	            if(currentTime - last_4wd_disabled_overlay_time > 1000) { // 4wd constraint relax change 24/08/2026
	                show_4wd_disabled_overlay = 0; // 1 sec elapsed. hide overlay // 4wd constraint relax change 24/08/2026
	            } // 4wd constraint relax change 24/08/2026
 
	            // Safety adaptation: Wipe with '\0' instead of ' ' to match netzmark engine safety standard
	            memset(&uartTxMsg[1], '\0', DASHBOARD_MESSAGE_MAX_LENGTH); // 4wd constraint relax change 24/08/2026
	            uint8_t msg[] = ">>4WD DISABLED<<"; // 4wd constraint relax change 24/08/2026
	            memcpy(&uartTxMsg[1], msg, 16); // 4wd constraint relax change 24/08/2026
 
	            // Calculate dynamic packet size for overlay to ensure clean termination
	            uint8_t toSendOverlay = 16 + 2;
	            if (toSendOverlay > (DASHBOARD_MESSAGE_MAX_LENGTH + 1)) {
	                toSendOverlay = DASHBOARD_MESSAGE_MAX_LENGTH + 1;
	            }
 
	            addToUARTSendQueue(uartTxMsg, toSendOverlay); // 4wd constraint relax change 24/08/2026
	            return; // 4wd constraint relax change 24/08/2026
	        } // 4wd constraint relax change 24/08/2026
	    } // 4wd constraint relax change 24/08/2026
 
	    // Initialize workspace buffer with zeros to prevent garbage during string construction
	    char stringToPrint[64] = {0};
 
	    // Get pointers for the current dashboard page based on engine type and page index
	    const char* tpl = uds_params_array[function_is_diesel_enabled][dashboardPageIndex].name;
	    const uint8_t* ids = uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId;
 
 		//@netzmark - to confirm
	    // Fill stringToPrint with formatted data (converts UDS IDs to human-readable values)
	    //buildLineWithFormat(tpl, dashboardParamCouple, ids, stringToPrint);
	    buildLineWithFormat(tpl, ids, stringToPrint);

 
	    // Calculate text length and enforce the physical screen limit (e.g., 30 chars)
	    uint8_t tmpStrLen = strlen(stringToPrint);
	    if(tmpStrLen > DASHBOARD_MESSAGE_MAX_LENGTH) {
	        tmpStrLen = DASHBOARD_MESSAGE_MAX_LENGTH;
	    }
 
	    // IMPORTANT: Wipe the UART buffer with NULLs before copying.
	    // This ensures that even if the Slave's memory is "dirty", it receives clean data.
	    memset(&uartTxMsg[1], '\0', DASHBOARD_MESSAGE_MAX_LENGTH);
 
	    // Copy the truncated string into the UART transmit buffer
	    memcpy(&uartTxMsg[1], stringToPrint, tmpStrLen);
 
	    // Calculate total packet size: Header(1) + Text(tmpStrLen) + NULL Terminator(1)
	    // Sending +2 ensures at least one NULL byte follows the text, killing "ü-umlaut" at 28 characters bug.

	    // @netzmark - to test the menu refresh speed
//	    uint8_t toSend = tmpStrLen + 2;
 
//	    // Final safety check to stay within the physical bounds of the uartTxMsg array
//	    if (toSend > (DASHBOARD_MESSAGE_MAX_LENGTH + 1)) {
//	        toSend = DASHBOARD_MESSAGE_MAX_LENGTH + 1;
//	    }
//
//	    // Push the clean, null-terminated frame to the UART transmission queue
//	    addToUARTSendQueue(uartTxMsg, toSend);
	    /*
	     * @netzmark: Ensure atomic transmission of the exact configuration width.
	     * Dynamic packet size 'toSend' introduces UART sync-latency on the Slave side.
	     * Total fixed frame length equals: 1 byte Header + Configuration Max Length.
	     */
	    // added instead of dynamic "to send"
	    addToUARTSendQueue(uartTxMsg, (DASHBOARD_MESSAGE_MAX_LENGTH + 1));
	}
 
	/* @netzmark - END */
 
	float getNativeParam(uint8_t paramId){
 
		switch(paramId){ //do preliminary additional stuff for special parameters (not uds)
			case 0: //print oil pressure
				return (float) oilPressure * single_uds_params_array[paramId].replyScale;
				break;
			case 1: //power in CV
				return (float)torque * (float)currentRpmSpeed * single_uds_params_array[paramId].replyScale ;
				break;
			case 2: //torque in NM
				return (float)torque;
				break;
			case 3: //battery state of charge (%)
				return (float)batteryStateOfCharge  * single_uds_params_array[paramId].replyScale;
				break;
			case 4: //battery current (A)
				return ((float)batteryCurrent * single_uds_params_array[paramId].replyScale) + single_uds_params_array[paramId].replyScaleOffset;
				break;
			case 5: //engine oil temperature
				return ((float)oilTemperature * single_uds_params_array[paramId].replyScale) + single_uds_params_array[paramId].replyScaleOffset;
				break;
			case 6: //current gear
				return (float)(currentGear>>4);
				break;
			case 7: //current speed (km/h)
				return currentSpeed_km_h;
				break;
			case 8: //DPF Regeneration type
				return (float)dieselEngineRegenerationMode;
				break;
			case 9: //stat 0-100km/h
				return chronometerElapsedTime_0_100_km_h; // >=20=MISSED , increase in progress= GO
				break;
			case 10: //stat 100-200km/h
				return chronometerElapsedTime_100_200_km_h; // >=40=MISSED , increase in progress= GO
				break;
			case 11: //Best stat 0-100km/h
				return readStatisticsFromFlash(1);
				break;
			case 12: //Best stat 100-200km/h
				return readStatisticsFromFlash(2);
				break;
			case 13: //seat belt alarm status
				return seatbeltAlarmDisabled;
				break;
			case 14://debug param.
				break;
			case 15://current Drive Style
				return currentDNAmode;
				break;
			case 16:// @Shadin: diagnostic _end-to-MSP gap, not linker free SRAM
				return getFreeRAM();
				break;
			case 17://Pedal current Map
				return currentSchizzaforteMap;
				break;
			case 18://get byte0 of 41A
				return (float)last_41A_raw_data[2];		// RAW Byte 2 - @netzmark: IBS behaviour testing reasons
				break;
			case 19: //get byte1 of 41A
				return (float)last_41A_raw_data[3];		// RAW Byte 3 - @netzmark: IBS behaviour testing reasons
			    break;
			default:
				break;
		}
		return 0;
		}
 
	//buildLineWithFormat composes string to print, starting from template string and float parameters
	// syntax: $n.yf or $enum
	// n = 0..6 (integer part), y = 0..3 (decimal)
 
	/* Gaucho ori code
	void buildLineWithFormat(const char* template, float values[2], const uint8_t paramId[2], char* result) {
		//onboardLed_red_blink(5);
	    int i = 0;   // indice output
	    int which = 0; // 0 = val1, 1 = val2
 
	    for (const char* p = template; *p && i < DASHBOARD_MESSAGE_MAX_LENGTH; p++) {
	        if (*p == '$'){ //special char. it could be a param in the format $x.yf or in the format $enum
	        	if (*(p+2) == '.'){ //it should be a param in the format $x.yf
	        		if(*(p+4) == 'f'){
	        			if ((*(p+1) >= '0' && *(p+1) <= '9')  && (*(p+3) >= '0' && *(p+3) <= '9')) {
							int n = *(p+1) - '0'; // parte intera max
							int y = *(p+3) - '0'; // decimali
							int maxLen = n + (y>0 ? 1 : 0) + y;
 
							char buf[maxLen+1]; // buffer dimensionato sul campo
 
							switch(single_uds_params_array[paramId[which]].reqId){
								case 0x1A: //0-100 stat
									if(values[which]>20.0){ //missed
										for (const char* q=speedStatisticEnumStrings[0]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q; //MISSED
									}else{
										if(statistics_0_100_started){
											for (const char* q=speedStatisticEnumStrings[1]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q; //GO
										}else{ //show time
											floatToStr(buf, values[which], y, maxLen+1);
											for (char* q=buf; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH && (q-buf)<maxLen; q++) result[i++] = *q;
										}
									}
									break;
								case 0x1B: //100-200 stat
									if(values[which]>40.0){ //missed
										for (const char* q=speedStatisticEnumStrings[0]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q; //MISSED
									}else{
										if(statistics_100_200_started){
											for (const char* q=speedStatisticEnumStrings[1]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q; //GO
										}else{ //show time
											floatToStr(buf, values[which], y, maxLen+1);
											for (char* q=buf; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH && (q-buf)<maxLen; q++) result[i++] = *q;
										}
									}
									break;
								case 0x1C: //Best 0-100
									if(values[which]>20.0){ //missed
										for (const char* q=speedStatisticEnumStrings[0]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q; //MISSED
									}else{
										floatToStr(buf, values[which], y, maxLen+1);
										for (char* q=buf; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH && (q-buf)<maxLen; q++) result[i++] = *q;
									}
									break;
								case 0x1D: //Best 100-200
									if(values[which]>40.0){ //missed
										for (const char* q=speedStatisticEnumStrings[0]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q; //MISSED
									}else{
										floatToStr(buf, values[which], y, maxLen+1);
										for (char* q=buf; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH && (q-buf)<maxLen; q++) result[i++] = *q;
									}
									break;
 
								default:
									floatToStr(buf, values[which], y, maxLen+1);
 
									for (char* q=buf; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH && (q-buf)<maxLen; q++) result[i++] = *q;
							}
 
 
							which++;
							p += 4; // salta "n.yf"
	        			}else { result[i++] = *p; }
	        		}else { result[i++] = *p; }
	        	}else if (*(p+4) == 'm'){ //$enum enumerator
 
	        		switch(single_uds_params_array[paramId[which]].reqId){
						case 0x17: //if Current gear request data - currentGear
							if ((uint8_t)values[which]<11){
								result[i++] = gearArray[(uint8_t)values[which]];
							}else{
								result[i++] = '-';
							}
							break;
						case 0x19:
							if((uint8_t)values[which]>6) values[which]=7;
							for (const char* q=dpfRegenEnumStrings[(uint8_t)values[which]]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q;
							break;
 
						case 0x1E:
							if((uint8_t)values[which]>1) values[which]=2;
							for (const char* q=setbeltEnumStrings[(uint8_t)values[which]]; *q && i<DASHBOARD_MESSAGE_MAX_LENGTH ; q++) result[i++] = *q;
							break;
						case 0x20:
							switch(currentDNAmode){
								case 0x00: //Natural
									result[i++]='N';
									break;
								case 0x08: //Dynamic
									result[i++]='D';
									break;
								case 0x10: //All Weather
									result[i++]='A';
									break;
								case 0x30: //Race
									result[i++]='R';
									break;
								default:
									result[i++]='?';
							}
							break;
						case 0x22: //current Pedal Map
							result[i++]=currentSchizzaforteMap;
							break;
						default:
							break;
	        		}
	        		which++;
	        		p += 4; // salta "enum"
	        	}else { result[i++] = *p; }
	        }else { result[i++] = *p; }
	    }
	    result[i] = '\0';
	}
	*/
 
			/* @netzmark: void buildLineWithFormat updated to create multi parameters screens */
		//@netzmark - to confirm	
	//void buildLineWithFormat(const char* template, float *values, const uint8_t *paramIds, char *result) {
	void buildLineWithFormat(const char* template, const uint8_t *paramIds, char *result) {
	    uint8_t i = 0, which = 0;
	    uint8_t idLeft = paramIds[0];
	    uint8_t idRight = paramIds[1];
	    const uds_params_group_element* activeGroup = NULL;
 
	    // 1. GROUP DETECTION (Left Rules)
	    if (idLeft >= UDS_STOP_ID) {
	    	for (uint8_t g = 0; g < (255 - UDS_STOP_ID + 1); g++) {
	            if (uds_params_groups_array[g].groupId == idLeft) {
	                activeGroup = &uds_params_groups_array[g];
	                break;
	            }
	            if (uds_params_groups_array[g].groupId == UDS_STOP_ID) break;
	        }
	    }
 
	    for (const char* p = template; *p && i < 63; p++) {
	        if (*p == '$') {
	            // --- ID SELECTION ---
	            uint8_t currentId;
	            if (activeGroup) {
	                currentId = activeGroup->udsParamId[which % 4];
	            } else {
	                if (which == 0) currentId = idLeft;
	                else if (which == 1) currentId = (idRight >= UDS_STOP_ID) ? idLeft : idRight;
	                else currentId = UDS_STOP_ID;
	            }
 
	            // --- DATA FETCHING (Unified Logic with Max Hold) ---
	            float valToPrint = NAN;
				if (currentId < MAX_UDS_PARAMS) {
					if (maxHold_enabled && which < 2 && !activeGroup) {
					//@netzmark - to confirm
						valToPrint = dashboardParamCouple[which];
						//valToPrint = values[which];
					} else if (single_uds_params_array[currentId].reqId <= 0xFF) {
						valToPrint = getNativeParam(currentId);
					} else {
						valToPrint = uds_values_cache[currentId];
					}
				}
 
	            // --- A. ENUM/SPECIAL HANDLER ($enum or $m) ---
	            if (*(p+1) == 'e' || *(p+4) == 'm') {
	            	uint8_t rId = (currentId < MAX_UDS_PARAMS) ? single_uds_params_array[currentId].reqId : 0;
	                switch(rId) {
	                    case 0x17: // Gear
	                        if (i < 63) result[i++] = ((uint8_t)valToPrint < 11) ? gearArray[(uint8_t)valToPrint] : '-';
	                        break;
	                    case 0x1E: // Seatbelt
	                        {
	                            const char* sbt = (valToPrint < 0.5f) ? "ON " : "OFF";
	                            for (const char* q = sbt; *q && i < 63; q++) result[i++] = *q;
	                        }
	                        break;
	                    case 0x20: // DNA Mode
	                        if(i < 63) {
	                            switch(currentDNAmode) {
	                                case 0x00: result[i++]='N'; break;
	                                case 0x08: result[i++]='D'; break;
	                                case 0x10: result[i++]='A'; break;
	                                case 0x30: result[i++]='R'; break;
	                                default:   result[i++]='?';
	                            }
	                        }
	                        break;
	                }
	                which++; p += 4; continue;
	            }
 
	            // --- B. FLOAT & STATS HANDLER ($1.1f) ---
	            if (*(p+2) == '.') {
	                int n = *(p+1) - '0';
	                int y = *(p+3) - '0';
	                int maxLen = n + (y > 0 ? 1 : 0) + y;
	                uint8_t rId = (currentId < MAX_UDS_PARAMS) ? single_uds_params_array[currentId].reqId : 0;
 
	                switch(rId) {
	                    case 0x1A: // 0-100 stat
	                        if(valToPrint > 20.0f) {
	                            for (const char* q=speedStatisticEnumStrings[0]; *q && i<63; q++) result[i++] = *q;
	                        } else if(statistics_0_100_started) {
	                            for (const char* q=speedStatisticEnumStrings[1]; *q && i<63; q++) result[i++] = *q;
	                        } else {
	                            appendFloatRightAligned(result, &i, 64, valToPrint, y, maxLen);
	                        }
	                        break;
	                    case 0x1B: // 100-200 stat
	                        if(valToPrint > 40.0f) {
	                            for (const char* q=speedStatisticEnumStrings[0]; *q && i<63; q++) result[i++] = *q;
	                        } else if(statistics_100_200_started) {
	                            for (const char* q=speedStatisticEnumStrings[1]; *q && i<63; q++) result[i++] = *q;
	                        } else {
	                            appendFloatRightAligned(result, &i, 64, valToPrint, y, maxLen);
	                        }
	                        break;
	                    case 0x1C: // Best 0-100
	                        if(valToPrint > 20.0f) {
	                            for (const char* q=speedStatisticEnumStrings[0]; *q && i<63; q++) result[i++] = *q;
	                        } else {
	                            appendFloatRightAligned(result, &i, 64, valToPrint, y, maxLen);
	                        }
	                        break;
	                    case 0x1D: // Best 100-200
	                        if(valToPrint > 40.0f) {
	                            for (const char* q=speedStatisticEnumStrings[0]; *q && i<63; q++) result[i++] = *q;
	                        } else {
	                            appendFloatRightAligned(result, &i, 64, valToPrint, y, maxLen);
	                        }
	                        break;
	                    default:
	                        appendFloatRightAligned(result, &i, 64, valToPrint, y, maxLen);
	                        break;
	                }
	                which++; p += 4; continue; // @netzmark UCINANIE ZNAKU
	                //which++; p += 3; continue;
	            }
	        }
	        if (i < 63) result[i++] = *p;
	    }
	    result[i] = '\0';
	}
 
	/* @netzmark - END */
 
	uint8_t removePatterns(char *str) {
	    char *p;
	    while ((p = strstr(str, "$")) != NULL) {
	        // Controllo che ci siano almeno 5 caratteri dopo '$'
	        if (strlen(p) >= 5) {
	            // pattern valido se termina con 'f' o 'm'
	            if (p[4] == 'f' || p[4] == 'm') {
	                // Rimuovo i 5 caratteri ($xxx[fm])
	                memmove(p, p + 5, strlen(p + 5) + 1);
	                continue; // riparto da qui
	            }
	        }
	        str = p + 1; // continuo la ricerca
	    }
	    return (uint8_t)strlen(str);
 
	}
 
 
	/**
	 * Calcola la RAM libera approssimativa
	 * Ritorna: bytes di RAM libera tra heap e stack
	 * @Shadin: Diagnostic gap from linker _end (end of .bss) to the current
	 * MSP. It is NOT the linker MAP free SRAM, nor guaranteed allocatable RAM:
	 * dynamic heap allocations and future stack growth are not subtracted.
	 * Keep the legacy UDS value and label unchanged for compatibility.
	 */
	uint32_t getFreeRAM(void) {
	    uint32_t stack_top;
	    uint32_t heap_end = (uint32_t)&_end;
 
	    // Ottieni il valore corrente dello stack pointer
	    stack_top = (uint32_t)__get_MSP();
 
	    // La RAM libera è lo spazio tra la fine dell'heap e lo stack corrente
	    if (stack_top > heap_end) {
	        return stack_top - heap_end;
	    }
	    return 0;
	}
 
 
 
	void clearDashboardBaccableMenu(){
		//prepare empty message
		uartTxMsg[0]=BhBusIDparamString; //# to send message to baccable slave connected to BH can bus
		for(uint8_t i=1;i<UART_BUFFER_SIZE;i++){
			uartTxMsg[i]=0x20; //space char
		}
		//send it
		addToUARTSendQueue(uartTxMsg, UART_BUFFER_SIZE);
 
	}
 
	// this function scales value received from can bus. It is assumed that pedal position (or motor rpm) will change vumeter volume represented with the leds strip
	float scaleVolume(uint8_t vol){
		//Scale this value to get a percentage between 0 and 100
		//sniffed data on can bus: msg id=0x412 , fourth byte goes from 0x33(51) to 0xE6(230)
		if (vol>50){
			vol=vol-51;
		}
		return (float)(((float)vol*100.0f)/180.0f);
	}
 
	// this function scales value received from can bus. It is assumed that gear selection will change the color effect of the leds strip
	uint8_t scaleColorSet(uint8_t col){
		//id 2ef, primo byte, 70=r, 00=n, f0=marcia inserita ma frizione premuta (indefinito), 10=prima, 20=seconda..
		col= col>>4;
		//onboardLed_red_blink(col);
		// 7=backward, f=gear set but frizione premuta (undefined), 1=first gear , 2=second gear, ... , 6= sixt gear
		return col;
	}
 
	uint8_t saveOnflash(){
        // Stage G: transactional two-copy config, never erase the legacy settings page.
		/* @Shadin: Reuse the transactional store's existing parameter count. */
		uint16_t params[CFG_TOTAL] = {
		  immobilizerEnabled,
		  function_smart_disable_start_stop_enabled,
		  function_led_strip_controller_enabled,
		  function_shift_indicator_enabled,
		  shift_threshold,
		  function_ipc_my23_is_installed,
		  function_route_msg_enabled,
		  function_dyno_mode_master_enabled,
		  function_acc_virtual_pad_enabled,
		  function_front_brake_forcer_master,
		  function_4wd_disabler_enabled,
		  function_remote_start_Enabled,
		  function_clear_faults_enabled,
		  function_esc_tc_customizator_enabled,
		  function_read_faults_enabled,
		  function_is_diesel_enabled,
		  function_regeneration_alert_enabled,
		  launch_torque_threshold,
		  function_seatbelt_alarm_enabled,
		  function_pedal_booster_enabled,
		  function_disable_odometer_blink,
		  function_show_race_mask,
		  function_park_mirror,
		  function_acc_autostart,
		  function_close_windows_with_door_lock,
		  function_open_windows_with_door_lock,
		  HAS_function_enabled,
		  QV_exhaust_flap_function_enabled,
		  (uint8_t)pedal_map_power,
		  parkSensorsMuteFunctionEnabled,  //slot 30: FRONT_PARK_MUTE
		  snifferFunctionEnabled,  //slot 31: SNIFFER //sniffer function 24/08/2026
		  elm327FunctionEnabled,  //slot 32: ELM327 //elm327 function 26/08/2026
		  dnaMemoryFunctionEnabled, // @Shadin: parameter 33 (zero-based array index 32)
		  reverseMuteFunctionEnabled, // @Shadin: parameter 34, Boolean 0=OFF / 1=ON
		};
        return config_store_save(params, CFG_TOTAL);
    }

	uint8_t resetStatisticsOnFlash(){
 
	HAL_FLASH_Unlock(); //unlock flash
 
			//erase flash
			FLASH_EraseInitTypeDef eraseInitStruct;
			uint32_t pageError=0;
			eraseInitStruct.TypeErase= FLASH_TYPEERASE_PAGES;
			eraseInitStruct.PageAddress=LAST_PAGE_ADDRESS_STATISTICS;
			eraseInitStruct.NbPages=1;
			if(HAL_FLASHEx_Erase(&eraseInitStruct,&pageError)!=HAL_OK){ //error during erase
				HAL_FLASH_Lock();
				onboardLed_red_blink(8);
				return 254; //error
			}
			HAL_FLASH_Lock();
			return 0;
	}
 
	uint8_t saveStatisticsOnFlash(){
		//compare current stats with best stored times
		uint8_t weShallWriteFlash=0;
		/* @Shadin: Only two statistics are defined/read; leave unused Flash slots erased. */
		uint16_t params[2];
		if(chronometerElapsedTime_0_100_km_h<readStatisticsFromFlash(1)){
			params[0]=(uint16_t)(chronometerElapsedTime_0_100_km_h *1000);
			weShallWriteFlash=1;
		}else{
			params[0]=(uint16_t)(readStatisticsFromFlash(1) * 1000);
		}
 
		if(chronometerElapsedTime_100_200_km_h<readStatisticsFromFlash(2)){
			params[1]=(uint16_t)(chronometerElapsedTime_100_200_km_h *1000);
			weShallWriteFlash=1;
		}else{
			params[1]=(uint16_t)(readStatisticsFromFlash(2) * 1000);
		}
 
 
		if(weShallWriteFlash){ //if new best time was found, save again all stats
			HAL_FLASH_Unlock(); //unlock flash
 
			//erase flash
			FLASH_EraseInitTypeDef eraseInitStruct;
			uint32_t pageError=0;
			eraseInitStruct.TypeErase= FLASH_TYPEERASE_PAGES;
			eraseInitStruct.PageAddress=LAST_PAGE_ADDRESS_STATISTICS;
			eraseInitStruct.NbPages=1;
			if(HAL_FLASHEx_Erase(&eraseInitStruct,&pageError)!=HAL_OK){ //error during erase
				HAL_FLASH_Lock();
				onboardLed_red_blink(8);
				return 254; //error
			}
 
			//it seems that stm32F072 supports only writing 2byte words
			//write parameter
 
			for (uint8_t i = 0; i < sizeof params / sizeof params[0]; i++) {
				if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, LAST_PAGE_ADDRESS_STATISTICS + (i * 4), params[i]) != HAL_OK) {
					HAL_FLASH_Lock();
					onboardLed_red_blink(9);
					return 255;
				}
			}
 
			//lock the flash
			HAL_FLASH_Lock();
			return 0;
 
		}
		return 253;
	}
 
 
	uint8_t saveShownParamsOnflash(){
		/* @Shadin: Keep the original halfword-per-4-bytes Flash layout.
		 * Only active UDS menu bits are written (48/56 for 18-char, 51/54 for
		 * 30-char display: 3 or 4 halfwords).
		 * Zero-initialize the packed words; the unwritten tail remains erased. */
		uint16_t params[(sizeof(shownParamsArray) + 15u) / 16u] = {0};
		size_t bitsToSave = total_pages_in_params_setup_dashboard_menu;
		if (bitsToSave > sizeof(shownParamsArray)) bitsToSave = sizeof(shownParamsArray);
		const size_t wordsToSave = (bitsToSave + 15u) / 16u;
		compress_to_uint16(shownParamsArray, bitsToSave, params);

		if (HAL_FLASH_Unlock() != HAL_OK) {
			onboardLed_red_blink(8);
			return 254;
		}

		//erase flash
		FLASH_EraseInitTypeDef eraseInitStruct;
		uint32_t pageError=0;
		eraseInitStruct.TypeErase= FLASH_TYPEERASE_PAGES;
		eraseInitStruct.PageAddress=LAST_PAGE_ADDRESS_SHOWN_PARAMS;
		eraseInitStruct.NbPages=1;
		if(HAL_FLASHEx_Erase(&eraseInitStruct,&pageError)!=HAL_OK){ //error during erase
			HAL_FLASH_Lock();
			onboardLed_red_blink(8);
			return 254; //error
		}

		//it seems that stm32F072 supports only writing 2byte words
		//write parameter
		for (size_t i = 0; i < wordsToSave; i++) {
			if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, LAST_PAGE_ADDRESS_SHOWN_PARAMS + (i * 4u), params[i]) != HAL_OK) {
				HAL_FLASH_Lock();
				onboardLed_red_blink(9);
				return 255;
			}
		}

		//lock the flash
		HAL_FLASH_Lock();
		return 0;
	}

	void compress_to_uint16(const uint8_t *input, size_t input_len, uint16_t *output) {
	    size_t out_index = 0;
	    uint16_t acc = 0;
	    uint8_t bit_count = 0;
 
	    for (size_t i = 0; i < input_len; ++i) {
	        acc = (acc << 1) | (input[i] & 1); // inserisce il bit in MSB-first
	        bit_count++;
 
	        if (bit_count == 16) {
	            output[out_index++] = acc;
	            acc = 0;
	            bit_count = 0;
	        }
	    }
 
	    // Scrive l'ultimo uint16_t se ci sono bit residui
	    if (bit_count > 0) {
	        acc <<= (16 - bit_count); // allinea i bit a sinistra
	        output[out_index] = acc;
	    }
	}
 
 
	void decompress_from_uint16(const uint16_t *input, size_t input_len, uint8_t *output, size_t output_len) {
	    size_t out_index = 0;
 
	    for (size_t i = 0; i < input_len && out_index < output_len; ++i) {
	        uint16_t value = input[i];
	        for (int b = 15; b >= 0 && out_index < output_len; --b) {
	            output[out_index++] = (value >> b) & 1;
	        }
	    }
	}
 
	void readShownParamsFromFlash(){
		/* @Shadin: Only the first 240 stored bits are used; preserve the
		 * original addresses and let erased tail words read as 0xFFFF. */
		uint16_t params[(sizeof(shownParamsArray) + 15u) / 16u];
		for (size_t b = 0; b < sizeof(params) / sizeof(params[0]); b++) {
			params[b]=*(volatile uint16_t*)(LAST_PAGE_ADDRESS_SHOWN_PARAMS+(b*4u));
		}
 
		//decompress
		decompress_from_uint16(params, sizeof(params) / sizeof(params[0]),
		                           shownParamsArray, sizeof(shownParamsArray));
 
		}
 
	float readStatisticsFromFlash(uint8_t paramId){ // 1=0-100km/h, 2=100/200km/h
		if(paramId<1) return 0;
		uint16_t tmpParam=*(volatile uint16_t*)(LAST_PAGE_ADDRESS_STATISTICS+((paramId-1)*4));
 
		return (float)(((float)tmpParam)/1000.0);
 
	}
 
	uint16_t readFromFlash(uint8_t paramId){
		if(paramId<1) return 0;
		uint16_t tmpParam=config_store_read((uint8_t)(paramId-1));
		switch(paramId){
			case 1: //IMMOBILIZER_ENABLED (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(IMMOBILIZER_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 2: //SMART_DISABLE_START_STOP enable status (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(SMART_DISABLE_START_STOP)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 3: //LED_STRIP_CONTROLLER_ENABLED  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(LED_STRIP_CONTROLLER_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 4: //SHIFT_INDICATOR_ENABLED  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(SHIFT_INDICATOR_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 5: //SHIFT_THRESHOLD
				if(tmpParam==0xFFFF){
					#if defined(SHIFT_THRESHOLD)
						return SHIFT_THRESHOLD;
					#else
						return 3500; // another default value
					#endif
				}
				break;
			case 6: //IPC_MY23_IS_INSTALLED  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(IPC_MY23_IS_INSTALLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 7: //ROUTE_MSG  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(ROUTE_MSG)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 8: //DYNO_MODE_MASTER  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(DYNO_MODE_MASTER)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 9: //ACC_VIRTUAL_PAD  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(ACC_VIRTUAL_PAD)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 10: //FRONT_BRAKE_FORCER_MASTER  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(FRONT_BRAKE_FORCER_MASTER)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 11: //_4WD_DISABLER  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(_4WD_DISABLER)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 12: //REMOTE_START_ENABLED  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(REMOTE_START_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 13: //CLEAR_FAULTS_ENABLED  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(CLEAR_FAULTS_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 14: //ESC_TC_CUSTOMIZATOR_MASTER  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(ESC_TC_CUSTOMIZATOR_MASTER)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 15: //READ_FAULTS_ENABLED  (1=enabled 0=disabled)
				if(tmpParam>1){
					#if defined(READ_FAULTS_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 16: //IS_DIESEL  (1=enabled 0=disabled)
					if(tmpParam>1){
						#if defined(IS_DIESEL)
							return 1;
						#else
							return 0;
						#endif
					}
					break;
			case 17: //REGENERATION_ALERT_ENABLED  (1=enabled 0=disabled)
					if(tmpParam>1){
						#if defined(REGENERATION_ALERT_ENABLED)
							return 1;
						#else
							return 0;
						#endif
					}
					break;
			case 18: //LAUNCH_THRESHOLD
				if(tmpParam==0xFFFF){
					#if defined(LAUNCH_THRESHOLD)
						return LAUNCH_THRESHOLD;
					#else
						return 100; // another default value
					#endif
				}
				break;
			case 19: //SEATBELT_ALARM_DISABLED
				if(tmpParam>1){
					#if defined(SEATBELT_ALARM_DISABLED)
						return 0;
					#else
						return 1;
					#endif
				}
				break;
			case 20: //PEDAL_BOOSTER_ENABLED
				if(tmpParam>8){
					#if defined(PEDAL_BOOSTER_ENABLED)
						return PEDAL_BOOSTER_ENABLED;
					#else
						return 0;
					#endif
				}
				break;
			case 21: //DISABLE_ODOMETER_BLINK
				if(tmpParam>1){
					#if defined(DISABLE_ODOMETER_BLINK)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 22: //SHOW_RACE_MASK
				if(tmpParam>1){
					#if defined(SHOW_RACE_MASK)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 23: //PARK_MIRROR
				if(tmpParam>1){
					#if defined(PARK_MIRROR)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 24: //ACC_AUTOSTART
				if(tmpParam>2){
					#if defined(ACC_AUTOSTART)
						return ACC_AUTOSTART;
					#else
						return 0;
					#endif
				}
				break;
			case 25: //CLOSE_WINDOWS
				if(tmpParam>2){
					#if defined(CLOSE_WINDOWS)
						return CLOSE_WINDOWS;
					#else
						return 0;
					#endif
				}
				break;
			case 26: //OPEN_WINDOWS
				if(tmpParam>2){
					#if defined(OPEN_WINDOWS)
						return OPEN_WINDOWS;
					#else
						return 0;
					#endif
				}
				break;
			case 27: //HAS_VIRTUAL_PAD
				if(tmpParam>1){
					#if defined(HAS_VIRTUAL_PAD)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 28: //QV_EXHAUST_FLAP_FUNCTION_ENABLED
				/* @Shadin: Gaucho's original Drive Style setting 2 is valid and persistent. */
				if(tmpParam>2){
					#if defined(QV_EXHAUST_FLAP_FUNCTION_ENABLED)
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			case 29: //pedal map amplification factor
				if(tmpParam==0xFFFF){
					#if defined(PEDAL_MAP_POWER)
						return PEDAL_MAP_POWER;
					#else
						return 0; // another default value
					#endif
				}
				break;
			case 30: //FRONT_PARK_MUTE (1=enabled 0=disabled)
				if(tmpParam>1){
					/* @Shadin: Restore Gaucho's user_config.h default for unset Flash. */
					#if defined(FRONT_PARK_MUTE)
						return 1;
					#else
						return 0; // flash non inizializzata: default disabilitato
					#endif
				}
				break;
			case 31: //SNIFFER (1=enabled 0=disabled) //sniffer function 24/08/2026
				if(tmpParam>1){
					#if defined(SNIFFER)
						return 1;
					#else
						return 0; // flash non inizializzata: default disabilitato
					#endif
				}
				break;
			case 32: //ELM327 (1=enabled 0=disabled) //elm327 function 26/08/2026
				if(tmpParam>1){
					#if defined(ELM327)
						return 1;
					#else
						return 0; // flash non inizializzata: default disabilitato
					#endif
				}
				break;
			case 33: if(tmpParam>1) return 0; break;
			case 34: // @Shadin: Reverse Mute uses standard Boolean 0/1; invalid values use user_config.h.
				if(tmpParam>1){
					#ifdef REVERSE_MUTE_ENABLED
						return 1;
					#else
						return 0;
					#endif
				}
				break;
			default:
				return 0;
				break;
		}
		return tmpParam;
	}
 
	uint8_t getParamIndexFromReqId(uint32_t searchedReqId){
 
		for (uint8_t i=0;i<total_pages_in_params_setup_dashboard_menu;i++){
			if(single_uds_params_array[uds_params_array[function_is_diesel_enabled][i].udsParamId[0]].reqId==searchedReqId) return i;
		}
		//if not found, search it in second param
		for (uint8_t i=0;i<total_pages_in_params_setup_dashboard_menu;i++){
			if(single_uds_params_array[uds_params_array[function_is_diesel_enabled][i].udsParamId[1]].reqId==searchedReqId) return i;
		}
		return 0; //means not found, or param0 (it could be an exception)
	}
 
 
 
 
	uint8_t getNextVisibleParam(uint8_t curIndex) {
	    /* @Shadin: Bounded circular search; do not index past the array if all pages are hidden. */
	    uint8_t count = total_pages_in_params_setup_dashboard_menu;
	    if (!count) return 0;
	    curIndex = (uint8_t)(curIndex % count);
	    for (uint8_t offset = 0; offset < count; offset++) {
	        uint8_t index = (uint8_t)((curIndex + offset) % count);
	        if (shownParamsArray[index]) return index;
	    }
	    return curIndex;
	}

	uint8_t getPreviousVisibleParam(uint8_t curIndex) {
	    /* @Shadin: Reverse wrap without uint8_t underflow or out-of-range reads. */
	    uint8_t count = total_pages_in_params_setup_dashboard_menu;
	    if (!count) return 0;
	    curIndex = (uint8_t)(curIndex % count);
	    for (uint8_t offset = 0; offset < count; offset++) {
	        uint8_t index = (uint8_t)((curIndex + count - offset) % count);
	        if (shownParamsArray[index]) return index;
	    }
	    return curIndex;
	}
 
	// Event-driven max hold update for native CAN parameters.
	// Called directly at the extraction point of each native CAN variable,
	// so peaks are captured at CAN bus rate (e.g. every 10ms) rather than at the 500ms display cycle.
	// Only updates dashboardParamMaxHold; display (dashboardParamCouple) is still refreshed at 500ms.
	// Fast-returns if max hold is inactive or the params view is not shown.
	void nativeMaxHoldUpdate(uint8_t paramId){
	    if(!maxHold_enabled || dashboard_menu_indent_level!=1 || main_dashboardPageIndex!=1 || !baccableDashboardMenuVisible) return;
	    for(uint8_t sel=0; sel<2; sel++){
	        if(uds_params_array[function_is_diesel_enabled][dashboardPageIndex].udsParamId[sel]==paramId){
	            if(!isnan(dashboardParamCouple[sel])){ //only after the 500ms cycle has set the initial value
	                float value=getNativeParam(paramId);
	                if(value > dashboardParamCouple[sel]){
	                    dashboardParamCouple[sel]=value;
	                }
	            }
	            break;
	        }
	    }
	}
 
 
#endif


/* @Shadin: ABS C1 service and transport integrated from abs_c1_service.c.
 * Original implementation retained for upstream-friendly C1 source organization. */
#if defined(C1baccable)
/* @Shadin: C1 ABS reply receiver and polling transport moved from
 * abs_receiver.c and abs_transport.c without changing their logic. */
void abs_receiver_init(AbsReceiver *r,uint8_t id){memset(r,0,sizeof(*r));r->id=id;r->status='B';}
int abs_receiver_accept(AbsReceiver *r,const uint8_t frame[ABS_PROTO_FRAME],uint8_t route){
 AbsReply x;if(abs_decode(frame,route,&x))return -1;
 if(x.id!=r->id)return -2;
 if(r->status=='E'||(r->status=='R'&&r->next==r->kept&&r->total_set))return -2;
 if(x.status=='E'){r->status='E';r->reason=x.reason;return -3;}
 if(x.status=='B')return 0;
 if(x.kept>x.total||x.index!=r->next)return -1;
 if(r->total_set&&(r->total!=x.total||r->kept!=x.kept))return -1;
 if(x.index<x.kept){
   if(!r->total_set){r->total=x.total;r->kept=x.kept;r->total_set=1;}
   memcpy(r->codes[r->next],x.code,3);r->next++;
   if(r->next==r->kept){r->status='R';return 2;}
   return 1;
 }
 if(x.kept==0&&!r->total_set){r->total=x.total;r->kept=0;r->total_set=1;r->status='R';return 2;}
 return -1;
}

void abs_transport_init(AbsTransport *t,uint8_t id){
 memset(t,0,sizeof(*t));abs_receiver_init(&t->receiver,id);
}
int abs_transport_tick(AbsTransport *t,uint32_t now,uint8_t *index){
 if(!t||!index)return -1;
 if(t->timed_out||t->receiver.status=='E'||
    (t->receiver.total_set&&t->receiver.next==t->receiver.kept))return -1;
 if(t->poll_started&&(uint32_t)(now-t->last_poll_ms)<ABS_TRANSPORT_POLL_MS)return 0;
 if(t->polls_without_progress>=ABS_TRANSPORT_MAX_RETRIES){t->timed_out=1;return -2;}
 t->poll_started=1;t->last_poll_ms=now;t->polls_without_progress++;
 *index=t->receiver.next;return 1;
}
int abs_transport_reply(AbsTransport *t,const uint8_t frame[ABS_PROTO_FRAME],uint8_t route){
 if(!t||!frame)return -1;
 if(t->timed_out)return -2;
 int result=abs_receiver_accept(&t->receiver,frame,route);
 if(result>0)t->polls_without_progress=0;
 return result;
}


static AbsTransport service;
static uint8_t enabled;
static uint8_t start_pending;
static uint8_t start_in_flight;
static uint8_t get_in_flight;
static uint32_t queued_at_ms;
static uint8_t queue_wait_armed;
#define ABS_UART_QUEUE_WAIT_MAX_MS 5000u
static const char hex[]="0123456789ABCDEF";
static void encode_byte(uint8_t *p, uint8_t v) { p[0]=hex[v>>4]; p[1]=hex[v&15u]; }
void abs_c1_service_begin(uint8_t id) {
    abs_c1_inbox_reset();
    abs_transport_init(&service,id);
    enabled=1u;
    start_in_flight=0u;
    get_in_flight=0u;
    queue_wait_armed=0u;
    /* Defer Start until processUART() permits actual UART traffic. */
    start_pending=1u;
}
void abs_c1_service_poll(uint32_t now_ms) {
    if (!enabled) return;
    if (queue_wait_armed && (start_in_flight || get_in_flight) &&
        (uint32_t)(now_ms - queued_at_ms) >= ABS_UART_QUEUE_WAIT_MAX_MS) {
        service.timed_out=1u;
        enabled=0u;
        queue_wait_armed=0u;
        return;
    }
    if (start_pending) {
        /* Do not start the poll budget while the Start command is pending. */
        uint8_t msg[4]={C2BusID,C2cmdAbsFaultsStart,0,0};
        encode_byte(&msg[2],service.receiver.id);
        if (!uart_try_enqueue_abs(msg,sizeof(msg))) {
            service.timed_out=1u;
            enabled=0u;
            start_pending=0u;
            return;
        }
        start_pending=0u;
        start_in_flight=1u;
        queued_at_ms=now_ms;
        queue_wait_armed=1u;
        /* Allow processUART() to transmit Start before scheduling Get. */
        return;
    }
    if (start_in_flight) return; /* Wait until Start actually enters UART hardware. */
    uint8_t frame[ABS_PROTO_FRAME];
    /* A dropped reply makes the transfer incomplete: fail closed instead of
       committing a potentially truncated DTC list. */
    if (abs_c1_inbox_dropped() != 0u) {
        service.timed_out = 1u;
        enabled = 0u;
        return;
    }
    while (abs_c1_inbox_pop(frame)) {
        (void)abs_transport_reply(&service,frame,C1BusID);
        /* Once a transfer is complete or rejected, stop consuming replies.
           Late frames cannot alter the published terminal state. */
        if (service.receiver.status == 'E' ||
            (service.receiver.status == 'R' && service.receiver.total_set &&
             service.receiver.next == service.receiver.kept)) {
            enabled = 0u;
            return;
        }
    }
    if (abs_c1_inbox_dropped() != 0u) {
        service.timed_out = 1u;
        enabled = 0u;
        return;
    }
    if (get_in_flight) return; /* No duplicate Get while it is queued. */
    uint8_t index;
    int result=abs_transport_tick(&service,now_ms,&index);
    if (result < 0) {
        /* Timeout or terminal transport state: do not keep polling. */
        enabled = 0u;
        return;
    }
    if(result==1) {
        uint8_t msg[6]={C2BusID,C2cmdAbsFaultsGet,0,0,0,0};
        encode_byte(&msg[2],service.receiver.id);
        encode_byte(&msg[4],index);
        if (uart_try_enqueue_abs(msg,sizeof(msg))) {
            get_in_flight=1u;
            queued_at_ms=now_ms;
            queue_wait_armed=1u;
        } else {
        service.timed_out = 1u;
        enabled = 0u;
    }
    }
}
void abs_c1_service_tx_started(uint8_t command, uint8_t id_hi, uint8_t id_lo, uint32_t now_ms) {
    if (!enabled) return;
    uint8_t expected[2];
    encode_byte(expected, service.receiver.id);
    if (id_hi != expected[0] || id_lo != expected[1]) return;
    if (command == C2cmdAbsFaultsStart && start_in_flight) {
        start_in_flight=0u;
        queue_wait_armed=0u;
        service.last_poll_ms=now_ms;
        service.poll_started=1u; /* First Get after the normal poll interval. */
    } else if (command == C2cmdAbsFaultsGet && get_in_flight) {
        get_in_flight=0u;
        queue_wait_armed=0u;
        service.last_poll_ms=now_ms; /* Retry budget starts on actual transmit. */
    }
}
uint8_t abs_c1_service_tx_is_current(uint8_t command, uint8_t id_hi, uint8_t id_lo) {
    uint8_t expected[2];
    if (!enabled) return 0u;
    encode_byte(expected, service.receiver.id);
    if (id_hi != expected[0] || id_lo != expected[1]) return 0u;
    if (command == C2cmdAbsFaultsStart) return start_in_flight;
    if (command == C2cmdAbsFaultsGet) return get_in_flight;
    return 0u;
}
const AbsTransport *abs_c1_service_state(void) { return &service; }
#else
void abs_c1_service_begin(uint8_t id) {(void)id;}
void abs_c1_service_poll(uint32_t now_ms) {(void)now_ms;}
void abs_c1_service_tx_started(uint8_t command, uint8_t id_hi, uint8_t id_lo, uint32_t now_ms) {(void)command;(void)id_hi;(void)id_lo;(void)now_ms;}
uint8_t abs_c1_service_tx_is_current(uint8_t command, uint8_t id_hi, uint8_t id_lo) {(void)command;(void)id_hi;(void)id_lo;return 0u;}
const AbsTransport *abs_c1_service_state(void) { return 0; }
#endif

/* @Shadin: DNA StageG implementation moved from dna_stageg.c into
 * Gaucho C1 function source. State machine and timing unchanged.
 */
/* Stage G v3.9.0 C1 adapter. dna_stageg_tick() is the sole restore action path. */
#include "compile_time_defines.h"
#if defined(C1baccable)
#include "functions_C1baccable.h"
#include "dna_journal.h"
#include "functions_C1baccable.h"
#include "stageg_ign_diag.h"
#include <stdint.h>

/* @Shadin: DNA guard implementation moved here from dna_guard.c.
 * Original state transitions and timing are preserved verbatim. */
#define SETTLE_MS 2000u
#define HOLD_MS 10000u
#define STABLE_MS 750u
#define MANUAL_SETTLE_MS 1000u
#define RUNNING_RPM 400u
static int valid(uint8_t m){return m==0||m==8||m==16||m==48;}
void dna_guard_init(dna_guard *g,uint8_t saved,uint8_t enabled){*g=(dna_guard){0};g->saved=valid(saved)?saved:255;g->enabled=enabled?1:0;g->restore_pending=g->enabled;g->candidate=255;}
/* Apply Setup changes without an unexpected DNA restore while driving.
 * A newly enabled feature learns a stable mode; the next power-on or
 * confirmed ignition return arms one guarded restore. */
void dna_guard_set_enabled(dna_guard *g, uint8_t enabled) {
    enabled = enabled ? 1u : 0u;
    if (g->enabled == enabled) return;
    g->enabled = enabled;
    g->restore_pending = 0u;
    g->command_sent = 0u;
    g->restore_failed = 0u;
    g->candidate = 255u;
    g->running_since = 0u;
    g->manual_hold_until = 0u;
}
void dna_guard_arm_cycle(dna_guard *g) {
    if (!g->enabled || g->restore_pending || g->command_sent) return;
    g->restore_pending = 1u;
    g->restore_failed = 0u;
    g->enqueue_retry_used = 0u;
    g->candidate = 255u;
    g->running_since = 0u;
    g->manual_hold_until = 0u;
}
void dna_guard_manual_selection(dna_guard *g, uint32_t now) {
    if (!g->enabled) return;
    g->restore_pending = 0u;
    g->command_sent = 0u;
    g->restore_failed = 0u;
    g->enqueue_retry_used = 0u;
    g->candidate = 255u;
    g->running_since = 0u;
    g->manual_hold_until = now + MANUAL_SETTLE_MS;
}
int dna_guard_restore(dna_guard *g,uint32_t now,uint16_t rpm,uint8_t actual,uint8_t *target){if(!g->enabled||!g->restore_pending)return 0;if(rpm<=RUNNING_RPM){g->running_since=0;return 0;}if(!g->running_since){g->running_since=now?now:1;return 0;}if((uint32_t)(now-g->running_since)<SETTLE_MS)return 0;g->restore_pending=0;if(!valid(g->saved)||!valid(actual)||g->saved==actual)return 0;g->command_sent=1;g->restore_target=g->saved;g->command_at=now?now:1;*target=g->saved;return 1;}
int dna_guard_observe(dna_guard *g,uint32_t now,uint16_t rpm,uint8_t actual,uint8_t *persist){if(!g->enabled||rpm<=RUNNING_RPM||!valid(actual)||g->restore_pending)return 0;if(g->manual_hold_until && (int32_t)(now-g->manual_hold_until)<0)return 0;g->manual_hold_until=0;if(g->command_sent){if((uint32_t)(now-g->command_at)<HOLD_MS)return 0;g->command_sent=0;g->candidate=255;if(actual!=g->restore_target)g->restore_failed=1;}/* A CAN mode transition alone does not prove driver intent.
 * On failed restoration, inhibit learning for this power cycle.
 * A deliberate Setup OFF -> ON action re-arms learning via set_enabled(). */
if(g->restore_failed)return 0;
if(g->candidate!=actual){g->candidate=actual;g->candidate_since=now;return 0;}if((uint32_t)(now-g->candidate_since)<STABLE_MS||actual==g->saved)return 0;g->saved=actual;*persist=actual;return 1;}

/* Re-arm once if the BH request never entered the UART TX queue.
 * Do not claim a command was sent when the queue was full. */
void dna_guard_restore_enqueue_failed(dna_guard *g) {
    if (g->command_sent) {
        g->command_sent=0u;
        g->running_since=0u;
        g->candidate=255u;
        if (!g->enqueue_retry_used) {
            g->enqueue_retry_used=1u;
            g->restore_pending=1u;
        } else {
            g->restore_pending=0u;
        }
    }
}

static dna_guard guard;
static dna_journal_state journal;
static uint8_t initialized;
static uint8_t dryrun_enabled;
static uint8_t seen_384;
static uint32_t last_384_ms;
static uint8_t confirmed_return_pending;
static volatile uint8_t manual_selection_pending;
void dna_stageg_note_384(uint32_t millis) { seen_384=1u; last_384_ms=millis; }
void dna_stageg_note_ignition_return(void) { if (initialized) confirmed_return_pending=1u; }
void dna_stageg_note_manual_selection(void) { if (initialized) manual_selection_pending=1u; }
void dna_stageg_restore_enqueue_failed(void) {
    if (initialized) dna_guard_restore_enqueue_failed(&guard);
}
int dna_stageg_init(uint8_t enabled) {
    if (dna_journal_load(&dna_stm32_flash_ops, &journal)) return -1;
    dna_guard_init(&guard, journal.valid ? journal.mode : 0xFFu, enabled);
    seen_384 = 0u;
    confirmed_return_pending = 0u;
    manual_selection_pending = 0u;
    dryrun_enabled = enabled ? 1u : 0u;
    initialized = 1u;
    return 0;
}
void dna_stageg_set_enabled(uint8_t enabled) {
    dryrun_enabled = enabled ? 1u : 0u;
    if (initialized) dna_guard_set_enabled(&guard, enabled);
}
/* Read-only eligibility check. No guard mutation, flash writes, or CAN TX. */
int dna_stageg_dryrun_target(uint8_t *target) {
    if (!initialized || !dryrun_enabled || !journal.valid || !target) return 0;
    if (journal.mode != 0x00u && journal.mode != 0x08u &&
        journal.mode != 0x10u && journal.mode != 0x30u) return 0;
    *target = journal.mode;
    return 1;
}
int dna_stageg_tick(uint32_t millis, uint16_t rpm, uint8_t observed_mode, uint8_t *restore_target) {
    uint8_t save_mode = 0xFFu;
    if (!initialized || !restore_target) return -1;
    if (confirmed_return_pending) {
        confirmed_return_pending = 0u;
        dna_guard_arm_cycle(&guard);
    }
    if (manual_selection_pending) {
        manual_selection_pending = 0u;
        dna_guard_manual_selection(&guard, millis);
        stageg_ign_diag_note_manual_selection();
    }
    /* Never restore or learn from an uninitialized/stale CAN mode. */
    if (!seen_384 || (uint32_t)(millis-last_384_ms)>500u) return 0;
    if (dna_guard_restore(&guard, millis, rpm, observed_mode, restore_target)) {
        stageg_ign_diag_note_guard_request(*restore_target);
        return 1;
    }
    if (dna_guard_observe(&guard, millis, rpm, observed_mode, &save_mode)) {
        if (dna_journal_save(&dna_stm32_flash_ops, &journal, save_mode) < 0) {
            /* Guard updated its RAM copy before flash commit. Restore the
             * last committed value so the same stable mode can be retried. */
            guard.saved = journal.valid ? journal.mode : 0xFFu;
            return -2;
        }
        return 2;
    }
    return 0;
}
#endif

/* @Shadin: STM32 DNA Journal Flash adapter relocated from dna_journal_stm32.c; implementation unchanged. */
#include "dna_journal.h"
#include "compile_time_defines.h"
#if defined(C1baccable)
#include "stm32f0xx_hal.h"
#include <string.h>

/* These pages are reserved candidates; validate against every flash consumer
 * and linker memory map before enabling firmware writes. */
static int in_reserved_pages(uint32_t address, size_t length)
{
    uint64_t start = (uint64_t)address;
    uint64_t end = start + (uint64_t)length;
    return (start >= DNA_JOURNAL_PAGE_A && end <= (uint64_t)DNA_JOURNAL_PAGE_A + DNA_JOURNAL_PAGE_SIZE) ||
           (start >= DNA_JOURNAL_PAGE_B && end <= (uint64_t)DNA_JOURNAL_PAGE_B + DNA_JOURNAL_PAGE_SIZE);
}
static int flash_read(uint32_t address, uint8_t *out, size_t length)
{
    if (!out || !in_reserved_pages(address, length)) return -1;
    memcpy(out, (const void *)(uintptr_t)address, length);
    return 0;
}
static int flash_halfword(uint32_t address, uint16_t value)
{
    if ((address & 1u) || !in_reserved_pages(address, 2u)) return -1;
    if (HAL_FLASH_Unlock() != HAL_OK) return -1;
    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, value);
    HAL_FLASH_Lock();
    if (status != HAL_OK) return -1;
    return (*(volatile const uint16_t *)(uintptr_t)address == value) ? 0 : -1;
}
static int flash_erase(uint32_t address)
{
    if ((address != DNA_JOURNAL_PAGE_A && address != DNA_JOURNAL_PAGE_B) ||
        !in_reserved_pages(address, DNA_JOURNAL_PAGE_SIZE)) return -1;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages = 1;
    if (HAL_FLASH_Unlock() != HAL_OK) return -1;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    if (status != HAL_OK) return -1;
    for (uint32_t offset = 0; offset < DNA_JOURNAL_PAGE_SIZE; offset += 2u)
        if (*(volatile const uint16_t *)(uintptr_t)(address + offset) != 0xFFFFu) return -1;
    return 0;
}
const dna_flash_ops dna_stm32_flash_ops = {flash_read, flash_halfword, flash_erase};

#endif /* C1baccable */
