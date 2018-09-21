#pragma once

#ifndef NDEBUG

void run_encoders_tests();

#define TEST(msg, expr)\
    do {\
    if (!(expr)) {\
        printf("\033[31m[FAILED]\033[m %s\n    %s\n    %s\n    %s::%d\n", msg, #expr, __FILE__, __FUNCTION__, __LINE__);\
    } else {\
        printf("\033[32m[PASSED]\033[m %s\n", msg);\
    }\
} while(0)

#endif
