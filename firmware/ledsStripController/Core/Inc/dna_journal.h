#ifndef DNA_JOURNAL_H
#define DNA_JOURNAL_H
#include <stdint.h>
#include <stddef.h>
#define DNA_JOURNAL_PAGE_SIZE 2048u
#define DNA_JOURNAL_SLOT_SIZE 16u
#define DNA_JOURNAL_PAGE_A 0x0801D800u
#define DNA_JOURNAL_PAGE_B 0x0801E000u
/* HAL-independent core. Port functions must implement atomic 16-bit programming,
 * page erase and 16-bit reads, and verify addresses against the C1 memory map. */
typedef struct {
    int (*read)(uint32_t address, uint8_t *dst, size_t len);
    int (*program_halfword)(uint32_t address, uint16_t value);
    int (*erase_page)(uint32_t address);
} dna_flash_ops;
typedef struct { uint8_t mode; uint32_t sequence; uint8_t valid; } dna_journal_state;
int dna_journal_load(const dna_flash_ops *ops, dna_journal_state *state);
int dna_journal_save(const dna_flash_ops *ops, dna_journal_state *state, uint8_t mode);
/* @Shadin: STM32 adapter declaration consolidated; implementation remains separate. */
#if defined(C1baccable)
extern const dna_flash_ops dna_stm32_flash_ops;
#endif
#endif
