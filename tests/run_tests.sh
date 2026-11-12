#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# AMB-1 host tests: the maths, the platform pieces kept from Felucca / X0X, the instrument, and the
# whole firmware app in the simulator (tests/scenarios/*.amb: UI, chords, rhythm, persistence), with
# screenshots and audio in build/scenarios/; and the same on the SMK-37 Elite's simulator (build/scenarios_smk37/).
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
run usb-desc-smk sh -c "$CC -O2 -w -DT_CDC=0 -DAISLING_BOARD_SMK37=1 -Ifirmware/src -Ifirmware/hal -o $OUT/usb_desc_smk tests/host/usb_desc_test.c -lm && $OUT/usb_desc_smk"
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
# its screen's and ADC's registers, faked; the control moves every pin to the next port, which must fail
run smk-hw sh -c "$CC $SMK_IN -o $OUT/smk_hw_test tests/host/smk_hw_test.c && $OUT/smk_hw_test && \
    $CC $SMK_IN -DCONTROL -o $OUT/smk_hw_control tests/host/smk_hw_test.c && ! $OUT/smk_hw_control >/dev/null"
# the one pin table (hal/smk37/smk_pins.h) and every SMK driver's pin setup; the control rotates the ports, which
# must fail. Then the build's guard against the FM-1's hardware headers in an SMK unit (tools/build.py)
run smk-pins sh -c "$CC $SMK_IN -o $OUT/smk_pins_test tests/host/smk_pins_test.c && $OUT/smk_pins_test && \
    $CC $SMK_IN -DCONTROL -o $OUT/smk_pins_control tests/host/smk_pins_test.c && ! $OUT/smk_pins_control >/dev/null"
run pin-guard python3 tests/pin_guard_test.py
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
    # a backup package's loader (tools/build.py --dry-loader): it compares, and writes no byte of the app area
    run update-loader-dry sh -c "$CC -DLDR_DRYRUN=1 -o $OUT/ldr_test_dry tests/ldr_test.c && \
        $OUT/ldr_test_dry $OUT/old.fwsc build/amb.fwsc"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/amb.fwsc: run ./build.sh)"
fi
# the SMK-37 Elite's update path: its packages have 36 marker blocks, and its own staging, record and app area
# (tests/fwsc.h); against its package from tools/mini_build.sh --board smk37. The control runs the FM-1's build of
# the test on it, which must fail (36 marker blocks, not 20)
SMK_PKG=
for f in build/smk37/amb.fwsc build/smk37/aisling-smk37-elite-*.fwsc; do
    case "$f" in *-dryloader.fwsc) continue ;; esac     # a backup package: the same app, another loader
    [ -f "$f" ] && SMK_PKG="$f"
done
if [ -n "$SMK_PKG" ]; then
    run ota-entry-smk37 sh -c "$CC -DAISLING_BOARD_SMK37=1 -o $OUT/ota_test_smk tests/ota_test.c && $OUT/ota_test_smk $SMK_PKG && \
        $CC -o $OUT/ota_test_fm1 tests/ota_test.c && ! $OUT/ota_test_fm1 $SMK_PKG >/dev/null"
    if [ -n "${AC79_SDK:-}" ] && [ -f build/smk37/amb.bin ] && [ -f build/smk37/loader/ota.bin ]; then
    run update-loader-smk37 sh -c "head -c 100000 build/smk37/amb.bin > $OUT/old_smk_app.bin && \
        python3 tools/fm1pkg_make.py --board smk37 $OUT/old_smk_app.bin build/smk37/loader/ota.bin $OUT/old_smk.fwsc >/dev/null && \
        $CC -DAISLING_BOARD_SMK37=1 -o $OUT/ldr_test_smk tests/ldr_test.c && $OUT/ldr_test_smk $OUT/old_smk.fwsc $SMK_PKG"
    run update-loader-dry-smk37 sh -c "$CC -DAISLING_BOARD_SMK37=1 -DLDR_DRYRUN=1 -o $OUT/ldr_test_dry_smk tests/ldr_test.c && \
        $OUT/ldr_test_dry_smk $OUT/old_smk.fwsc $SMK_PKG"
    else
        echo "  skip update-loader-smk37 (needs AC79_SDK and the SMK build's amb.bin and loader/ota.bin)"
    fi
else
    echo "  skip SMK-37 update-path tests (no build/smk37 package: tools/mini_build.sh --board smk37)"
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
# the SMK-37 Elite (build/host/amb_host_smk37): every FM-1 scenario with its controls' names (tests/smk37_scenario.py)
# must pass and write what the FM-1's run just wrote, file for file (screens, MIDI out, audio): the same app on its
# controls. Then its own (tests/scenarios_smk37/: K1 as MASTER, the outer keys, the four more buttons, the pads and faders, the
# battery). The controls must fail: an FM-1 scenario as written, `master N` (and the renamer on it), and its battery
# scenario on the FM-1's simulator
smk_same() {
    sn="$1" sd="build/scenarios_smk37/fm1/$1"
    rm -rf "$sd" && mkdir -p "$sd" || return 1
    python3 tests/smk37_scenario.py "tests/scenarios/$sn.amb" > "$sd.amb" || return 1
    build/host/amb_host_smk37 "$sd.amb" "$sd" || return 1
    for f in "$sd"/*; do
        [ -f "$f" ] || continue
        cmp "$f" "build/scenarios/$sn/${f##*/}" || return 1
    done
    echo "the same files as the FM-1's run"
}
for s in tests/scenarios/*.amb; do
    n=$(basename "$s" .amb)
    run "smk37-$n" smk_same "$n"
done
for s in tests/scenarios_smk37/*.amb; do
    n=$(basename "$s" .amb)
    mkdir -p "build/scenarios_smk37/$n"
    run "smk37-own-$n" build/host/amb_host_smk37 "$s" "build/scenarios_smk37/$n"
done
C=build/scenarios_smk37/control
mkdir -p "$C"
run smk37-control sh -c "! build/host/amb_host_smk37 tests/scenarios/pages.amb $C >/dev/null && \
    printf 'wait 10\nmaster 100\n' > $C.amb && ! build/host/amb_host_smk37 $C.amb $C >/dev/null && \
    ! python3 tests/smk37_scenario.py $C.amb >/dev/null 2>&1 && \
    ! build/host/amb_host tests/scenarios_smk37/battery.amb $C >/dev/null"
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
