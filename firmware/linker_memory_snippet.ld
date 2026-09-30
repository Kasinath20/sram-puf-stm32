/* Replace the MEMORY block in STM32H533RETX_FLASH.ld.
   Firmware uses SRAM1 only, so SRAM2/SRAM3 keep their power-up state
   untouched. Verify addresses/sizes against RM0481 (Memory map). */
MEMORY
{
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}
