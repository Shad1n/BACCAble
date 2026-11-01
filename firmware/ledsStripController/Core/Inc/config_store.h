#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H
#include <stdint.h>
#define CONFIG_STORE_COUNT 34u
uint8_t config_store_save(const uint16_t *values, uint8_t count);
uint16_t config_store_read(uint8_t index);
#endif
