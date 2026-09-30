# SRAM PUF Key Generator on STM32H533 (NUCLEO-H533RE)
**Documentation:** see [OPERATION_MANUAL.md](OPERATION_MANUAL.md) for setup, operation procedure and evaluation steps.
A Physical Unclonable Function built from the power-up state of on-chip SRAM,
with a data-driven cell-selection step and a fuzzy extractor that turns the noisy
fingerprint into an exact 256-bit key. No key is stored in flash: only public
helper data is.

## How it works
1. **Capture** - firmware snapshots SRAM2/SRAM3 before HAL init and streams the raw
   bytes over the ST-LINK virtual COM port. Firmware links only into SRAM1, so the
   PUF banks keep their power-up state (the startup code only clears `.bss`).
2. **Characterise** - `analyze.py` reports uniformity, stability and Hamming distance.
3. **Learn stable cells** - `enroll.py` estimates per-cell flip probability from N
   power cycles and selects the 3840 most reliable cells. Validation uses held-out boots.
4. **Fuzzy extractor** (code-offset, repetition code n=15, 256 secret bits):
   `helper = response XOR repeat(secret)`. At boot: `w = response XOR helper`,
   majority-vote each group of 15, `key = SHA256(tag || secret)`, verify with a stored hash.
5. **Boot** - `PUF_MODE_KEY` firmware reconstructs the key and prints a short
   fingerprint, corrected-bit count and worst-group margin (fails at >= 8 errors/group).

## Layout
```
firmware/Core/Inc,Src   puf_app, puf_fe (fuzzy extractor), sha256, puf_config, helper_data (generated)
firmware/linker_memory_snippet.ld
host/                   capture.py analyze.py enroll.py sim_puf.py requirements.txt
tests/                  test_host.c run_tests.sh   (runs the real firmware crypto on a PC)
```

## Build & run
1. Create the project (CubeMX board selector: NUCLEO-H533RE, TrustZone off), import into CubeIDE.
2. Copy `firmware/Core/Inc/*` and `firmware/Core/Src/*` into the project's `Core/Inc`, `Core/Src`.
3. Replace the `MEMORY` block in `STM32H533RETX_FLASH.ld` with `linker_memory_snippet.ld`.
4. Edit `Core/Src/main.c` (inside USER CODE blocks; replace `huart3` with your VCP handle):
```c
/* USER CODE BEGIN Includes */
#include "puf_app.h"
/* USER CODE END Includes */
...
int main(void) {
  /* USER CODE BEGIN 1 */
  puf_snapshot_early();          // BEFORE HAL_Init()
  /* USER CODE END 1 */
  ...
  /* USER CODE BEGIN 2 */
  puf_app_run(&huart3);          // never returns
  /* USER CODE END 2 */
```
5. **Collect data:** `PUF_MODE=PUF_MODE_CAPTURE` (default). Flash with *Run*, then
   `python host/capture.py --n 100` (true USB unplug/replug between captures).
   Vary temperature/USB port for a few dozen captures.
6. **Analyse:** `python host/analyze.py 20020000`
7. **Enroll:** `python host/enroll.py --dir captures --addr 20020000 --out firmware/Core/Inc/helper_data.h`
   Note the printed KEY FINGERPRINT.
8. **Key mode:** add `-DPUF_MODE=1` (Project Properties -> C/C++ Build -> Settings -> MCU GCC Compiler ->
   Preprocessor) or change the default in `puf_config.h`. Rebuild, flash, power-cycle:
   the serial terminal (115200) should print `KEY_OK fp=<same fingerprint>`.
   If you enrolled bank SRAM3, set `PUF_KEY_REGION 1`.

## What has been verified
Verified on a PC with a simulated PUF (`bash tests/run_tests.sh`): SHA-256 matches
Python's hashlib; enrolled key is reconstructed in 200/200 fresh boots; a different
simulated chip is rejected 20/20; still 96-100% at 2-4x the enrollment noise.
The C code used is the same file the firmware builds.

## NOT verified (needs your board)
- SRAM bank addresses/sizes, ECC and erase-on-reset behaviour of SRAM2/SRAM3 (check RM0481; if `analyze.py` shows all-0/all-1, use the other bank)
- Which USART is the VCP; whether the CMSIS macros `RCC_RSR_RMVF`, `LD2_Pin` exist under those names
- Real noise statistics: the simulator is a model, not silicon. Re-run `enroll.py` on real dumps; if validation shows failures, raise `PUF_GROUP_N` (in both `puf_config.h` and `enroll.py`) or collect more captures.

## Limitations (state these in the write-up)
- One board only: inter-chip uniqueness cannot be measured.
- Repetition code leaks entropy via helper data; 256 secret bits from cells with <1 bit of entropy each leave less than 256 bits of true entropy. Fine for a prototype; a production design would use a proper entropy budget and a stronger code.
- Helper data is not authenticated (helper-data manipulation attacks are out of scope).
- SRAM PUFs are affected by aging and by extreme temperature/voltage; re-enrollment may be needed.
- Debugger resets, the reset button and `NVIC_SystemReset` preserve SRAM: only a real power-cycle gives a power-up state.
- "ML" component: cell selection is a statistical estimator validated on held-out data. It is not a neural network, deliberately: the key must be bit-exact and a model of the fingerprint would store it in its weights.
