#!/bin/bash
# End-to-end test on simulated data. Run from the project root: bash tests/run_tests.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
W=tests/work; rm -rf $W; mkdir -p $W/gen

echo "== 1. SHA-256 vs Python hashlib"
gcc -O2 -Wall -Ifirmware/Core/Inc -o $W/t_sha tests/test_host.c firmware/Core/Src/sha256.c firmware/Core/Src/puf_fe.c 2>/dev/null || true
# (puf_fe.c needs a helper_data.h; use the stub for this quick build)
for s in "" "abc" "$(head -c 200 /dev/zero | tr '\0' 'x')"; do
  a=$($W/t_sha sha "$s"); b=$(printf "%s" "$s" | python3 -c "import sys,hashlib;print(hashlib.sha256(sys.stdin.buffer.read()).hexdigest())")
  [ "$a" = "$b" ] && echo "  ok (${#s} bytes)" || { echo "  SHA MISMATCH"; exit 1; }
done

echo "== 2. Simulate chip #1 (60 boots), enroll, validate on held-out boots"
python3 host/sim_puf.py --out $W/chip1 --n 60 --chip-seed 1 >/dev/null
python3 host/enroll.py --dir $W/chip1 --out $W/gen/helper_data.h --seed 42 | tee $W/enroll.log
FP=$(grep "KEY FINGERPRINT" $W/enroll.log | awk '{print $NF}')

echo "== 3. Build firmware crypto with enrolled helper data"
gcc -O2 -Wall -Wextra -I$W/gen -Ifirmware/Core/Inc -o $W/t_puf \
    tests/test_host.c firmware/Core/Src/sha256.c firmware/Core/Src/puf_fe.c

echo "== 4. 200 fresh boots of the SAME chip (new noise) -> must all give $FP"
python3 host/sim_puf.py --out $W/fresh --n 200 --chip-seed 1 >/dev/null
pass=0; maxerr=0
for f in $W/fresh/*.bin; do
  out=$($W/t_puf $f) && rc=0 || rc=1
  if [ $rc = 0 ] && echo "$out" | grep -q "fp=$FP"; then pass=$((pass+1)); fi
done
echo "  reconstruction success: $pass/200"

echo "== 5. A DIFFERENT chip must NOT reproduce the key"
python3 host/sim_puf.py --out $W/chip2 --n 20 --chip-seed 2 >/dev/null
bad=0
for f in $W/chip2/*.bin; do $W/t_puf $f >/dev/null && bad=$((bad+1)) || true; done
echo "  false accepts: $bad/20"

[ $pass = 200 ] && [ $bad = 0 ] && echo "ALL TESTS PASSED" || { echo "TESTS FAILED"; exit 1; }
