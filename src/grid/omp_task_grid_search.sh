#!/bin/bash

# Static parameters
N_VALUES=(10000000) # --> da modificare con 50 mln
P_VALUES=(256)
SEED_VALUES=(13)
MAX_KEY_VALUES=(1000000)

# --- Full OMP combinations ---
# OpenMP thread configurations
PARTITION_THREAD_VALUES=(16 32 64)
JOIN_THREAD_VALUES=(16 32 64)
# Block size used by the parallel partitioning implementation (for the block-based histogram/scatter phase)
PARTITION_BLOCK_SIZE_VALUES=(32768 131072)
# Taskloop grains (partition_task_grain measured in input blocks, join/offset grains measured in partitions)
PARTITION_TASK_GRAIN_VALUES=(1 32)
JOIN_TASK_GRAIN_VALUES=(1 32)
OFFSET_TASK_GRAIN_VALUES=(1)

# --- Reduced OMP combinations ---
# OpenMP thread configurations
PARTITION_THREAD_VALUES=(64)
JOIN_THREAD_VALUES=(32 64)
# Block size used by the parallel partitioning implementation (for the block-based histogram/scatter phase)
PARTITION_BLOCK_SIZE_VALUES=(16384 32768)
# Taskloop grains (partition_task_grain measured in input blocks, join/offset grains measured in partitions)
PARTITION_TASK_GRAIN_VALUES=(2 4)
JOIN_TASK_GRAIN_VALUES=(4 8)
OFFSET_TASK_GRAIN_VALUES=(2 4 8)

# --- Unused parameters for the OMP loop version ---
PARTITION_SCHEDULE_VALUES=(auto)
JOIN_SCHEDULE_VALUES=(auto)
PARTITION_CHUNK_VALUES=(0)
JOIN_CHUNK_VALUES=(0)
