#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    uint16_t size; //total size of this message (includes data crc and this number) in bytes
    //layout of buffer
    //[size][data][crc32]
    uint8_t* buffer; //what we are sending
    uint32_t crc32; //ensure there was no corruption
    size_t len; //total data size in bytes
    bool partial_byte_eh; //true if last slice ended on a partial byte
    size_t index; //progress through sending
    size_t slice; //how much at a time in bytes
} encoder_frame_t;

void outbound_encoder_init(
    encoder_frame_t* enc, const uint8_t* msg, const size_t len, const size_t slice_len);
void inbound_encoder_init(encoder_frame_t* enc, const uint16_t len, const size_t slice_len);
void encoder_close(encoder_frame_t* enc);

//for sending data
//0 on success non zero on unused bytes
int encoder_get_next(encoder_frame_t* enc, uint8_t* slice);
//for recieving data
void encoder_add_next(encoder_frame_t* enc, const uint8_t* slice);
//is the CRC32 valid
int encoder_verify(encoder_frame_t* enc);
//check if there is more to send
int encoder_finished(encoder_frame_t* enc);
//print all info on the struct, useful for debugging
void encoder_print(const encoder_frame_t* enc);

//find size to allocate for the transcoded buffer
size_t tr_size(uint8_t* buffer, size_t len);
//perform transcoding
//len is src length
void tr_encode(uint8_t* dest, uint8_t* src, size_t len);
//perform transcoding
//len is src len, ret is new len
size_t tr_decode(uint8_t* dest, uint8_t* src, size_t len);
