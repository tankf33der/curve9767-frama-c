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
    uint8_t seed[32];
    uint8_t tmp[64];
    curve9767_point Q2;
    curve9767_scalar s2;
    for (size_t i = 0; i < 32; i++) {
        seed[i] = i;
    }
    curve9767_keygen(&s2, tmp, &Q2, seed, sizeof seed);

    return 0;
}
