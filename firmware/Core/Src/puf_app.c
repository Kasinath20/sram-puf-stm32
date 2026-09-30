#include "puf_app.h"
#include "puf_config.h"
#include "puf_fe.h"
#include <stdio.h>
#include <string.h>

static const uint32_t puf_addr[PUF_REGIONS] = { PUF_ADDR_0, PUF_ADDR_1 };
static uint8_t  snap[PUF_REGIONS][PUF_BYTES];   /* lives in SRAM1 (.bss) */
static uint32_t boot_rsr;

typedef struct __attribute__((packed)) {
    char     magic[4];   /* "PUF1" */
    uint32_t rsr;        /* RCC->RSR reset flags at boot */
    uint32_t addr;
    uint32_t len;
} puf_hdr_t;

void puf_snapshot_early(void)
{
    boot_rsr = RCC->RSR;               /* which reset caused this boot?      */
    RCC->RSR |= RCC_RSR_RMVF;          /* clear the flags for the next boot  */

#ifdef __HAL_RCC_SRAM2_CLK_ENABLE
    __HAL_RCC_SRAM2_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_SRAM3_CLK_ENABLE
    __HAL_RCC_SRAM3_CLK_ENABLE();
#endif

    for (int r = 0; r < PUF_REGIONS; r++) {
        volatile const uint32_t *src = (volatile const uint32_t *)puf_addr[r];
        uint32_t *dst = (uint32_t *)snap[r];
        for (uint32_t i = 0; i < PUF_BYTES / 4u; i++) dst[i] = src[i];
    }
}

static uint32_t crc32_sw(const uint8_t *d, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (n--) {
        crc ^= *d++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

static void tx(UART_HandleTypeDef *u, const void *p, uint16_t n)
{
    HAL_UART_Transmit(u, (uint8_t *)p, n, HAL_MAX_DELAY);
}

static void heartbeat(void)
{
#ifdef LD2_Pin
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
#endif
}

#if PUF_MODE == PUF_MODE_CAPTURE
static void run_capture(UART_HandleTypeDef *u)
{
    uint8_t c = 0;
    while (c != 'G') HAL_UART_Receive(u, &c, 1, HAL_MAX_DELAY);  /* host trigger */

    for (int r = 0; r < PUF_REGIONS; r++) {
        puf_hdr_t h = { {'P','U','F','1'}, boot_rsr, puf_addr[r], PUF_BYTES };
        uint32_t crc = crc32_sw(snap[r], PUF_BYTES);
        tx(u, &h, sizeof h);
        tx(u, snap[r], PUF_BYTES);
        tx(u, &crc, 4);
    }
    for (;;) { heartbeat(); HAL_Delay(500); }
}
#else
static void run_key(UART_HandleTypeDef *u)
{
    uint8_t key[PUF_KEY_BYTES], fp[4];
    puf_stats_t st = {0, 0};
    char line[112];

    puf_status_t rc = puf_reconstruct(snap[PUF_KEY_REGION], key, &st);
    if (rc == PUF_OK) {
        puf_fingerprint(key, fp);
        snprintf(line, sizeof line,
                 "KEY_OK fp=%02x%02x%02x%02x corrected=%lu worst_group=%lu rsr=0x%08lx\r\n",
                 fp[0], fp[1], fp[2], fp[3],
                 (unsigned long)st.bit_errors, (unsigned long)st.max_group_errors,
                 (unsigned long)boot_rsr);
    } else {
        snprintf(line, sizeof line,
                 "KEY_FAIL code=%d corrected=%lu worst_group=%lu rsr=0x%08lx\r\n",
                 (int)rc, (unsigned long)st.bit_errors,
                 (unsigned long)st.max_group_errors, (unsigned long)boot_rsr);
    }
    /* ---- application would use `key` here (AES/HMAC/...) ---- */
    puf_secure_zero(key, sizeof key);
    puf_secure_zero(snap, sizeof snap);       /* wipe the raw fingerprint */

    for (;;) {                                /* repeat so any terminal sees it */
        tx(u, line, (uint16_t)strlen(line));
        if (rc == PUF_OK) heartbeat();
        HAL_Delay(1000);
    }
}
#endif

void puf_app_run(UART_HandleTypeDef *vcp)
{
#if PUF_MODE == PUF_MODE_CAPTURE
    run_capture(vcp);
#else
    run_key(vcp);
#endif
}
