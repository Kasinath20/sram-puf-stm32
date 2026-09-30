/*
 * Fuzzy extractor (code-offset construction, repetition code).
 *
 * Enrollment (host/enroll.py):
 *   - choose PUF_SEL_BITS most stable SRAM cells  -> PUF_SEL_MASK (public)
 *   - draw random secret s (256 bits), codeword c = each bit repeated N times
 *   - helper = response XOR c                       -> PUF_HELPER  (public)
 *   - key = SHA256("PUF-KEY-v1"||s); check = SHA256("PUF-CHK-v1"||key)
 * Reconstruction (here):
 *   - w = response XOR helper (= c + noise), majority-vote each group -> s
 *   - key = SHA256(...); verify against PUF_KEYCHECK.
 */
#include "puf_fe.h"
#include "sha256.h"
#include "helper_data.h"
#include <string.h>

void puf_secure_zero(void *p, uint32_t n)
{
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (n--) *v++ = 0;
}

static void hash2(const char *tag, const uint8_t *d, size_t n, uint8_t out[32])
{
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, (const uint8_t *)tag, strlen(tag));
    sha256_update(&c, d, n);
    sha256_final(&c, out);
}

puf_status_t puf_reconstruct(const uint8_t *sram, uint8_t key[PUF_KEY_BYTES],
                             puf_stats_t *st)
{
    if (!PUF_ENROLLED) return PUF_ERR_NOT_ENROLLED;

    uint8_t  ones[PUF_KEY_BITS];
    uint8_t  s[PUF_KEY_BITS / 8];
    uint8_t  chk[32];
    memset(ones, 0, sizeof ones);
    memset(s, 0, sizeof s);

    uint32_t j = 0;
    for (uint32_t i = 0; i < PUF_BITS; i++) {
        if (!((PUF_SEL_MASK[i >> 3] >> (i & 7)) & 1u)) continue;
        if (j >= PUF_SEL_BITS) return PUF_ERR_BAD_MASK;
        uint8_t x = (sram[i >> 3] >> (i & 7)) & 1u;
        uint8_t h = (PUF_HELPER[j >> 3] >> (j & 7)) & 1u;
        ones[j / PUF_GROUP_N] += (uint8_t)(x ^ h);
        j++;
    }
    if (j != PUF_SEL_BITS) return PUF_ERR_BAD_MASK;

    uint32_t errs = 0, maxg = 0;
    for (uint32_t g = 0; g < PUF_KEY_BITS; g++) {
        uint8_t bit = ones[g] > (PUF_GROUP_N / 2);
        uint32_t e = bit ? (PUF_GROUP_N - ones[g]) : ones[g];
        errs += e;
        if (e > maxg) maxg = e;
        s[g >> 3] |= (uint8_t)(bit << (g & 7));
    }
    if (st) { st->bit_errors = errs; st->max_group_errors = maxg; }

    hash2("PUF-KEY-v1", s, sizeof s, key);
    hash2("PUF-CHK-v1", key, PUF_KEY_BYTES, chk);
    puf_secure_zero(s, sizeof s);

    uint8_t diff = 0;                       /* constant-time compare */
    for (int i = 0; i < 32; i++) diff |= chk[i] ^ PUF_KEYCHECK[i];
    if (diff) { puf_secure_zero(key, PUF_KEY_BYTES); return PUF_ERR_KEYCHECK; }
    return PUF_OK;
}

void puf_fingerprint(const uint8_t key[PUF_KEY_BYTES], uint8_t fp[4])
{
    uint8_t h[32];
    hash2("PUF-FP", key, PUF_KEY_BYTES, h);
    memcpy(fp, h, 4);
}
