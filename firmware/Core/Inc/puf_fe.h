#ifndef PUF_FE_H
#define PUF_FE_H
#include <stdint.h>
#include "puf_config.h"

typedef enum {
    PUF_OK = 0,
    PUF_ERR_NOT_ENROLLED = 1,
    PUF_ERR_BAD_MASK     = 2,
    PUF_ERR_KEYCHECK     = 3   /* too many bit errors: reconstruction failed */
} puf_status_t;

typedef struct {
    uint32_t bit_errors;        /* total corrected bit errors (of PUF_SEL_BITS) */
    uint32_t max_group_errors;  /* worst group; failure if >= (N+1)/2 = 8       */
} puf_stats_t;

/* sram: PUF_BYTES of raw power-up SRAM. key: PUF_KEY_BYTES output. */
puf_status_t puf_reconstruct(const uint8_t *sram, uint8_t key[PUF_KEY_BYTES],
                             puf_stats_t *st);
/* First 4 bytes of SHA-256("PUF-FP"||key): safe to print for comparison. */
void puf_fingerprint(const uint8_t key[PUF_KEY_BYTES], uint8_t fp[4]);
void puf_secure_zero(void *p, uint32_t n);
#endif
