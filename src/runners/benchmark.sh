#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE_DIR="$SCRIPT_DIR/.."
RUNNER_SCRIPT="$SCRIPT_DIR/run_with_slurm.sh"

cd "$EXE_DIR"
mkdir -p out err

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 EXECUTABLE_NAME [GRID_CONFIG(optional)] [REPEAT_COUNT(optional)]"
    echo "  EXECUTABLE_NAME: make target to compile and benchmark (ex: hashjoin_sequential, hashjoin_parallel)"
    echo "  GRID_CONFIG: shell file exporting benchmark arrays (default: $EXE_DIR/grid/seq_grid_search.sh)"
    echo "  REPEAT_COUNT: positive integer (default: 1)"
    exit 1
fi

EXECUTABLE_INPUT="$1"
EXECUTABLE_TARGET="$(basename "$EXECUTABLE_INPUT")"
GRID_CONFIG="$EXE_DIR/grid/seq_grid_search.sh"
REPEAT_COUNT="1"

if [ $# -eq 2 ]; then
    if [[ "$2" =~ ^[0-9]+$ ]]; then
        REPEAT_COUNT="$2"
    else
        if [[ "$2" = /* ]]; then
            GRID_CONFIG="$2"
        else
            GRID_CONFIG="$EXE_DIR/$2"
        fi
    fi
elif [ $# -eq 3 ]; then
    if [[ "$2" = /* ]]; then
        GRID_CONFIG="$2"
    else
        GRID_CONFIG="$EXE_DIR/$2"
    fi
    REPEAT_COUNT="$3"
fi

if ! [[ "$REPEAT_COUNT" -ge 1 ]]; then
    echo "REPEAT_COUNT must be greater than 0, received: $REPEAT_COUNT"
    exit 1
fi

if [ ! -f "$GRID_CONFIG" ]; then
    echo "Grid configuration file not found: $GRID_CONFIG"
    exit 1
fi
if [ ! -f "$RUNNER_SCRIPT" ]; then
    echo "Runner script not found: $RUNNER_SCRIPT"
    exit 1
fi

source "$GRID_CONFIG"

require_non_empty_array() {
    local array_name="$1"
    local array_len
    local declaration

    if [[ ! "$array_name" =~ ^[a-zA-Z_][a-zA-Z0-9_]*$ ]]; then
        return 1
    fi
    if ! declaration="$(declare -p "$array_name" 2>/dev/null)"; then
        return 1
    fi
    if [[ "$declaration" != declare\ -a* && "$declaration" != declare\ -A* ]]; then
        return 1
    fi

    eval "array_len=\${#$array_name[@]}"
    [ "$array_len" -gt 0 ]
}

if ! require_non_empty_array "N_VALUES" || ! require_non_empty_array "P_VALUES" || \
   ! require_non_empty_array "SEED_VALUES" || ! require_non_empty_array "MAX_KEY_VALUES" || \
   ! require_non_empty_array "PARTITION_THREAD_VALUES" || ! require_non_empty_array "JOIN_THREAD_VALUES"; then
    echo "Grid configuration must define non-empty N_VALUES, P_VALUES, SEED_VALUES, MAX_KEY_VALUES, PARTITION_THREAD_VALUES and JOIN_THREAD_VALUES."
    exit 1
fi

if [[ "$EXECUTABLE_TARGET" == "hashjoin_seq" ]]; then
    make cleanall_seq
elif [[ "$EXECUTABLE_TARGET" == "hashjoin_par_p" ]]; then
    make cleanall_par_p
elif [[ "$EXECUTABLE_TARGET" == "hashjoin_par_pj" ]]; then
    make cleanall_par_pj
elif [[ "$EXECUTABLE_TARGET" == "hashjoin_par_pj_wb" ]]; then
    make cleanall_par_pj_wb
elif [[ "$EXECUTABLE_TARGET" == "hashjoin_par_pj_wb_map" ]]; then
    make cleanall_par_pj_wb_map
elif [[ "$EXECUTABLE_TARGET" == "weak_scaling" ]]; then
    make cleanall_weak_scaling
else
    echo "Cleaning not done"
    exit 1
fi

mkdir -p compilation
if ! make -B "$EXECUTABLE_TARGET"; then
    echo "Compilation failed or unknown make target: $EXECUTABLE_TARGET"
    exit 1
fi
EXECUTABLE="$EXE_DIR/$EXECUTABLE_TARGET"
if [ ! -x "$EXECUTABLE" ]; then
    echo "Compiled executable not found or not executable: $EXECUTABLE"
    exit 1
fi

TOTAL=$(( ${#N_VALUES[@]} * ${#P_VALUES[@]} * ${#SEED_VALUES[@]} * ${#MAX_KEY_VALUES[@]} * ${#PARTITION_THREAD_VALUES[@]} * ${#JOIN_THREAD_VALUES[@]} * REPEAT_COUNT ))
COUNT=0

echo
echo "Benchmark executable: $EXECUTABLE_TARGET"
echo "Grid source:          $GRID_CONFIG"
echo "N values:             ${N_VALUES[*]}"
echo "P values:             ${P_VALUES[*]}"
echo "Repeat count:         $REPEAT_COUNT"
echo "Total runs:           $TOTAL"
echo

for ((RUN_INDEX=1; RUN_INDEX<=REPEAT_COUNT; RUN_INDEX++)); do
    for N in "${N_VALUES[@]}"; do
        for P in "${P_VALUES[@]}"; do
            for SEED in "${SEED_VALUES[@]}"; do
                for MAX_KEY in "${MAX_KEY_VALUES[@]}"; do
                    for PARTITION_THREADS in "${PARTITION_THREAD_VALUES[@]}"; do
                        for JOIN_THREADS in "${JOIN_THREAD_VALUES[@]}"; do
                            COUNT=$((COUNT + 1))

                            printf '[%d/%d] run=%d/%d N=%s P=%s seed=%s max_key=%s partition_threads=%s join_threads=%s\n' \
                                "$COUNT" "$TOTAL" "$RUN_INDEX" "$REPEAT_COUNT" \
                                "$N" "$P" "$SEED" "$MAX_KEY" "$PARTITION_THREADS" "$JOIN_THREADS"

                            JOB_ID=$(sbatch --parsable --wait \
                                "$RUNNER_SCRIPT" \
                                "$EXECUTABLE" \
                                "$N" \
                                "$N" \
                                "$SEED" \
                                "$MAX_KEY" \
                                "$P" \
                                "$PARTITION_THREADS" \
                                "$JOIN_THREADS")
                        done
                    done
                done
            done
        done
    done
done

echo
make checker
./checker
make clean
