
#include <stdint.h>
void test();

void SWIO_reset(int pin);

void SWIO_re_open();

uint32_t get_data(uint32_t addr);

void put_data(uint32_t addr, uint32_t data);