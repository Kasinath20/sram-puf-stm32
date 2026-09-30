#ifndef SHA256_H
#define SHA256_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t h[8]; uint8_t buf[64]; uint64_t len; uint32_t fill; } sha256_ctx;
void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const uint8_t *d, size_t n);
void sha256_final(sha256_ctx *c, uint8_t out[32]);
#endif
