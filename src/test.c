#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encoder.h"
#include "test.h"

void run_encoders_tests() {
    const char* msg = "testing message";
    size_t len      = strlen(msg);
    encoder_msg_t enc_in, enc_out;
    puts("\nbasic test\n");
    {
        outbound_encoder_init(&enc_in, (const uint8_t*)msg, len, 2);
        inbound_encoder_init(&enc_out, enc_in.size, 2);

        uint8_t slice[2];
        while (!encoder_finished(&enc_in)) {
            encoder_get_next(&enc_in, slice);
            encoder_add_next(&enc_out, slice);
        }

        puts("==sending>");
        encoder_print(&enc_in);
        puts("==recieving>");
        encoder_print(&enc_out);
        if (encoder_verify(&enc_out)) {
            puts("[verified]");
        } else {
            puts("[not verified]");
        }

        encoder_close(&enc_in);
        encoder_close(&enc_out);
    }
    puts("\ndifferent field sizes\n");
    {
        for (int i = 1; i <= 10; ++i) {
            printf("testing slice of %d bytes: ", i);
            outbound_encoder_init(&enc_in, (const uint8_t*)msg, len, i);
            inbound_encoder_init(&enc_out, enc_in.size, i);

            uint8_t* slice = malloc(i);
            while (!encoder_finished(&enc_in)) {
                encoder_get_next(&enc_in, slice);
                encoder_add_next(&enc_out, slice);
            }
            free(slice);

            if (encoder_verify(&enc_out)) {
                puts("[verified]");
            } else {
                puts("[not verified]");
            }

            encoder_close(&enc_in);
            encoder_close(&enc_out);
        }
    }
}
