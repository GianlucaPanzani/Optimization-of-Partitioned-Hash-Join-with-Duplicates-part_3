#!/bin/bash

# Static parameters
N_VALUES=(10000000) # --> da modificare con 50 mln
P_VALUES=(256)
SEED_VALUES=(13)
MAX_KEY_VALUES=(1000000)

# OpenMP thread configurations
PARTITION_THREAD_VALUES=(4 8 16 32 64)
JOIN_THREAD_VALUES=(4 8 16 32 64)

# Supported by hashjoin_omp_loop.cpp: static | dynamic | guided | auto
PARTITION_SCHEDULE_VALUES=(auto static dynamic guided)
JOIN_SCHEDULE_VALUES=(auto static dynamic guided)

# Use 0 to mean "no explicit chunk" (e.g. schedule(static) instead of schedule(static,chunk))
PARTITION_CHUNK_VALUES=(0 8 16 32)
JOIN_CHUNK_VALUES=(0 8 16 32)

# Block size used by the parallel partitioning implementation (for the block-based histogram/scatter phase)
PARTITION_BLOCK_SIZE_VALUES=(32768 65536 131072)