/*
 * functions_Common.c
 *
 *  Created on: May 2, 2025
 *      Author: GauchoHP
 */

#include "functions_Common.h"
#include "debug.h"
#include <string.h>	// @netzmark: for strlen
#include <stdio.h> /* @Shadin: bounded DTC log records */


/*
 * @netzmark custom-printf improvement
 * floatToStr() + appendFloatRightAligned()
*/

/*
 * floatToStr()
 * Converts a floating-point value to a null-terminated ASCII string.
 *
 * - Performs manual decimal rounding according to the requested precision
 * - Suppresses negative zero and very small values (e.g. "-0.00")
 * - Outputs a fixed number of fractional digits
 * - Handles sign explicitly and writes into a bounded buffer
 *
 * Unlike printf():
 * - Does not perform automatic field width adjustment
 * - Does not apply padding or alignment rules internally
 * - Does not support format flags or scientific notation
 *
 * Note:
 * Field width, padding, and alignment are handled externally
 * (by appendFloatRightAligned()).
 */

void floatToStr(char* str, float num, uint8_t precision, uint8_t maxLen)
{
    if (maxLen == 0) return;

    // Fast lookup for precision 0-3 (max usable for standard float)
    static const float lsbTable[] = { 1.0f, 0.1f, 0.01f, 0.001f };
    if (precision > 3) precision = 3; //I assume that never need higher precision

    char* ptr = str;
    char* end = str + maxLen - 1; // Reserve space for '\0'

    float lsb = lsbTable[precision];
    float rounding = lsb / 2.0f;

    // --- Sign handling & Zero-clamping ---
    // Eliminates "-0.0" by checking if value is below displayable threshold
    if (num < -rounding) {
        if (ptr < end) *ptr++ = '-';
        num = -num + rounding;
    } else if (num < rounding) {
        num = 0.0f;
    } else {
        num += rounding;
    }

    uint32_t intPart = (uint32_t)num;
    float remainder = num - (float)intPart;

    // --- Integer part ---
    char temp[11];
    int intLen = 0;

    do {
        temp[intLen++] = (intPart % 10) + '0';
        intPart /= 10;
    } while (intPart && intLen < (int)sizeof(temp));

    while (intLen > 0 && ptr < end) {
        *ptr++ = temp[--intLen];
    }

    // --- Fractional part ---
    if (precision > 0 && ptr < end) {
        *ptr++ = '.';
        while (precision-- && ptr < end) {
            remainder *= 10.0f;
            int digit = (int)remainder;
            *ptr++ = (char)(digit + '0');
            remainder -= (float)digit;
        }
    }

    *ptr = '\0';
}


/*
 * appendFloatRightAligned()
 * Appends a float value into a transport buffer as a fixed-length field.
 * Total field length is predictable, matching the original Gaucho's intent (I guess).
 * Handles field width, left padding, right alignment, and visual layout.
 */

void appendFloatRightAligned(char* result,
                             uint8_t* index,
                             uint8_t resultMaxLen,
                             float value,
                             uint8_t precision,
                             uint8_t fieldWidth)
{
    /* Must always write exactly `fieldWidth` bytes
       to preserve fixed-length transport frames. */

    if (*index >= resultMaxLen)
        return;

    /* Invalid float → fill entire field */
    if (isnan(value) || isinf(value))
    {
        for (uint8_t i = 0; i < fieldWidth && *index < resultMaxLen; i++)
            result[(*index)++] = ' ';   // or '-' if visually preferred
        return;
    }

    char buf[16] = {0}; // instead of "char buf[32]
    floatToStr(buf, value, precision, sizeof(buf));
    //buf[15] = '\0';// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    uint8_t len = strlen(buf);

    /* Overflow → mark entire field */
    if (len > fieldWidth)
    {
        for (uint8_t i = 0; i < fieldWidth && *index < resultMaxLen; i++)
            result[(*index)++] = '#';
        return;
    }

    uint8_t pad = fieldWidth - len;

    /* Left padding (right alignment) */
    for (uint8_t i = 0; i < pad && *index < resultMaxLen; i++)
        result[(*index)++] = ' ';

    /* Write value */
    for (uint8_t i = 0; i < len && *index < resultMaxLen; i++)
        result[(*index)++] = buf[i];

    /* Safety guard: ensure exact fieldWidth bytes written */
    for (uint8_t written = pad + len;
         written < fieldWidth && *index < resultMaxLen;
         written++)
    {
        result[(*index)++] = ' ';
    }
}


uint8_t calculateCRC(uint8_t* data, uint8_t arraySize) {
	uint8_t crc = 0xFF;
	if(arraySize>1){
		//calculate sae_j1850 CRC-8 of the array (excluded last element, that will be used to store the final CRC)
		for (uint8_t i=0;i<arraySize-1;i++){
			crc ^= data[i];
			for (int i = 0; i < 8; ++i){
				crc = (crc & 0x80) ? (crc << 1) ^ 0x1D : crc << 1;
			}
		}
		return (crc ^ 0xFF); //return calculated checksum
	}
	return 0; //nothing to calculate
}


//System Clock Configuration

/* HSI/HSE configuration added by @netzmark - BEGINING */

#if !defined(HSE_ENABLED_FOR_UCAN)
// System Clock Configuration for HSI (Gaucho's ori code)
// It means this is Baccable/Canable standard clock configuration

void SystemClock_Config(void){
  HAL_Init();
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK){
    Error_Handler(2000);
  }

  // configure CRS to stabilize HSI48
  __HAL_RCC_CRS_CLK_ENABLE();

  RCC_CRSInitTypeDef crs = {0};
  crs.Prescaler = RCC_CRS_SYNC_DIV1;
  crs.Source = RCC_CRS_SYNC_SOURCE_USB;
  crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  crs.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  crs.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
  crs.HSI48CalibrationValue = 0x20;
  HAL_RCCEx_CRSConfig(&crs);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK){
    Error_Handler(1500);
  }

  //the following part is used by usb, used by canable

  // Set USB clock source to HSI48 (48 MHz)
  RCC_PeriphCLKInitTypeDef PeriphClkInit= {0};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK){
	  Error_Handler(1000);
  }
  __HAL_RCC_GPIOA_CLK_ENABLE();

}
#endif

#if defined(HSE_ENABLED_FOR_UCAN)
// Configuration the boards for HSE (external oscillator) using - BEGIN ; by @netzmark

void SystemClock_Config(void) {
  HAL_Init();
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** 1. HSE oscillator and PLL loop configuration
  * HSI48 disabled, HSE (8MHz) enabled
  * PLL: 8MHz * 6 = 48MHz
  */

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;             // External oscillator ON
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE; // external oscillator is set as the source for PLL
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;         // Multiplication x6 for dla 8MHz external oscillator (8*6=48)

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler(2000);
  }

  /** 2. Bus clock initialisation (SYSCLK = 48MHz z PLL)
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // System starts with PLL
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
    Error_Handler(1500);
  }

  /** 3. Setting clock source for USB device
  * Changed from HSI48 onto signal from PLL
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLLCLK; // USB gets stable clock from PLL(HSE)

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler(1000);
  }
   //__HAL_FLASH_PREFETCH_BUFFER_ENABLE(); // it seems to be not necessary as it is applied by HAL_init().
   __HAL_RCC_GPIOA_CLK_ENABLE();
}
#endif
//System Clock Configuration for HSE/HSI - END ; by @netzmark

void Error_Handler(uint16_t halfPeriod){
	//onboardLed_red_on();
	//LOGS("System error\r\n");
	//__disable_irq();
	//NVIC_SystemReset();



	RCC->AHBENR |=RCC_AHBENR_GPIOAEN; //ensure clock is enabled on port gpioA


	uint8_t tmpBool01=0;
	while (1){

		//now toggle leds without using HAL, to be more resilient
		if(tmpBool01){
			GPIOA->BSRR= GPIO_PIN_0 <<16; //set PA0 low (red led)

		}else{
			GPIOA->BSRR= GPIO_PIN_0 ; //set PA0 high (red led)
		}

		tmpBool01=!tmpBool01;

		for (volatile uint32_t i = 0; i < (12500*halfPeriod) ; i++){ //12500cycles=1msec
			__asm("nop");
		}
	}
}

// Disable all interrupts
void system_irq_disable(void){
	__disable_irq();
	__DSB();
	__ISB();
}


// Enable all interrupts
void system_irq_enable(void){
        __enable_irq();
}

void system_hex32(char *out, uint32_t val){
	char *p = out + 8;
	*p-- = 0;
	while (p >= out) {
		uint8_t nybble = val & 0x0F;
		if (nybble < 10)
			*p = '0' + nybble;
		else
			*p = 'A' + nybble - 10;
		val >>= 4;
		p--;
	}
}

/* USB MSC files reside in C2/BH local Flash, not in C1 settings Flash.
 * Update only when content differs. Never format on mount failure.
 * disk_write rejects any page touching the reserved persistent-data region. */
#ifdef ENABLE_USB_MASS_STORAGE
static void usb_storage_sync_text(const char *name, const char *expected) {
    FIL file;
    FRESULT result;
    UINT read_count = 0, written = 0;
    char current[48];
    size_t expected_len = strlen(expected);
    if (expected_len >= sizeof(current)) return;
    result = f_open(&file, name, FA_READ);
    if (result == FR_OK) {
        FSIZE_t old_size = f_size(&file);
        if (old_size == expected_len) {
            result = f_read(&file, current, sizeof(current), &read_count);
            f_close(&file);
            if (result == FR_OK && read_count == expected_len &&
                memcmp(current, expected, expected_len) == 0) return;
        } else {
            f_close(&file);
        }
    } else if (result != FR_NO_FILE) {
        return;
    }
    result = f_open(&file, name, FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK) return;
    result = f_write(&file, expected, expected_len, &written);
    f_close(&file);
    (void)result;
    (void)written;
}
#endif

void storage_init(void){
#ifdef ENABLE_USB_MASS_STORAGE
    /* Preserve the existing volume and its C2/BH label. Never auto-format. */
    if (f_mount(&fs, "", 1) == FR_OK) {
        usb_storage_sync_text("VERSION.TXT", _FW_VERSION);
        /* @Shadin: Retire the test file non-destructively; preserve VERSION.TXT. */
        (void)f_unlink("HELLO.TXT");
        /* One-time migration: remove obsolete file from the local C2/BH FAT volume.
         * FR_NO_FILE is normal on subsequent boots. No format or retry loop.
         * disk_write protects reserved Flash pages; deletion failure is nonfatal. */
        (void)f_unlink("BACCABLE.TXT");
    }
    f_unmount("");
#endif
}


/* @Shadin: Manual DTC event journal, using Gaucho's existing FatFs and UART.
 * Only BH stores events. USB Mass Storage remains read-only to the host.
 * The UART ISR copies complete frames into a one-frame inbox; all file I/O
 * runs in the main loop. No new implementation modules or Flash layout. */
#if defined(BHbaccable) && defined(ENABLE_USB_MASS_STORAGE)
#define DTC_LOG_FILE "DTC_LOG.TXT"
#define DTC_LOG_MAX_BYTES 24576u /* conservative bound, below reserved Flash area */
#define DTC_LOG_MAX_CODES 60u

typedef struct {
    uint8_t id, action, count[3], status[3], n, got;
    uint8_t codes[DTC_LOG_MAX_CODES][4];
    uint8_t valid, ready;
} DtcLogPending;
static DtcLogPending dtcLogRx;
static volatile uint8_t dtcLogInboxFull, dtcLogClearRequested;
static volatile uint8_t dtcLogInbox[12];
static uint8_t dtcLogLastId, dtcLogLastEnd, dtcLogHaveLast;
static uint32_t dtcLogNextSequence;
static uint8_t dtcLogSequenceLoaded;
void dtcLogReceiveIsr(const uint8_t *frame) {
    if (dtcLogInboxFull) return; /* sender retries on missing ACK */
    for (uint8_t i=0; i<12u; ++i) dtcLogInbox[i]=frame[i];
    dtcLogInboxFull=1u;
}
void dtcLogClearIsr(void) { dtcLogClearRequested=1u; }

static void dtcLogAck(uint8_t id, uint8_t index, uint8_t status) {
    uint8_t frame[6] = {C1BusID,C1cmdDtcLogAck,id,index,status,0u};
    frame[5]=(uint8_t)(frame[0]^frame[1]^frame[2]^frame[3]^frame[4]^0x5Au);
    (void)tryAddToUARTSendQueue(frame,sizeof(frame));
}
static void dtcLogClearAck(uint8_t success) {
    uint8_t frame[4] = {C1BusID,C1cmdDtcLogClearResult,success,0u};
    frame[3]=(uint8_t)(frame[0]^frame[1]^frame[2]^0x5Au);
    (void)tryAddToUARTSendQueue(frame,sizeof(frame));
}
/* @Shadin: A blank line terminates a complete event. Keep reading old
 * END-marked logs, so an upgrade never requires clearing existing records. */
static uint8_t dtcLogNumber(const char *line, uint8_t len,
                            uint8_t *index, uint32_t *value) {
    uint32_t number=0u;
    uint8_t i=*index, first=i;
    while(i<len && line[i]>='0' && line[i]<='9') {
        uint32_t digit=(uint32_t)(line[i]-'0');
        if(number>(UINT32_MAX-digit)/10u)return 0u;
        number=number*10u+digit;
        ++i;
    }
    if(i==first || number==0u)return 0u;
    *index=i;*value=number;
    return 1u;
}
static FRESULT dtcLogLoadSequence(FIL *f) {
    uint8_t buf[64]; UINT n=0u;
    uint32_t max=0u, pending=0u;
    FSIZE_t position=0u, lastComplete=0u;
    char line[32];
    uint8_t used=0u, tooLong=0u, sawHeader=0u;
    FRESULT rc=f_lseek(f,0u);
    if(rc!=FR_OK)return rc;
    while((rc=f_read(f,buf,sizeof(buf),&n))==FR_OK && n!=0u) {
        for(UINT i=0u;i<n;++i) {
            ++position;
            if(buf[i]=='\n') {
                if(!tooLong && used==0u && pending!=0u) {
                    /* @Shadin: The empty separator line is the commit marker. */
                    if(pending>max)max=pending;
                    lastComplete=position;
                    pending=0u;
                } else if(!tooLong && used>=5u && memcmp(line,"END ",4u)==0) {
                    /* Legacy END #00000001 and END 1. remain readable. */
                    uint8_t index=(line[4]=='#')?5u:4u;
                    uint32_t value=0u;
                    if(dtcLogNumber(line,used,&index,&value) &&
                       ((index==used) ||
                        (index+1u==used && line[index]=='.'))) {
                        if(value>max)max=value;
                        lastComplete=position;
                        pending=0u;
                    }
                } else if(!tooLong && used>=14u) {
                    uint8_t index=0u;
                    uint32_t value=0u;
                    if(dtcLogNumber(line,used,&index,&value) &&
                       index+2u<used && line[index]=='.' && line[index+1u]==' ' &&
                       ((used-index-2u==11u && memcmp(line+index+2u,"Read faults",11u)==0) ||
                        (used-index-2u==12u && memcmp(line+index+2u,"Clear faults",12u)==0))) {
                        pending=value;
                        sawHeader=1u;
                    }
                }
                used=0u;tooLong=0u;
            } else if(!tooLong) {
                if(used<sizeof(line))line[used++]=(char)buf[i];
                else tooLong=1u;
            }
        }
    }
    if(rc!=FR_OK)return rc;
    /* @Shadin: Discard only an interrupted trailing append, never a complete
     * record. This prevents a partial event from being counted after restart. */
    if(lastComplete<f_size(f) && (lastComplete!=0u || sawHeader)) {
        rc=f_lseek(f,lastComplete);
        if(rc!=FR_OK)return rc;
        rc=f_truncate(f);
        if(rc!=FR_OK)return rc;
    }
    dtcLogNextSequence=(max==UINT32_MAX)?1u:max+1u;
    dtcLogSequenceLoaded=1u;
    return FR_OK;
}
static FRESULT dtcLogText(FIL *f,const char *line) {
    UINT done=0u; UINT len=(UINT)strlen(line);
    FRESULT result=f_write(f,line,len,&done);
    return (result==FR_OK && done==len)?FR_OK:FR_DISK_ERR;
}
static uint8_t dtcLogPersist(void) {
    if (usbConnectedToSlave) return 0u; /* never mutate FAT behind a mounted host */
    if (f_mount(&fs,"",1)!=FR_OK) return 2u;
    FIL f;
    FRESULT rc=f_open(&f,DTC_LOG_FILE,FA_READ|FA_WRITE|FA_OPEN_ALWAYS);
    if(rc!=FR_OK) {f_unmount("");return 2u;}
    if(!dtcLogSequenceLoaded && dtcLogLoadSequence(&f)!=FR_OK) {
        (void)f_close(&f);f_unmount("");return 2u;
    }
    /* The log stays below this cap even if the flash volume advertises 64 KiB.
     * The underlying disk_write() separately protects reserved pages. */
    if(f_size(&f)>DTC_LOG_MAX_BYTES-2400u) {
        (void)f_close(&f);f_unmount("");return 2u;
    }
    /* @Shadin: Insert a separator when appending to an older END-marked log.
     * New-format records already finish with an empty separator line. */
    FSIZE_t oldSize=f_size(&f);
    uint8_t separator=0u;
    if(oldSize!=0u) {
        char tail[2];UINT got=0u;
        UINT want=(oldSize>=2u)?2u:1u;
        rc=f_lseek(&f,oldSize-want);
        if(rc==FR_OK)rc=f_read(&f,tail,want,&got);
        if(rc==FR_OK && got!=want)rc=FR_DISK_ERR;
        if(rc==FR_OK)separator=(tail[want-1u]=='\n')?
            (uint8_t)(want<2u || tail[want-2u]!='\n'):2u;
    }
    if(rc==FR_OK)rc=f_lseek(&f,oldSize);
    if(rc==FR_OK && separator!=0u)
        rc=dtcLogText(&f,(separator==2u)?"\n\n":"\n");
    char line[100];
    if(rc==FR_OK) {
        (void)snprintf(line,sizeof(line),"%lu. %s\n",
                       (unsigned long)dtcLogNextSequence,
                       dtcLogRx.action=='R'?"Read faults":"Clear faults");
        rc=dtcLogText(&f,line);
    }
    if(rc==FR_OK && dtcLogRx.action=='C')
        rc=dtcLogText(&f,"  BCM/ECM/ABS: Request initiated\n");
    static const char *ecuName[3]={"BCM","ECM","ABS"};
    static const char *states[5]={"NOT_TESTED","WAIT","OK","TIMEOUT","ERROR"};
    if(rc==FR_OK && dtcLogRx.action=='R') {
        for(uint8_t ecu=0u;ecu<3u && rc==FR_OK;++ecu) {
            uint8_t status=(uint8_t)(dtcLogRx.status[ecu]&0x7Fu);
            if(status>4u)status=4u;
            const char *state=(status==2u && dtcLogRx.count[ecu]==0u)?"No faults":states[status];
            const char *trunc=(dtcLogRx.status[ecu]&0x80u)?" [TRUNCATED]":"";
            (void)snprintf(line,sizeof(line),"  %s: %s, %u recorded DTC(s)%s\n",
                ecuName[ecu],state,dtcLogRx.count[ecu],trunc);
            rc=dtcLogText(&f,line);
            for(uint8_t i=0u;i<dtcLogRx.n && rc==FR_OK;++i) {
                if(dtcLogRx.codes[i][0]!=ecu)continue;
                const uint8_t a=dtcLogRx.codes[i][1], b=dtcLogRx.codes[i][2];
                const uint8_t c=dtcLogRx.codes[i][3];
                const char type[4]={'P','C','B','U'};
                (void)snprintf(line,sizeof(line),"    %c%X%X%02X-%02X\n",type[a>>6],
                    (a>>4)&3u,a&15u,b,c);
                rc=dtcLogText(&f,line);
            }
        }
    }
    if(rc==FR_OK)rc=dtcLogText(&f,"\n"); /* @Shadin: blank line commits event */
    if(rc==FR_OK)rc=f_sync(&f);
    FRESULT closed=f_close(&f);
    f_unmount("");
    if(rc!=FR_OK || closed!=FR_OK) {
        /* @Shadin: Re-scan and trim a possible partial append on next use. */
        dtcLogSequenceLoaded=0u;
        return 2u;
    }
    ++dtcLogNextSequence;
    return 1u;
}

void dtcLogStorageService(void) {
    /* @Shadin: Do not add Flash delays to Sniffer/ELM327 streaming. */
    if(snifferInUse)return;
    /* Prioritize completing an already received transaction; do no disk I/O
     * while the host has mounted the exported read-only USB volume. */
    if(dtcLogRx.ready) {
        uint8_t result=dtcLogPersist();
        if(result==1u) {
            dtcLogLastId=dtcLogRx.id;
            dtcLogLastEnd=(uint8_t)(dtcLogRx.n+1u);
            dtcLogHaveLast=1u;
            dtcLogRx.ready=0u;dtcLogRx.valid=0u;
            dtcLogAck(dtcLogLastId,dtcLogLastEnd,0u);
        } else if(result==2u) {
            dtcLogAck(dtcLogRx.id,(uint8_t)(dtcLogRx.n+1u),2u);
            dtcLogRx.ready=0u;dtcLogRx.valid=0u;
        }
        return;
    }
    if(dtcLogClearRequested) {
        dtcLogClearRequested=0u;
        if(usbConnectedToSlave || dtcLogRx.valid) {dtcLogClearAck(0u);return;}
        if(f_mount(&fs,"",1)!=FR_OK) {dtcLogClearAck(0u);return;}
        FRESULT rc=f_unlink(DTC_LOG_FILE);
        f_unmount("");
        if(rc==FR_OK || rc==FR_NO_FILE) {
            dtcLogSequenceLoaded=0u;
            dtcLogClearAck(1u);
        } else dtcLogClearAck(0u);
        return;
    }
    uint8_t b[12];
    uint32_t mask=__get_PRIMASK();__disable_irq();
    uint8_t have=dtcLogInboxFull;
    if(have) {for(uint8_t i=0;i<12u;++i)b[i]=dtcLogInbox[i];dtcLogInboxFull=0u;}
    __set_PRIMASK(mask);
    if(!have || b[0]!=BhBusID || b[1]!=BHcmdDtcLogFrame)return;
    uint8_t check=0x5Au;
    for(uint8_t i=0u;i<11u;++i)check^=b[i];
    if(check!=b[11])return;
    uint8_t id=b[2], index=b[3];
    if(index==0u) {
        if(dtcLogRx.valid) {
            if(dtcLogRx.id==id)dtcLogAck(id,0u,0u);
            else dtcLogAck(id,0u,1u);
            return;
        }
        if(b[4]!='R' && b[4]!='C')return;
        uint8_t total=(uint8_t)(b[5]+b[6]+b[7]);
        if(total>DTC_LOG_MAX_CODES || (b[4]=='C' && total!=0u))return;
        memset(&dtcLogRx,0,sizeof(dtcLogRx));
        dtcLogRx.id=id;dtcLogRx.action=b[4];dtcLogRx.n=total;
        for(uint8_t i=0;i<3u;++i) {
            dtcLogRx.count[i]=b[5u+i];dtcLogRx.status[i]=b[8u+i];
            if(dtcLogRx.count[i]>20u || (dtcLogRx.status[i]&0x7Fu)>4u) {dtcLogRx.valid=0u;return;}
        }
        dtcLogRx.valid=1u;
        dtcLogAck(id,0u,0u);
    } else if(dtcLogRx.valid && dtcLogRx.id==id) {
        if(index<=dtcLogRx.n && index==dtcLogRx.got+1u) {
            uint8_t ecu=b[4];
            if(ecu>2u)return;
            dtcLogRx.codes[dtcLogRx.got][0]=ecu;
            dtcLogRx.codes[dtcLogRx.got][1]=b[5];
            dtcLogRx.codes[dtcLogRx.got][2]=b[6];
            dtcLogRx.codes[dtcLogRx.got][3]=b[7];
            ++dtcLogRx.got;
            dtcLogAck(id,index,0u);
        } else if(index<=dtcLogRx.got) dtcLogAck(id,index,0u);
        else if(index==dtcLogRx.n+1u && dtcLogRx.got==dtcLogRx.n && b[4]==0xEEu)
            dtcLogRx.ready=1u; /* ACK only after durable append */
    } else if(dtcLogHaveLast && id==dtcLogLastId && index==dtcLogLastEnd) {
        dtcLogAck(id,index,0u); /* duplicated terminal frame, never append twice */
    }
}
#else
void dtcLogStorageService(void) { }
#endif

//sniffer function 24/08/2026 - BEGIN
#if defined(C1baccable) || defined(C2baccable) || defined(BHbaccable)
//Writes one 16 byte frame into the ring buffer. Called from the can rx path, must stay short.
//SNIFFER_BUFFER_SIZE is a multiple of SNIFFER_FRAME_SIZE and snifferRingHead only ever advances
//by whole frames, so it stays frame aligned: a single frame never wraps past the end of the
//array, and the wrap mask only needs to be applied once, after the whole frame has been written.
void snifferPushFrame(CAN_RxHeaderTypeDef *snifferRxHeader, uint8_t *snifferRxData){
	uint16_t snifferBase;
	uint32_t snifferTimeStamp;
	uint32_t snifferCanId;
	uint8_t  snifferDlc;
	uint8_t  snifferByteIndex;

	//as soon as there is room for the marker plus one more frame, report the frames lost during the overflow
	if(snifferDroppedFrames>0 && snifferRingCount<=(SNIFFER_BUFFER_SIZE-(2*SNIFFER_FRAME_SIZE))){
		snifferBase=snifferRingHead;
		snifferTimeStamp=currentTime;
		snifferRingBuffer[snifferBase+0]=SNIFFER_OVERFLOW_MARKER;
		snifferRingBuffer[snifferBase+1]=(uint8_t)(snifferTimeStamp);
		snifferRingBuffer[snifferBase+2]=(uint8_t)(snifferTimeStamp>>8);
		snifferRingBuffer[snifferBase+3]=(uint8_t)(snifferTimeStamp>>16);
		snifferRingBuffer[snifferBase+4]=(uint8_t)(snifferDroppedFrames);
		snifferRingBuffer[snifferBase+5]=(uint8_t)(snifferDroppedFrames>>8);
		snifferRingBuffer[snifferBase+6]=0;
		snifferRingBuffer[snifferBase+7]=0;
		for(snifferByteIndex=8; snifferByteIndex<SNIFFER_FRAME_SIZE; snifferByteIndex++){
			snifferRingBuffer[snifferBase+snifferByteIndex]=0;
		}
		snifferRingHead=(snifferBase+SNIFFER_FRAME_SIZE)&SNIFFER_BUFFER_MASK;
		snifferRingCount+=SNIFFER_FRAME_SIZE;
		snifferDroppedFrames=0;
	}

	//no room left for a whole frame: count it as lost and give up, the marker will report it later
	if(snifferRingCount>(SNIFFER_BUFFER_SIZE-SNIFFER_FRAME_SIZE)){
		if(snifferDroppedFrames<0xFFFF) snifferDroppedFrames++;
		return;
	}

	snifferDlc=snifferRxHeader->DLC;
	if(snifferDlc>8) snifferDlc=8;
	if(snifferRxHeader->IDE==CAN_ID_EXT){
		snifferCanId=snifferRxHeader->ExtId;
	}else{
		snifferCanId=snifferRxHeader->StdId;
	}
	snifferTimeStamp=currentTime;
	snifferBase=snifferRingHead;

	snifferRingBuffer[snifferBase+0]=SNIFFER_START_NIBBLE|snifferDlc;
	snifferRingBuffer[snifferBase+1]=(uint8_t)(snifferTimeStamp);
	snifferRingBuffer[snifferBase+2]=(uint8_t)(snifferTimeStamp>>8);
	snifferRingBuffer[snifferBase+3]=(uint8_t)(snifferTimeStamp>>16);
	snifferRingBuffer[snifferBase+4]=(uint8_t)(snifferCanId);
	snifferRingBuffer[snifferBase+5]=(uint8_t)(snifferCanId>>8);
	snifferRingBuffer[snifferBase+6]=(uint8_t)(snifferCanId>>16);
	snifferRingBuffer[snifferBase+7]=(uint8_t)(snifferCanId>>24);
	for(snifferByteIndex=0; snifferByteIndex<8; snifferByteIndex++){
		snifferRingBuffer[snifferBase+8+snifferByteIndex]=(snifferByteIndex<snifferDlc)?snifferRxData[snifferByteIndex]:0; //zero padding above DLC
	}
	snifferRingHead=(snifferBase+SNIFFER_FRAME_SIZE)&SNIFFER_BUFFER_MASK;
	snifferRingCount+=SNIFFER_FRAME_SIZE;
}

//Moves buffered bytes to usb. Never waits on the usb: if the endpoint is busy we retry on the next loop.
void snifferFlush(void){
	uint16_t snifferSendLength;
	uint16_t snifferContiguousBytes;

	if(snifferUsbInited==0) return;
	if(snifferRingCount==0) return;
	if(hUsbDeviceFS.pClassData==NULL) return; //usb not enumerated yet
	if(((USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData)->TxState) return; //endpoint busy: do not enter the busy wait inside CDC_Transmit_FS

	snifferSendLength=snifferRingCount;
	//a partial buffer is sent only after a while, so that on a busy bus we always send full 64 byte packets
	if(snifferSendLength<SNIFFER_USB_CHUNK && (currentTime-snifferLastFlushTime)<SNIFFER_FLUSH_TIMEOUT_MS) return;
	if(snifferSendLength>SNIFFER_USB_CHUNK) snifferSendLength=SNIFFER_USB_CHUNK;
	//never read across the end of the ring: the remaining part goes out on the next flush
	snifferContiguousBytes=SNIFFER_BUFFER_SIZE-snifferRingTail;
	if(snifferSendLength>snifferContiguousBytes) snifferSendLength=snifferContiguousBytes;

	if(CDC_Transmit_FS(&snifferRingBuffer[snifferRingTail], snifferSendLength)==USBD_OK){
		snifferRingTail=(snifferRingTail+snifferSendLength)&SNIFFER_BUFFER_MASK;
		snifferRingCount-=snifferSendLength;
		snifferLastFlushTime=currentTime;
	}
}

//Starts the function. Only ram is touched here, so this is safe to call from the uart interrupt as well:
//the usb bring up blocks for a few milliseconds and is deferred to snifferUsbStartIfRequested().
void snifferStart(void){
	snifferRingHead=0;
	snifferRingTail=0;
	snifferRingCount=0;
	snifferDroppedFrames=0;
	snifferLastFlushTime=currentTime;
	snifferUsbStartRequested=1;
	snifferInUse=1;
	snifferActivationConfirmed=0;
	snifferActivationTime=currentTime; //starts the host detection window (SNIFFER_ACTIVATION_TIMEOUT_MS), checked by snifferCheckActivationTimeout()
}

//Fully powers usb down: releases the D+ pull-up, tears down the class and the low level driver, and (on
//C2/BH) tells C1 this board's usb is no longer configured. This blocks for up to ~260ms (HAL_Delay plus the
//teardown itself), so it must only ever be called from the main loop, never from an interrupt - both call
//sites below (snifferUsbShutdownIfRequested and snifferCheckActivationTimeout) already run from there.
static void snifferUsbShutdown(void){
	if(snifferUsbInited==0) return; //already down, or never was up
	if(hUsbDeviceFS.pData!=NULL){
		HAL_PCD_DevDisconnect((PCD_HandleTypeDef*)hUsbDeviceFS.pData); //release the D+ pull-up: nothing is left attached from the host's point of view
		HAL_Delay(250);
	}
	if(hUsbDeviceFS.pClassData!=NULL){
		USBD_DeInit(&hUsbDeviceFS);
	}else{
		USBD_LL_Stop(&hUsbDeviceFS);
		USBD_LL_DeInit(&hUsbDeviceFS);
	}
	snifferUsbInited=0; //usb is fully off: a later activation must bring it back up from scratch

	#if defined(C2baccable) || defined(BHbaccable)
		//usbConnectedToSlave is this board's OWN "my usb is connected" flag, set by usbd_storage_if.c: with the
		//usb now off there is no pen drive/sniffer session left on this board to protect from sleep. C1's own
		//instance of the same named variable means something entirely different (a slave told me its usb is
		//connected) and must never be guessed from here - only the C1usbConnectedFrom/DisconnectedFrom commands, received over
		//uart, are allowed to change it, hence the notification right below. //sniffer function 24/08/2026
		usbConnectedToSlave=0;
		//own command per sender, so C1 tracks the two slaves independently, see uart.c //sniffer function 24/08/2026
		#if defined(C2baccable)
			uint8_t tmpArrUsbDisc[2]={C1BusID,C1usbDisconnectedFromC2};
		#else
			uint8_t tmpArrUsbDisc[2]={C1BusID,C1usbDisconnectedFromBH};
		#endif
		addToUARTSendQueue(tmpArrUsbDisc, 2);
	#endif
}

//Safe to call from the uart interrupt: only ram is touched here, the actual usb shutdown (which blocks) is
//deferred to snifferUsbShutdownIfRequested(), called from the main loop only - mirroring how snifferStart()
//defers its own usb bring up to snifferUsbStartIfRequested().
void snifferStop(void){
	snifferInUse=0;
	snifferActivationConfirmed=0;
	snifferUsbShutdownRequested=1;
}

//Brings usb down if snifferStop() requested it. Called from the main loop only: snifferUsbShutdown() waits on
//HAL_Delay().
void snifferUsbShutdownIfRequested(void){
	if(snifferUsbShutdownRequested==0) return;
	snifferUsbShutdownRequested=0;
	snifferUsbShutdown();
}

//Called every main loop iteration while the function is on. snifferActivationTime is a rolling "last time we
//saw the host" timestamp, refreshed every time dev_state reads CONFIGURED: if it falls behind by more than
//SNIFFER_ACTIVATION_TIMEOUT_MS, usb is fully powered down and we go back to being a plain baccable. This
//covers both the initial activation (host never showed up at all) and a later disconnect (host was there and
//went away): this mcu has no vbus sensing, so a genuine unplug and a routine host side usb suspend are
//indistinguishable to the peripheral - both are just "no bus activity" - and a sustained absence is the best
//available signal for either. The saved preference on flash is left untouched: the next power up, wake up, or
//menu toggle will attempt activation again the same way.
void snifferCheckActivationTimeout(void){
	if(snifferInUse==0) return;
	if(hUsbDeviceFS.dev_state==USBD_STATE_CONFIGURED){
		if(snifferActivationConfirmed==0){
		snifferActivationConfirmed=1;
			#if defined(C2baccable) || defined(BHbaccable)
				//Tell C1 this board's usb session is genuinely alive, once, the first time it is confirmed.
				//Without this, C1 only knows about this board's usb from STORAGE_Init_FS() (usbd_storage_if.c),
				//a callback of the pen drive class: once the sniffer switches this board away from mass storage
				//to cdc, that class is gone for good and its notification can never fire again for this board.
				//Left unnotified, C1's own usbConnectedToSlave (see lowConsume.c) would depend only on C1's own
				//local sniffer session, which can legitimately time out on its own (nobody plugged into C1's
				//port) while this board's session is still genuinely in use - and C1 would put everyone to
				//sleep despite this board still being actively read by a host. //sniffer function 24/08/2026
				usbConnectedToSlave=1;
				//own command per sender, so C1 tracks the two slaves independently, see uart.c //sniffer function 24/08/2026
				#if defined(C2baccable)
					uint8_t tmpArrUsbConn[2]={C1BusID,C1usbConnectedFromC2};
				#else
					uint8_t tmpArrUsbConn[2]={C1BusID,C1usbConnectedFromBH};
				#endif
				addToUARTSendQueue(tmpArrUsbConn, 2);
			#endif
		}
		snifferActivationTime=currentTime;
		return;
	}
	if(currentTime-snifferActivationTime<SNIFFER_ACTIVATION_TIMEOUT_MS) return; //still within the grace window

	snifferInUse=0;
	snifferActivationConfirmed=0;
	snifferUsbShutdown(); //already running from the main loop here, safe to call directly
}

//Brings the usb up as cdc. Called from the main loop only: MX_USB_DEVICE_Init() waits on HAL_Delay().
void snifferUsbStartIfRequested(void){
	if(snifferUsbStartRequested==0) return;
	snifferUsbStartRequested=0;
	if(snifferUsbInited) return; //already running as cdc, nothing to do until the next power cycle. this is a one shot bring up: no retry if the host never configures us.

	//Any usb stack already running must be torn down before we come back as cdc. This is driven by the actual
	//state of the stack, not by the build flavor: besides the pen drive on C2/BH, C1 too can already have the usb
	//up at boot (main.c does that whenever ACT_AS_CANABLE, DEBUG_MODE, ENABLE_USB_MASS_STORAGE or
	//ACT_AS_SCHIZZAFORTE_SERIAL_CONTROLLER is defined). Without this teardown MX_USB_DEVICE_Init() would
	//re-initialize on top of a live stack, and HAL_PCD_Init() would skip HAL_PCD_MspInit() because the pcd state
	//is not HAL_PCD_STATE_RESET, leaving the peripheral half re-initialized and the host with no clean re-attach.
	//pClass is NULL only when MX_USB_DEVICE_Init() was never called at all, and then there is nothing to tear down.
	if(hUsbDeviceFS.pClass!=NULL){
		//The teardown below (USBD_DeInit / USBD_LL_Stop+DeInit) resets and powers down the usb core, but never
		//clears BCDR_DPPU: the D+ pull-up stays asserted the whole time. From the host's point of view the device
		//never actually detaches, it just goes quiet and comes back later announcing a completely different class
		//(pen drive -> cdc) without ever having been unplugged. Most usb stacks, Android included, do not reliably
		//re-enumerate a class change that was never preceded by a real disconnect. So the pull-up is explicitly
		//released here and held low long enough for the host to notice the removal, before anything else runs.
		if(hUsbDeviceFS.pData!=NULL){
			HAL_PCD_DevDisconnect((PCD_HandleTypeDef*)hUsbDeviceFS.pData);
			HAL_Delay(250);
		}

		//pClassData is allocated by the class Init(), which only runs when the host sends SET_CONFIGURATION: with no
		//cable attached (or a cable that never enumerated us) it is still NULL. USBD_DeInit() would then call
		//pClass->DeInit() -> MSC_BOT_DeInit(), which dereferences pClassData without any NULL check and writes to it,
		//hitting address 0x00000008 (flash alias, not writable) and hard faulting. So the class teardown is done only
		//when the class was actually initialized, while the low level driver is torn down either way.
		//note: USBD_DeInit() already performs USBD_LL_Stop()+USBD_LL_DeInit() internally, so it is not called twice here.
		if(hUsbDeviceFS.pClassData!=NULL){
		USBD_DeInit(&hUsbDeviceFS);
		}else{
			USBD_LL_Stop(&hUsbDeviceFS);
			USBD_LL_DeInit(&hUsbDeviceFS);
		}
	}
	usbdDescSelectCdcMode(); //descriptors must answer as cdc before the host re-enumerates us
	MX_USB_DEVICE_Init();
	snifferUsbInited=1;
}
#endif
//sniffer function 24/08/2026 - END
