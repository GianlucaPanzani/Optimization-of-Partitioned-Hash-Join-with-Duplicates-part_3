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
# Supported by hashjoin_omp_loop.cpp: static | dynamic | guided | auto
PARTITION_SCHEDULE_VALUES=(static guided auto)
JOIN_SCHEDULE_VALUES=(static guided auto)
# Use 0 to mean "no explicit chunk" (e.g. schedule(static) instead of schedule(static,chunk))
PARTITION_CHUNK_VALUES=(0 32)
JOIN_CHUNK_VALUES=(0 32)
# Block size used by the parallel partitioning implementation (for the block-based histogram/scatter phase)
PARTITION_BLOCK_SIZE_VALUES=(32768 131072)

# --- Reduced OMP combinations ---
# OpenMP thread configurations
PARTITION_THREAD_VALUES=(32 64)
JOIN_THREAD_VALUES=(32 64)
# Supported by hashjoin_omp_loop.cpp: static | dynamic | guided | auto
#PARTITION_SCHEDULE_VALUES=(static)
#JOIN_SCHEDULE_VALUES=(static)
# Use 0 to mean "no explicit chunk" (e.g. schedule(static) instead of schedule(static,chunk))
PARTITION_CHUNK_VALUES=(0 8)
JOIN_CHUNK_VALUES=(0 8)
# Block size used by the parallel partitioning implementation (for the block-based histogram/scatter phase)
PARTITION_BLOCK_SIZE_VALUES=(16384 32768)