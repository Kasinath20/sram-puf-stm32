#ifndef PUF_APP_H
#define PUF_APP_H
#include "main.h"

/* Call FIRST in main(), before HAL_Init(): freezes the power-up SRAM state. */
void puf_snapshot_early(void);
/* Call after peripheral init. Never returns. */
void puf_app_run(UART_HandleTypeDef *vcp);
#endif
