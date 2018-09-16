#include <stdlib.h>
       #include <string.h>

#include "crc32.h"
#include "encoder.h"

void encoder_init(encoder_msg_t* enc, uint8_t* msg, size_t len, size_t slice_len) {
    enc->size   = sizeof(uint16_t) + len + sizeof(uint32_t);
    enc->buffer = malloc(enc->size);
    enc->crc32  = (uint32_t)-1;
    enc->len    = len;
    enc->index  = 0;
    enc->slice  = slice_len;
    memcpy(enc->buffer + sizeof(uint16_t), msg, len);
}

void encoder_close(encoder_msg_t* enc) {
    memset(enc->buffer, 0, enc->size);
    free(enc->buffer);
    enc->buffer = 0;
}

void encoder_get_next(encoder_msg_t* enc, uint8_t* slice) {}

void encoder_add_next(encoder_msg_t* enc, uint8_t* slice) {}

int encoder_verify(encoder_msg_t* enc) {
    //check the CRC
    return 0;
}

int encoder_finished(encoder_msg_t* enc) {
    return enc->index == enc->len;
}
