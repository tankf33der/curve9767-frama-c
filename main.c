#include <stdlib.h>
#include <stdio.h>
#include "curve9767.h"

int main(void) {
    curve9767_point Q1;
    shake_context sc1;
    uint8_t mike[4] = "mike";

    // hash_to_curve
    shake_init(&sc1, 256);
    shake_inject(&sc1, mike, 4);
    shake_flip(&sc1);
    curve9767_hash_to_curve(&Q1, &sc1);

    //keygen
    uint8_t seed2[32];
    uint8_t tmp2[64];
    curve9767_point Q2;
    curve9767_scalar s2;
    for (size_t i = 0; i < 32; i++) {
        seed2[i] = i;
    }
    curve9767_keygen(&s2, tmp2, &Q2, seed2, sizeof seed2);

    // ecdh_keygen
    uint8_t seed3[32];
    uint8_t tmp3[32];
    curve9767_scalar s3;
    for (size_t i = 0; i < 32; i++) {
        seed3[i] = i;
    }
    curve9767_ecdh_keygen(&s3, tmp3, seed3, sizeof seed3);

    //ecdh_recv
    //XXX, scalar s3 above
    uint8_t bk4[32];
    uint8_t bQ4[32];
    uint8_t tmp4[32];
    for (size_t i = 0; i < 32; i++) {
        bk4[i] = bQ4[i] =  i;
    }
    curve9767_ecdh_recv(tmp4, sizeof bk4, &s3, bQ4);


    return 0;
}
