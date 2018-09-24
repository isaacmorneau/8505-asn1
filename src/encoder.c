#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc32.h"
#include "encoder.h"

/*
 * function:
 *    outbound_encoder_init
 *
 * return:
 *    void
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to initialize
 *    const uint8_t* restrict msg the message to add to the frame
 *    const size_t len the length of the message
 *    const size_t slice_len the amount to transmit per message
 *
 * notes:
 *    gets data calculated, wrapped, and ready to send
 *
 * */

void outbound_encoder_init(encoder_frame_t* restrict enc, const uint8_t* restrict msg,
    const size_t len, const size_t slice_len) {
    enc->size = sizeof(uint16_t) + len + sizeof(uint32_t);

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

    size_t trs = tr_size(enc->buffer, enc->size);
    //zeros detected, remove zeros with encoding
    if (trs != enc->size) {
        //create new buffer to move to

        uint8_t* trtmp = malloc(trs);
        tr_encode(trtmp, enc->buffer, enc->size);

        //finish swap to the new buffer
        free(enc->buffer);
        enc->buffer = trtmp;
        enc->size   = trs;
    }

    enc->partial_byte_eh = false;
}

/*
 * function:
 *    inbound_encoder_init
 *
 * return:
 *    void
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to initialize
 *    const size_t len the length of the message
 *    const size_t slice_len the amount to transmit per message
 *
 * notes:
 *    gets buffers ready to recieve data
 *
 * */

void inbound_encoder_init(
    encoder_frame_t* restrict enc, const uint16_t len, const size_t slice_len) {
    if (len) { //we know the frame size
        enc->len  = len;
        enc->size = sizeof(uint16_t) + len + sizeof(uint32_t);

        enc->buffer     = malloc(enc->size);
        uint16_t* sbuff = (uint16_t*)enc->buffer;
        *sbuff          = enc->size;
        //dont bother to initialize crc
    } else { //framesize is unknown
        enc->len    = 0;
        enc->size   = 0;
        enc->buffer = malloc(sizeof(uint16_t));
    }
    //to keep the interfact the same start at 0 though the first uint16_t size is already known
    enc->index = 0;
    enc->slice = slice_len;

    enc->partial_byte_eh = false;
}


/*
 * function:
 *    encoder_close
 *
 * return:
 *    void
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to clean up
 *
 * notes:
 *    releases resources from frame
 *
 * */

void encoder_close(encoder_frame_t* restrict enc) {
    memset(enc->buffer, 0, enc->size);
    free(enc->buffer);
    enc->buffer = 0;
}


/*
 * function:
 *    encoder_get_next
 *
 * return:
 *    int
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to get from
 *    uint8_t* restrict slice the buffer to fill
 *
 * notes:
 *    gets the next slice from an outbound frame
 *
 * */

int encoder_get_next(encoder_frame_t* restrict enc, uint8_t* restrict slice) {
    size_t i = 0;

    //no need to worry about transcoding on the fly as the complete message is already done in init
    for (; i < enc->slice; ++i) {
        if (enc->index < enc->size) {
            slice[i] = enc->buffer[enc->index++];
        } else {
            break;
        }
    }

    return enc->slice - i;
}


/*
 * function:
 *    encoder_add_next
 *
 * return:
 *    void
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to build
 *    const uint8_t* restrict slice the buffer to read
 *
 * notes:
 *    add the next slice to an inboud frame
 *
 * */

void encoder_add_next(encoder_frame_t* restrict enc, const uint8_t* restrict slice) {
    for (size_t i = 0; i < enc->slice; ++i) {
        if (!enc->size || enc->index < enc->size) {
            if (enc->partial_byte_eh) { //last byte escaped this one
                enc->partial_byte_eh = false;
                if (slice[i] == 0xFF) {
                    enc->buffer[enc->index++] = 0xFF;
                } else if (slice[i] == 0xF0) {
                    enc->buffer[enc->index++] = 0;
                }
            } else if (slice[i] == 0xFF) { //escape byte incomming
                if (i + 1 < enc->slice) {
                    if (slice[++i] == 0xFF) {
                        enc->buffer[enc->index++] = 0xFF;
                    } else if (slice[i] == 0xF0) {
                        enc->buffer[enc->index++] = 0;
                    }
                } else { //escape byte is in the next slice
                    enc->partial_byte_eh = true;
                }
            } else { //regular byte
                enc->buffer[enc->index++] = slice[i];
            }
        } else { //no need to report on unused slice parts as the structure is filled
            break;
        }
    }

    if (!enc->size && enc->index >= sizeof(uint16_t)) { //confirmed size attained, realloc to full
        enc->size = *(uint16_t*)enc->buffer;
        enc->len  = enc->size - sizeof(uint16_t) - sizeof(uint32_t);

        enc->buffer = realloc(enc->buffer, enc->size);
    }
}


/*
 * function:
 *    encoder_verify
 *
 * return:
 *    int returns 1 if verified 0 otherwise
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to check
 *
 * notes:
 *    verify the frame sent currectly
 *
 * */

int encoder_verify(encoder_frame_t* restrict enc) {
    return (enc->crc32 = *(uint32_t*)(enc->buffer + enc->size - sizeof(uint32_t)))
        == xcrc32(enc->buffer, enc->size - sizeof(uint32_t), (uint32_t)-1);
}


/*
 * function:
 *    encoder_finished
 *
 * return:
 *    int returns 1 if finished 0 otherwise
 *
 * parameters:
 *    encoder_frame_t* restrict enc the frame to check
 *
 * notes:
 *    check if the frame is finished sending or recieving
 *
 * */

int encoder_finished(encoder_frame_t* restrict enc) {
    if (enc->size && enc->index >= enc->size) {
        enc->crc32 = *(uint32_t*)(enc->buffer + enc->size - sizeof(uint32_t));
        return 1;
    }

    return 0;
}


/*
 * function:
 *    encoder_print
 *
 * return:
 *    void
 *
 * parameters:
 *    const encoder_frame_t* restrict enc the encoder to print
 *
 * notes:
 *    print out debuging information about frames
 *
 * */

void encoder_print(const encoder_frame_t* restrict enc) {
    printf("[message data]\nsize: %u\ndata: %.*s\ncrc32: 0x%X\n", enc->size, enc->len,
        enc->buffer + sizeof(uint16_t), enc->crc32);
    printf("[internal data]\nindex: %u\nslice: %u\nlen: %u\n", enc->index, enc->slice, enc->len);
    puts("[raw hex]");
    for (size_t i = 0; i < enc->size; ++i) {
        printf("%02X ", enc->buffer[i]);
    }
    puts("");
}


/*
 * function:
 *    tr_size
 *
 * return:
 *    size_t returns the encodes size
 *
 * parameters:
 *    uint8_t* restrict buffer the buffer to check
 *    size_t len the length of the buffer
 *
 * notes:
 *    checks how long an encoded array would be
 *
 * */

size_t tr_size(uint8_t* restrict buffer, size_t len) {
    size_t total = len;
    for (size_t i = 0; i < len; ++i) {
        if (!buffer[i] || buffer[i] == 0xFF) {
            ++total;
        }
    }
    return total;
}


/*
 * function:
 *    tr_encode
 *
 * return:
 *    void
 *
 * parameters:
 *    uint8_t* restrict dest the encoded buffer
 *    uint8_t* restrict src the unencoded buffer
 *    size_t len the length of the unencoded buffer
 *
 * notes:
 *    converts regular data into encoded data
 *
 * */

void tr_encode(uint8_t* restrict dest, uint8_t* restrict src, size_t len) {
    for (size_t i = 0, j = 0; i < len; ++i) {
        if (!src[i]) { //0
            dest[j++] = 0xFF;
            dest[j++] = 0xF0; //0
        } else if (src[i] == 0xFF) { //0xFF
            dest[j++] = 0xFF;
            dest[j++] = 0xFF; //0xFF
        } else {
            dest[j++] = src[i];
        }
    }
}


/*
 * function:
 *    tr_decode
 *
 * return:
 *    size_t the new size
 *
 * parameters:
 *    uint8_t* restrict dest the unencoded data
 *    uint8_t* restrict src the encoded data
 *    size_t len the length of the encoded data
 *
 * notes:
 *    converts the encoded data back into regular data
 *
 * */

size_t tr_decode(uint8_t* restrict dest, uint8_t* restrict src, size_t len) {
    size_t ret = len;
    for (size_t i = 0, j = 0; i < len; ++i) {
        if (src[i] == 0xFF) {
            if (++i < len) { //bounds check for good measure
                --ret;
                if (src[i] == 0xF0) {
                    dest[j++] = 0;
                } else if (src[i] == 0xFF) {
                    dest[j++] = 0xFF;
                }
            }
        } else {
            dest[j++] = src[i];
        }
    }
    return ret;
}


/*
 * function:
 *    encoder_data
 *
 * return:
 *    const uint8_t* returns a pointer to the data in a frame
 *
 * parameters:
 *    const encoder_frame_t* restrict enc the frame to check
 *
 * notes:
 *    a helper to extract the real data from the frame
 *
 * */

const uint8_t* encoder_data(const encoder_frame_t* restrict enc) {
    return enc->buffer + sizeof(uint16_t);
}
