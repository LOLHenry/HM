#!/bin/bash
# MV-HEVC Multi-Profile Test Suite
# Tests: standard 2-view, mixed bitdepth, mixed chroma, uniform 4:4:4 12-bit,
#        3-layer mixed format, 3-layer simulcast

set -e

HMDIR="$(cd "$(dirname "$0")/.." && pwd)"
TESTDIR="$HMDIR/test"
ENC="$HMDIR/bin/xcode/clang-17.0/x86_64/relwithdebinfo/TAppEncoder"
DEC="$HMDIR/bin/xcode/clang-17.0/x86_64/relwithdebinfo/TAppDecoder"
CFGDIR="$HMDIR/cfg/MV-HEVC"

if [ ! -x "$ENC" ]; then
    echo "ERROR: Encoder not found at $ENC"
    exit 1
fi
if [ ! -x "$DEC" ]; then
    echo "ERROR: Decoder not found at $DEC"
    exit 1
fi

PASS=0
FAIL=0
TESTS=""

run_test() {
    local test_name="$1"
    local base_cfg="$2"
    local extra_args="$3"
    local expect_fail="${4:-0}"
    local skip_seq_qp="${5:-0}"

    echo ""
    echo "================================================================"
    echo "TEST: $test_name"
    echo "================================================================"

    local workdir="$TESTDIR/out_${test_name}"
    rm -rf "$workdir"
    mkdir -p "$workdir"

    # --- ENCODE ---
    echo "  [ENCODE] Running encoder..."
    local enc_cmd
    if [ "$skip_seq_qp" = "1" ]; then
        enc_cmd="$ENC -c $base_cfg $extra_args"
    else
        enc_cmd="$ENC -c $base_cfg -c $TESTDIR/seqCfg_test.cfg -c $TESTDIR/qpCfg_test.cfg $extra_args"
    fi
    echo "  CMD: $enc_cmd"

    if (cd "$workdir" && $enc_cmd > enc.log 2>&1); then
        echo "  [ENCODE] SUCCESS"
    else
        local enc_exit=$?
        echo "  [ENCODE] FAILED (exit code $enc_exit)"
        echo "  --- Last 30 lines of encoder log ---"
        tail -30 "$workdir/enc.log"
        echo "  --- End of log ---"
        if [ "$expect_fail" = "1" ]; then
            echo "  (Expected failure)"
            TESTS="$TESTS\n  SKIP: $test_name (expected failure)"
            return 0
        fi
        FAIL=$((FAIL + 1))
        TESTS="$TESTS\n  FAIL: $test_name (encoder)"
        return 1
    fi

    # Show encoder summary (last few lines usually have PSNR summary)
    echo "  --- Encoder Summary ---"
    grep -E "Total|Summary|PSNR|Bitrate|LayerId" "$workdir/enc.log" | tail -10 || true
    echo ""

    # Check bitstream was created
    local bitstream=$(ls "$workdir"/*.bit 2>/dev/null | head -1)
    if [ -z "$bitstream" ]; then
        echo "  [ERROR] No bitstream file generated!"
        FAIL=$((FAIL + 1))
        TESTS="$TESTS\n  FAIL: $test_name (no bitstream)"
        return 1
    fi
    local bs_size=$(wc -c < "$bitstream")
    echo "  Bitstream: $(basename "$bitstream") ($bs_size bytes)"

    # --- DECODE ---
    echo "  [DECODE] Running decoder..."
    local dec_cmd="$DEC -b $bitstream -o $workdir/dec_out.yuv"
    echo "  CMD: $dec_cmd"

    if (cd "$workdir" && $dec_cmd > dec.log 2>&1); then
        echo "  [DECODE] SUCCESS"
    else
        local dec_exit=$?
        echo "  [DECODE] FAILED (exit code $dec_exit)"
        echo "  --- Last 20 lines of decoder log ---"
        tail -20 "$workdir/dec.log"
        echo "  --- End of log ---"
        FAIL=$((FAIL + 1))
        TESTS="$TESTS\n  FAIL: $test_name (decoder)"
        return 1
    fi

    # Show decoder output
    echo "  --- Decoder Summary ---"
    grep -E "POC|decoded|hash|layerId|Output" "$workdir/dec.log" | tail -10 || true

    # Check reconstructed files exist
    local rec_files=$(ls "$workdir"/rec_*.yuv "$workdir"/dec_*.yuv 2>/dev/null | wc -l)
    echo "  Reconstructed files: $rec_files"

    PASS=$((PASS + 1))
    TESTS="$TESTS\n  PASS: $test_name"
    echo "  [RESULT] PASS"
}

echo "================================================================"
echo "MV-HEVC Multi-Profile Test Suite"
echo "================================================================"
echo "Encoder: $ENC"
echo "Decoder: $DEC"
echo ""

# --- TEST 1: Standard 2-view 4:2:0 8-bit (Regression) ---
run_test "standard_2view_420_8bit" \
    "$CFGDIR/baseCfg_2view.cfg" \
    "--InputFile_0=$TESTDIR/test_420_8bit_v0.yuv --InputFile_1=$TESTDIR/test_420_8bit_v1.yuv"

# --- TEST 2: Mixed bitdepth (4:2:0 8-bit + 4:2:0 10-bit) ---
run_test "mixed_bitdepth_420" \
    "$CFGDIR/baseCfg_2view_mixed_bitdepth.cfg" \
    "--InputFile_0=$TESTDIR/test_420_8bit_v0.yuv --InputFile_1=$TESTDIR/test_420_10bit_v1.yuv"

# --- TEST 3: Mixed chroma (4:2:0 8-bit + 4:4:4 10-bit) ---
run_test "mixed_chroma_444" \
    "$CFGDIR/baseCfg_2view_mixed444.cfg" \
    "--InputFile_0=$TESTDIR/test_420_8bit_v0.yuv --InputFile_1=$TESTDIR/test_444_10bit_v1.yuv"

# --- TEST 4: Uniform 4:4:4 12-bit ---
run_test "uniform_444_12bit" \
    "$CFGDIR/baseCfg_2view_444_12bit.cfg" \
    "--InputFile_0=$TESTDIR/test_444_12bit_v0.yuv --InputFile_1=$TESTDIR/test_444_12bit_v1.yuv"

# --- TEST 5: 3-layer mixed format (inter-layer prediction + independent) ---
run_test "3layer_mixed_format" \
    "$CFGDIR/baseCfg_3view_mixed.cfg" \
    "--InputFile_0=$TESTDIR/test_420_8bit_v0.yuv --InputFile_1=$TESTDIR/test_420_8bit_v1.yuv --InputFile_2=$TESTDIR/test_444_10bit_64x64_v2.yuv" \
    0 1

# --- TEST 6: 3-layer all-independent simulcast (mixed resolution) ---
run_test "3layer_simulcast" \
    "$CFGDIR/baseCfg_3view_simulcast.cfg" \
    "--InputFile_0=$TESTDIR/test_420_8bit_v0.yuv --InputFile_1=$TESTDIR/test_420_8bit_64x64_v1.yuv --InputFile_2=$TESTDIR/test_444_10bit_v1.yuv" \
    0 1

echo ""
echo "================================================================"
echo "TEST RESULTS SUMMARY"
echo "================================================================"
echo -e "$TESTS"
echo ""
echo "PASSED: $PASS"
echo "FAILED: $FAIL"
echo "================================================================"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
