#pragma once
#include <stdint.h>

//extract from source file:
//init is typically 0xffffffff but can be carriend from previous runs to allow composition of multiple blocks
uint32_t xcrc32(const uint8_t *buf, int len, uint32_t init);
