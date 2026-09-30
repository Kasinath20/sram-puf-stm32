/* Host-side harness: runs the SAME firmware code (puf_fe.c, sha256.c) on a PC.
   usage: test_host sha <text> | test_host dump.bin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "puf_fe.h"
#include "sha256.h"

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "sha")) {
        uint8_t o[32]; sha256_ctx c;
        sha256_init(&c); sha256_update(&c, (uint8_t *)argv[2], strlen(argv[2])); sha256_final(&c, o);
        for (int i = 0; i < 32; i++) printf("%02x", o[i]);
        printf("\n");
        return 0;
    }
    if (argc != 2) return 2;
    static uint8_t buf[PUF_BYTES];
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(buf, 1, PUF_BYTES, f) != PUF_BYTES) { fprintf(stderr, "read error\n"); return 2; }
    fclose(f);
    uint8_t key[PUF_KEY_BYTES], fp[4]; puf_stats_t st = {0, 0};
    puf_status_t rc = puf_reconstruct(buf, key, &st);
    if (rc == PUF_OK) {
        puf_fingerprint(key, fp);
        printf("KEY_OK fp=%02x%02x%02x%02x corrected=%u worst=%u\n",
               fp[0], fp[1], fp[2], fp[3], st.bit_errors, st.max_group_errors);
    } else {
        printf("KEY_FAIL code=%d corrected=%u worst=%u\n", rc, st.bit_errors, st.max_group_errors);
    }
    return rc == PUF_OK ? 0 : 1;
}
