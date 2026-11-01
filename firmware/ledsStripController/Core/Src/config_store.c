/* Stage G transactional C1 settings storage. Two 2-KiB pages, legacy read-only fallback.
 * Never erase the old settings page (0x0801F800). Both copies remain independently valid.
 * Commit halfword is written last. CRC covers all 34 parameter halfwords.
 */
#include "compile_time_defines.h"
#if defined(C1baccable)
#include "config_store.h"
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
static uint16_t crc16(const uint16_t *data) {
    uint16_t crc=0xFFFFu;
    for(unsigned i=0;i<CFG_TOTAL;i++) for(unsigned j=0;j<2;j++) {
        uint8_t byte=(uint8_t)(data[i]>>(8u*j)); crc ^= (uint16_t)byte<<8;
        for(unsigned k=0;k<8;k++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1);
    }
    return crc;
}
static int read_page(uint32_t base, uint16_t *out, uint32_t *seq) {
    if(HW(base)!=CFG_MAGIC || HW(base+CFG_FMT_OFFSET)!=CFG_FORMAT ||
       HW(base+CFG_COUNT_OFFSET)!=CFG_TOTAL || HW(base+CFG_COMMIT_OFFSET)!=CFG_COMMIT) return 0;
    uint16_t values[CFG_TOTAL];
    for(unsigned i=0;i<CFG_TOTAL;i++) values[i]=HW(base+CFG_DATA_OFFSET+4u*i);
    if(crc16(values)!=HW(base+CFG_CRC_OFFSET)) return 0;
    if(out) for(unsigned i=0;i<CFG_TOTAL;i++) out[i]=values[i];
    if(seq) *seq=(uint32_t)HW(base+CFG_SEQ_OFFSET) | ((uint32_t)HW(base+CFG_SEQ_OFFSET+2u)<<16);
    return 1;
}
static uint32_t active_page(uint32_t *sequence) {
    uint32_t a=0,b=0; int va=read_page(CFG_A,NULL,&a), vb=read_page(CFG_B,NULL,&b);
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
        read_cache_base=active_page(NULL);
        read_cache_valid=1u;
    }
    return HW(read_cache_base ? read_cache_base+CFG_DATA_OFFSET+4u*index : CFG_LEGACY+4u*index);
}
static int program(uint32_t address, uint16_t value) {
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,address,value)==HAL_OK && HW(address)==value;
}
uint8_t config_store_save(const uint16_t *values, uint8_t count) {
    if(!values || count!=CFG_TOTAL) return 251;
    uint32_t seq=0, active=active_page(&seq);
    uint32_t target=(active==CFG_A)?CFG_B:CFG_A;
    // Do not touch the valid copy until the new copy has been committed and verified.
    FLASH_EraseInitTypeDef erase={0}; uint32_t page_error=0;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.PageAddress=target;
    erase.NbPages=1;
    if(HAL_FLASH_Unlock()!=HAL_OK) return 254;
    if(HAL_FLASHEx_Erase(&erase,&page_error)!=HAL_OK) {HAL_FLASH_Lock();return 254;}
    uint32_t next=seq+1u;
    int ok=program(target,CFG_MAGIC) && program(target+CFG_FMT_OFFSET,CFG_FORMAT) &&
           program(target+CFG_SEQ_OFFSET,(uint16_t)next) &&
           program(target+CFG_SEQ_OFFSET+2u,(uint16_t)(next>>16)) &&
           program(target+CFG_COUNT_OFFSET,CFG_TOTAL) &&
           program(target+CFG_CRC_OFFSET,crc16(values));
    for(unsigned i=0;i<CFG_TOTAL && ok;i++) ok=program(target+CFG_DATA_OFFSET+4u*i,values[i]);
    if(ok) ok=program(target+CFG_COMMIT_OFFSET,CFG_COMMIT);
    HAL_FLASH_Lock();
    if(!ok) return 255;
    uint16_t verify[CFG_TOTAL]; uint32_t check=0;
    if(!read_page(target,verify,&check) || check!=next) return 252;
    for(unsigned i=0;i<CFG_TOTAL;i++) if(verify[i]!=values[i]) return 252;
    /* A successful commit switches the active copy: refresh on next read. */
    read_cache_valid=0u;
    return 0;
}
#endif
