#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# AMB-1 host tests: the maths, the platform pieces kept from Felucca / X0X, the instrument, and the
# whole firmware app in the simulator (tests/scenarios/*.amb: UI, chords, rhythm, persistence), with
# screenshots and audio in build/scenarios/.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc} -D_GNU_SOURCE"          # (Linux hides clock_gettime and friends under -std=c99)
FAIL=0
OUT=build/host
mkdir -p "$OUT" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"
run master sh -c "$CC -O2 -Wall -Wextra -Werror -Ifirmware/src/app -o $OUT/master_test tests/host/master_test.c && $OUT/master_test && $CC -O2 -Wall -Wextra -Werror -DAISLING_QUIET_TEST=1 -Ifirmware/src/app -o $OUT/master_quiet_test tests/host/master_test.c && $OUT/master_quiet_test"
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
run uart-dma sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uart_dma_test tests/host/uart_dma_test.c && $OUT/uart_dma_test"
# the same test against the old poll must fail, or it proves nothing
run uart-dma-control sh -c "$CC -O2 -w -DLEGACY_CONTROL -Ifirmware/src -Ifirmware/hal -o $OUT/uart_dma_legacy tests/host/uart_dma_test.c && ! $OUT/uart_dma_legacy"
run usb-desc sh -c "$CC -O2 -w -DT_CDC=0 -Ifirmware/src -Ifirmware/hal -o $OUT/usb_desc_test tests/host/usb_desc_test.c -lm && $OUT/usb_desc_test"
run usb-desc-cdc sh -c "$CC -O2 -w -DT_CDC=1 -Ifirmware/src -Ifirmware/hal -o $OUT/usb_desc_cdc tests/host/usb_desc_test.c -lm && $OUT/usb_desc_cdc"
run amb sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Ifirmware/src/dsp \
    -o $OUT/amb_test tests/host/amb_test.c firmware/src/dsp/amb.c -lm && $OUT/amb_test"
run usbtap sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Ifirmware/src/dsp \
    -o $OUT/usbtap_test tests/host/usbtap_test.c firmware/src/dsp/amb.c -lm && $OUT/usbtap_test"
run golden sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Ifirmware/src/dsp \
    -o $OUT/golden_test tests/host/golden_test.c firmware/src/dsp/amb.c -lm && $OUT/golden_test"
run seq sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -Ifirmware/src/dsp -o $OUT/seq_test tests/host/seq_test.c firmware/src/dsp/seq.c && $OUT/seq_test"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
# the SMK-37 Elite's flash map; the control builds it with the FM-1's map, which must fail
run smk-flash sh -c "$CC -O2 -Wall -Wextra -Wno-unused-function -Werror -DAISLING_BOARD_SMK37=1 -o $OUT/smk_flash_test tests/smk_flash_test.c && $OUT/smk_flash_test && \
    $CC -O2 -w -o $OUT/smk_flash_fm1 tests/smk_flash_test.c && ! $OUT/smk_flash_fm1 >/dev/null"
# its keys, buttons and knobs on a simulated matrix; the control swaps OCT-'s and OCT+'s codes, which must fail
SMK_IN="-O2 -Wall -Wextra -Wno-unused-function -Werror -DAISLING_BOARD_SMK37=1 -Ifirmware/hal -Ifirmware/src/app"
run smk-input sh -c "$CC $SMK_IN -o $OUT/smk_input_test tests/host/smk_input_test.c && $OUT/smk_input_test && \
    $CC $SMK_IN -DCONTROL -o $OUT/smk_input_control tests/host/smk_input_test.c && ! $OUT/smk_input_control >/dev/null"
run rescue python3 tests/rescue_test.py
run package python3 tests/pkg_test.py
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/amb.fwsc ]; then
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/amb.fwsc"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/amb.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/amb.fwsc"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/amb.fwsc: run ./build.sh)"
fi
run host-build sh host/build_host.sh
if command -v node >/dev/null 2>&1; then
    run visualiser node tests/host/vis_test.mjs web/visualiser/index.html
fi
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh web/emu/build.sh >/dev/null && node tests/host/emu_test.mjs build/emu/amb.wasm"
    run app sh -c "sh web/app/build.sh >/dev/null && node tests/host/app_test.mjs build/app/amb.wasm"
fi
for s in tests/scenarios/*.amb; do
    n=$(basename "$s" .amb)
    mkdir -p "build/scenarios/$n"
    run "scenario-$n" build/host/amb_host "$s" "build/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
