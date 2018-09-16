#pragma once

#include <sys/types.h>
#include <stdint.h>

typedef struct {
    uint16_t size;//total size of this message (includes data crc and this number) in bytes
    //layout of buffer
    //[size][data][crc32]
    uint8_t * buffer;//what we are sending
    uint32_t crc32;//ensure there was no corruption
    size_t len;//total data size in bytes
    size_t index;//progress through sending
    size_t slice;//how much at a time in bytes
} encoder_msg_t;

void outbound_encoder_init(encoder_msg_t * enc, uint8_t* msg, size_t len, size_t slice_len);
void inbound_encoder_init(encoder_msg_t * enc, uint16_t size, size_t slice_len);
void encoder_close(encoder_msg_t * enc);

//for sending data
//0 on success non zero on unused bytes
int encoder_get_next(encoder_msg_t* enc, uint8_t* slice);
//for recieving data
void encoder_add_next(encoder_msg_t* enc, uint8_t* slice);
//is the CRC32 valid
int encoder_verify(encoder_msg_t* enc);
//is there more?
int encoder_finished(encoder_msg_t* enc);

