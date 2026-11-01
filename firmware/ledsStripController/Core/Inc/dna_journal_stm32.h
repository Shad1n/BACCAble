#ifndef DNA_JOURNAL_STM32_H
#define DNA_JOURNAL_STM32_H
#include "dna_journal.h"
/* C1 only: do not include this adapter in BH/C2 builds. */
extern const dna_flash_ops dna_stm32_flash_ops;
#endif
