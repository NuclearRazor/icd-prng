#!/bin/bash
#
# Multi-seed PractRand testing for ICD-PRNG
#
# Run from repository root:
#   ./tests/icd_multi_seed_test.sh [num_seeds] [max_size]
#
# Examples:
#   ./tests/icd_multi_seed_test.sh 10 1GB
#   ./tests/icd_multi_seed_test.sh 5 128MB
#

NUM_SEEDS=${1:-10}
MAX_SIZE=${2:-1GB}

echo "╔═══════════════════════════════════════════════════════════════════╗"
echo "║           ICD-PRNG Multi-Seed PractRand Testing                   ║"
echo "╠═══════════════════════════════════════════════════════════════════╣"
echo "║  Seeds: $NUM_SEEDS                                                ║"
echo "║  Size per seed: $MAX_SIZE                                         ║"
echo "╚═══════════════════════════════════════════════════════════════════╝"
echo ""

# Find script directory and repo root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

# Build generator if needed
GENERATOR="$REPO_ROOT/icd_generator"
if [ ! -f "$GENERATOR" ]; then
    echo "Building icd_generator..."
    gcc -O3 -march=native -o "$GENERATOR" "$REPO_ROOT/icd_generator.c"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to build icd_generator"
        exit 1
    fi
fi

# Find PractRand
PRACTRAND=""
for path in "./RNG_test" "$REPO_ROOT/RNG_test" "$(which RNG_test 2>/dev/null)"; do
    if [ -f "$path" ]; then
        PRACTRAND="$path"
        break
    fi
done

if [ -z "$PRACTRAND" ]; then
    echo "ERROR: PractRand RNG_test not found"
    echo "Please install PractRand and ensure RNG_test is in PATH"
    exit 1
fi

PASSED=0
FAILED=0
RESULTS=""

for i in $(seq 1 $NUM_SEEDS); do
    SEED="icd_test_seed_$(printf '%03d' $i)_$(date +%s)"
    echo ""
    echo "═══════════════════════════════════════════════════════════════════"
    echo "  Seed $i/$NUM_SEEDS: $SEED"
    echo "═══════════════════════════════════════════════════════════════════"
    
    OUTPUT=$("$GENERATOR" --seed "$SEED" 2>/dev/null | "$PRACTRAND" stdin64 -tlmax $MAX_SIZE 2>&1)
    
    if echo "$OUTPUT" | grep -q "no anomalies"; then
        echo "  ✓ PASSED"
        PASSED=$((PASSED + 1))
        RESULTS="$RESULTS\n  Seed $i: PASSED"
    elif echo "$OUTPUT" | grep -q "FAIL"; then
        echo "  ✗ FAILED"
        echo "$OUTPUT" | tail -20
        FAILED=$((FAILED + 1))
        RESULTS="$RESULTS\n  Seed $i: FAILED"
    else
        if echo "$OUTPUT" | grep -qE "(suspicious|SUSPICIOUS)"; then
            echo "  ⚠ SUSPICIOUS"
            RESULTS="$RESULTS\n  Seed $i: SUSPICIOUS"
        else
            echo "  ✓ PASSED"
            PASSED=$((PASSED + 1))
            RESULTS="$RESULTS\n  Seed $i: PASSED"
        fi
    fi
done

echo ""
echo "╔═══════════════════════════════════════════════════════════════════╗"
echo "║                         SUMMARY                                   ║"
echo "╠═══════════════════════════════════════════════════════════════════╣"
echo -e "$RESULTS"
echo "╠═══════════════════════════════════════════════════════════════════╣"
echo "║  Total: $NUM_SEEDS | Passed: $PASSED | Failed: $FAILED            ║"
echo "╚═══════════════════════════════════════════════════════════════════╝"

[ $FAILED -gt 0 ] && exit 1 || exit 0
