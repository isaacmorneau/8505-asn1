#pragma once
#include <stdint.h>
#include <stddef.h>

uint8_t *base64_encode(const uint8_t *src, size_t len, size_t *out_len);
uint8_t *base64_decode(const uint8_t *src, size_t len, size_t *out_len);
