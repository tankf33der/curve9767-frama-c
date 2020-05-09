#include <stdlib.h>
#include <stdio.h>
#include "curve9767.h"

int main(void) {
    curve9767_point Q;
    shake_context sc;
    uint8_t mike[4] = "mike";

    shake_init(&sc, 256);
    shake_inject(&sc, mike, 4);
    shake_flip(&sc);
    curve9767_hash_to_curve(&Q, &sc);

    return 0;
}
