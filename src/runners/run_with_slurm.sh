#!/bin/bash
#SBATCH --job-name=hashjoin
#SBATCH --time=00:01:00
#SBATCH --nodes=1
#SBATCH --partition=normal
#SBATCH --nodelist=node05
#SBATCH --output=out/slurm-%j.log
#SBATCH --error=err/slurm-%j.log

set -euo pipefail

if [ "$#" -ne 8 ]; then
    echo "Usage: $0 EXECUTABLE NR NS SEED MAX_KEY P PARTITION_THREADS JOIN_THREADS"
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

SUBMIT_DIR="${SLURM_SUBMIT_DIR:-$(pwd)}"
cd "$SUBMIT_DIR"

if [[ "$EXECUTABLE" != /* ]]; then
    EXECUTABLE="$SUBMIT_DIR/$EXECUTABLE"
fi

if [ ! -x "$EXECUTABLE" ]; then
    echo "Executable not found or not executable: $EXECUTABLE"
    exit 1
fi

"$EXECUTABLE" \
    -nr "$NR" \
    -ns "$NS" \
    -seed "$SEED" \
    -max-key "$MAX_KEY" \
    -p "$P" \
    --partition-threads "$PARTITION_THREADS" \
    --join-threads "$JOIN_THREADS"
