#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc32.h"
#include "encoder.h"

void outbound_encoder_init(
    encoder_msg_t* enc, const uint8_t* msg, const size_t len, const size_t slice_len) {
    enc->size   = sizeof(uint16_t) + len + sizeof(uint32_t);
    enc->buffer = malloc(enc->size);
    enc->crc32  = (uint32_t)-1;
    enc->len    = len;
    enc->index  = 0;
    enc->slice  = slice_len;

    //move the size into the buffer
    uint16_t* sbuff = (uint16_t*)enc->buffer;
    *sbuff          = enc->size;

    //move the data into the buffer
    memcpy(enc->buffer + sizeof(uint16_t), msg, len);

    //calculate and move the crc into the buffer
    enc->crc32      = xcrc32(enc->buffer, sizeof(uint16_t) + len, (uint32_t)-1);
    uint32_t* cbuff = (uint32_t*)(enc->buffer + sizeof(uint16_t) + len);
    *cbuff          = enc->crc32;
}

void inbound_encoder_init(encoder_msg_t* enc, const uint16_t size, const size_t slice_len) {
    enc->size   = size;
    enc->buffer = malloc(enc->size);
    //dont bother to initialize crc
    enc->len   = enc->size - sizeof(uint16_t) - sizeof(uint32_t);
    //to keep the interfact the same start at 0 though the first uint16_t size is already known
    enc->index = 0;
    enc->slice = slice_len;

    uint16_t* sbuff = (uint16_t*)enc->buffer;
    *sbuff          = enc->size;
}

void encoder_close(encoder_msg_t* enc) {
    memset(enc->buffer, 0, enc->size);
    free(enc->buffer);
    enc->buffer = 0;
}

int encoder_get_next(encoder_msg_t* enc, uint8_t* slice) {
    size_t i = 0;

    for (; i < enc->slice; ++i) {
        if (enc->index < enc->size) {
            slice[i] = enc->buffer[enc->index++];
        } else {
            break;
        }
    }

    return enc->slice - i;
}

void encoder_add_next(encoder_msg_t* enc, const uint8_t* slice) {
    size_t i = 0;
    for (; i < enc->slice; ++i) {
        if (enc->index < enc->size) {
            enc->buffer[enc->index++] = slice[i];
        } else {
            //no need to report on unused slice parts as the structure is filled
            break;
        }
    }
}

int encoder_verify(encoder_msg_t* enc) {
    uint32_t actual = xcrc32(enc->buffer, enc->size - sizeof(uint32_t), (uint32_t)-1);
    uint32_t* cbuff = (uint32_t*)(enc->buffer + enc->size - sizeof(uint32_t));
    enc->crc32      = *cbuff;

    return enc->crc32 == actual;
}

int encoder_finished(encoder_msg_t* enc) {
    return enc->index == enc->size;
}

void encoder_print(encoder_msg_t* enc) {
    printf("[message data]\nsize: %u\ndata: %.*s\ncrc32: 0x%X\n", enc->size, enc->len,
        enc->buffer + sizeof(uint16_t), enc->crc32);
    printf("[internal data]\nindex: %u\nslice: %u\nlen: %u\n", enc->index, enc->slice, enc->len);
    puts("[raw hex]");
    for (size_t i = 0; i < enc->size; ++i) {
        printf("%02X ", enc->buffer[i]);
    }
    puts("");
}

size_t transcode_size(uint8_t* buffer, size_t len) {
    size_t total = 0;
    for (;len;--len)
        if (!buffer[len] || buffer[len] == 0xFF)
            ++total;
    return total;
}
void transcode(uint8_t* dest, uint8_t* src, size_t len) {

}
