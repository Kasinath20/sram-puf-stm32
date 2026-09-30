#ifndef PUF_CONFIG_H
#define PUF_CONFIG_H

/* ---- Build mode -------------------------------------------------------- */
#define PUF_MODE_CAPTURE   0   /* dump raw SRAM over UART (data collection) */
#define PUF_MODE_KEY       1   /* reconstruct key with the fuzzy extractor  */
#ifndef PUF_MODE
#define PUF_MODE           PUF_MODE_CAPTURE
#endif

/* ---- SRAM regions (VERIFY against RM0481 memory map) ------------------- */
#define PUF_REGIONS        2
#define PUF_ADDR_0         0x20020000u   /* SRAM2 */
#define PUF_ADDR_1         0x20034000u   /* SRAM3 */
#define PUF_KEY_REGION     0             /* index of region used in KEY mode */
#define PUF_BYTES          4096u
#define PUF_BITS           (PUF_BYTES * 8u)

/* ---- Fuzzy extractor parameters (must match host/enroll.py) ------------ */
#define PUF_GROUP_N        15u                      /* repetition length   */
#define PUF_KEY_BITS       256u                     /* secret bits         */
#define PUF_SEL_BITS       (PUF_KEY_BITS * PUF_GROUP_N)  /* 3840 cells used */
#define PUF_HELPER_BYTES   (PUF_SEL_BITS / 8u)      /* 480                 */
#define PUF_KEY_BYTES      32u

#endif
