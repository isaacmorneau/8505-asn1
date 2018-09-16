#pragma once

#include <stdint.h>
#include <sys/types.h>

typedef struct {
    uint16_t size; //total size of this message (includes data crc and this number) in bytes
    //layout of buffer
    //[size][data][crc32]
    uint8_t* buffer; //what we are sending
    uint32_t crc32; //ensure there was no corruption
    size_t len; //total data size in bytes
    size_t index; //progress through sending
    size_t slice; //how much at a time in bytes
} encoder_msg_t;

void outbound_encoder_init(
    encoder_msg_t* enc, const uint8_t* msg, const size_t len, const size_t slice_len);
void inbound_encoder_init(encoder_msg_t* enc, const uint16_t size, const size_t slice_len);
void encoder_close(encoder_msg_t* enc);

//for sending data
//0 on success non zero on unused bytes
int encoder_get_next(encoder_msg_t* enc, uint8_t* slice);
//for recieving data
void encoder_add_next(encoder_msg_t* enc, const uint8_t* slice);
//is the CRC32 valid
int encoder_verify(encoder_msg_t* enc);
//check if there is more to send
int encoder_finished(encoder_msg_t* enc);
//print all info on the struct, useful for debugging
void encoder_print(encoder_msg_t* enc);
