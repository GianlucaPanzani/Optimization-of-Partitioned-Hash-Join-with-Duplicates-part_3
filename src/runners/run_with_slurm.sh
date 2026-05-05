#!/bin/bash
#SBATCH --job-name=hashjoin
#SBATCH --time=00:01:00
#SBATCH --nodes=1
#SBATCH --partition=normal
#SBATCH --nodelist=node08
#SBATCH --output=out/slurm-%j.log
#SBATCH --error=err/slurm-%j.log

set -euo pipefail

if [ "$#" -ne 13 ] && [ "$#" -ne 16 ]; then
    echo "Usage: $0 EXECUTABLE NR NS SEED MAX_KEY P PARTITION_THREADS JOIN_THREADS PARTITION_SCHEDULE JOIN_SCHEDULE PARTITION_CHUNK JOIN_CHUNK PARTITION_BLOCK_SIZE [PARTITION_TASK_GRAIN JOIN_TASK_GRAIN OFFSET_TASK_GRAIN]"
    exit 1
fi

EXECUTABLE="$1"
NR="$2"
NS="$3"
SEED="$4"
MAX_KEY="$5"
P="$6"
PARTITION_THREADS="$7"
JOIN_THREADS="$8"
PARTITION_SCHEDULE="$9"
JOIN_SCHEDULE="${10}"
PARTITION_CHUNK="${11}"
JOIN_CHUNK="${12}"
PARTITION_BLOCK_SIZE="${13}"
PARTITION_TASK_GRAIN="${14:-}"
JOIN_TASK_GRAIN="${15:-}"
OFFSET_TASK_GRAIN="${16:-}"

SUBMIT_DIR="${SLURM_SUBMIT_DIR:-$(pwd)}"
cd "$SUBMIT_DIR"

if [[ "$EXECUTABLE" != /* ]]; then
    EXECUTABLE="$SUBMIT_DIR/$EXECUTABLE"
fi

if [ ! -x "$EXECUTABLE" ]; then
    echo "Executable not found or not executable: $EXECUTABLE"
    exit 1
fi

MAX_THREADS="$PARTITION_THREADS"
if [ "$JOIN_THREADS" -gt "$MAX_THREADS" ]; then
    MAX_THREADS="$JOIN_THREADS"
fi

export OMP_NUM_THREADS="$MAX_THREADS"
export OMP_DISPLAY_ENV="${OMP_DISPLAY_ENV:-false}"

runner_args=(
    "$EXECUTABLE"
    -nr "$NR"
    -ns "$NS"
    -seed "$SEED"
    -max-key "$MAX_KEY"
    -p "$P"
    --partition-threads "$PARTITION_THREADS"
    --join-threads "$JOIN_THREADS"
    --partition-schedule "$PARTITION_SCHEDULE"
    --join-schedule "$JOIN_SCHEDULE"
    --partition-chunk "$PARTITION_CHUNK"
    --join-chunk "$JOIN_CHUNK"
    --partition-block-size "$PARTITION_BLOCK_SIZE"
    --partition-task-grain "$PARTITION_TASK_GRAIN"
    --join-task-grain "$JOIN_TASK_GRAIN"
    --offset-task-grain "$OFFSET_TASK_GRAIN"
)

"${runner_args[@]}"
