#include "dna_journal_stm32.h"
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
