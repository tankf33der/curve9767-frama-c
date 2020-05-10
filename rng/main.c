#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "../sha3.h"

int main(void)
{
    uint32_t buf[2048];
    uint32_t   b[4];
    shake_context ctx;

    shake_init(&ctx, 256);
    shake_flip(&ctx);
    while (1) {
        for(size_t i = 0; i < 2048; i++) {
            shake_extract(&ctx, b, sizeof b);
            buf[i] = (uint32_t)b[0]
    			| ((uint32_t)b[1] << 8)
    			| ((uint32_t)b[2] << 16)
    			| ((uint32_t)b[3] << 24);
        }
        if (fwrite(buf, 4, 2048, stdout) != 2048) break;
    }
    return 0;
}
