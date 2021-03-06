#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoder.h"
#include "test.h"


/*
 * function:
 *    run_encoders_tests
 *
 * return:
 *    void
 *
 * parameters:
 *    void
 *
 * notes:
 *    this is a set of unit tests only used in debug mode
 *    it verifies that all facets of the encoding are fully functional
 *
 * */

#ifndef NDEBUG
void run_encoders_tests() {
    const char* msg = "testing message";
    size_t len      = strlen(msg);
    encoder_frame_t enc_in, enc_out;
    puts("\nbasic test\n");
    {
        outbound_encoder_init(&enc_out, (const uint8_t*)msg, len, 2);
        inbound_encoder_init(&enc_in, enc_out.len, 2);

        uint8_t slice[2];
        while (!encoder_finished(&enc_out)) {
            encoder_get_next(&enc_out, slice);
            encoder_add_next(&enc_in, slice);
        }

        puts("==sending>");
        encoder_print(&enc_out);
        puts("==recieving>");
        encoder_print(&enc_in);
        TEST("crc32 check", encoder_verify(&enc_in));

        encoder_close(&enc_in);
        encoder_close(&enc_out);
    }
    puts("\ndifferent field sizes\n");
    {
        for (int i = 1; i <= 10; ++i) {
            printf("testing slice of %d bytes\n", i);
            outbound_encoder_init(&enc_out, (const uint8_t*)msg, len, i);
            inbound_encoder_init(&enc_in, enc_out.len, i);

            uint8_t* slice = malloc(i);
            while (!encoder_finished(&enc_out)) {
                encoder_get_next(&enc_out, slice);
                encoder_add_next(&enc_in, slice);
            }
            free(slice);

            TEST("crc32 check", encoder_verify(&enc_in));

            encoder_close(&enc_in);
            encoder_close(&enc_out);
        }
    }
    puts("\ntesting unknown inboud sizing\n");
    {
        outbound_encoder_init(&enc_out, (const uint8_t*)msg, len, 2);
        inbound_encoder_init(&enc_in, 0, 2);

        uint8_t slice[2];
        while (!encoder_finished(&enc_out)) {
            encoder_get_next(&enc_out, slice);
            encoder_add_next(&enc_in, slice);
        }

        TEST("crc32 check", encoder_verify(&enc_in));
        TEST("outbound finished", encoder_finished(&enc_out));
        TEST("inbound finished", encoder_finished(&enc_in));

        encoder_close(&enc_in);
        encoder_close(&enc_out);
    }
    puts("\ntr encoding test\n");
    {
        uint8_t test[6];
        test[0] = 'h';
        test[1] = 'i';
        test[2] = 0x0;
        test[3] = '!';
        test[4] = 0xFF;
        test[5] = 0xFF;

        puts("raw data");
        for (size_t i = 0; i < sizeof(test); ++i) {
            printf("%02X ", test[i]);
        }
        puts("");

        size_t s;

        TEST("tr encoding sizing correct", (s = tr_size(test, sizeof(test))) == 9);

        uint8_t* test2 = malloc(s);

        tr_encode(test2, test, sizeof(test));

        puts("encoded data");
        for (size_t i = 0; i < s; ++i) {
            printf("%02X ", test2[i]);
        }
        puts("");

        tr_decode(test, test2, s);

        puts("decoded data");

        for (size_t i = 0; i < sizeof(test); ++i) {
            printf("%02X ", test[i]);
        }
        puts("");

        TEST("tr maintained 0, 0xFF, and regular data",
            test[0] == 'h' && test[2] == 0 && test[4] == 0xFF);
    }
}
#endif
