#include <stdlib.h>
#include <stdio.h>
#include "curve9767.h"

int main(void) {
    curve9767_point Q1;
    shake_context sc1;
    uint8_t mike[4] = "mike";

    // hash_to_curve - 1
    shake_init(&sc1, 256);
    shake_inject(&sc1, mike, 4);
    shake_flip(&sc1);
    curve9767_hash_to_curve(&Q1, &sc1);

    //keygen - 2
    uint8_t seed2[32];
    uint8_t tmp2 [64];
    curve9767_point Q2;
    curve9767_scalar s2;
    for (size_t i = 0; i < 32; i++) {
        seed2[i] = i;
    }
    curve9767_keygen(&s2, tmp2, &Q2, seed2, sizeof seed2);

    // ecdh_keygen - 3
    uint8_t seed3[32];
    uint8_t tmp3 [32];
    curve9767_scalar s3;
    for (size_t i = 0; i < 32; i++) {
        seed3[i] = i;
    }
    curve9767_ecdh_keygen(&s3, tmp3, seed3, sizeof seed3);

    //ecdh_recv - 4
    //XXX, scalar s3 above
    uint8_t bk4 [32];
    uint8_t bQ4 [32];
    uint8_t tmp4[32];
    for (size_t i = 0; i < 32; i++) {
        bk4[i] = bQ4[i] =  i;
    }
    curve9767_ecdh_recv(tmp4, sizeof bk4, &s3, bQ4);

    //sign_generate - 5
    //verify and vartime
    sha3_context sc5;
    curve9767_point Q5;
    curve9767_scalar s5;
    uint8_t seed5[32];
    uint8_t hv5  [32];
    uint8_t sig5 [64];
    uint8_t tmp5 [64];
    uint8_t t5   [32];
    int r = 0;

    for (size_t i = 0; i < 64; i++) {
        sig5[i] = i;
    }
    for (size_t i = 0; i < 32; i++) {
        seed5[i] = i;
        t5   [i] = i;
    }

	curve9767_keygen(&s5, tmp5, &Q5, seed5, sizeof seed5);

    sha3_init(&sc5, 256);
    sha3_update(&sc5, mike, 4);
    sha3_close(&sc5, hv5);

	curve9767_sign_generate(sig5, &s5, t5, &Q5,
		CURVE9767_OID_SHA3_256, hv5, sizeof hv5);
	r |= curve9767_sign_verify(sig5, &Q5,
		CURVE9767_OID_SHA3_256, hv5, sizeof hv5);
    r |= curve9767_sign_verify_vartime(sig5, &Q5,
		CURVE9767_OID_SHA3_256, hv5, sizeof hv5);
    // r will be 1;

    return 0;
}
