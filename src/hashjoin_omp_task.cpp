// hashjoin_omp_task.cpp
//
// Task-based OpenMP implementation for Module 3
// Partitioned Hash Join with Duplicates
//
// This file is intentionally kept close to hashjoin_seq.cpp, with only the
// modifications needed to express the partitioning and local-join phases with
// OpenMP tasks/taskloop constructs.
//
// Run example:
//   ./hashjoin_omp_task -nr 5 -ns 8 -seed 13 -max-key 8 -p 4
//
// Output:
//   join_count
//   checksum1
//   checksum2

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <omp.h>

#include "lib/timing.hpp"
#include "lib/results.hpp"

// ------------------------------------------------------------
// Record definition
// ------------------------------------------------------------
struct Record {
    std::uint64_t key{};
};

// ------------------------------------------------------------
// Utility: command-line parsing
// ------------------------------------------------------------
static bool read_arg_u64(int argc, char** argv,
                         std::initializer_list<std::string_view> names,
                         std::uint64_t& out) {
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string_view arg(argv[i]);
        for (const auto name : names) {
            if (arg == name) {
                out = std::strtoull(argv[i + 1], nullptr, 10);
                return true;
            }
        }
    }
    return false;
}
static bool read_arg_string(int argc, char** argv,
                            std::initializer_list<std::string_view> names,
                            std::string& out) {
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string_view arg(argv[i]);
        for (const auto name : names) {
            if (arg == name) {
                out = argv[i + 1];
                return true;
            }
        }
    }
    return false;
}
static void usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog
        << " -nr NR -ns NS -seed SEED -max-key K -p P\n"
        << "       --dataset-type uniform|skewed_RECORDPCT_PARTITIONPCT\n"
        << "       --partition-threads T --join-threads T\n"
        << "       --partition-schedule NAME --join-schedule NAME\n"
        << "       --partition-chunk C --join-chunk C\n"
        << "       --partition-block-size B\n"
        << "       --partition-task-grain G --join-task-grain G --offset-task-grain G\n\n"
        << "Parameters:\n"
        << "  -nr                        Number of records in relation R\n"
        << "  -ns                        Number of records in relation S\n"
        << "  -seed                      Deterministic seed\n"
        << "  -max-key                   Keys are generated in [0, max-key)\n"
        << "  -p                         Number of partitions (power of two required in this reference code)\n"
        << "  --dataset-type             Input distribution, e.g. uniform or skewed_80_5\n"
        << "  --partition-threads        Number of threads for task-based partition phase\n"
        << "  --join-threads             Number of threads for task-based join phase\n"
        << "  --partition-schedule / --join-schedule are kept for compatibility/logging\n"
        << "  --partition-chunk / --join-chunk are kept for compatibility/logging\n"
        << "  --partition-block-size     Number of records per input block during partitioning\n"
        << "  --partition-task-grain     taskloop grainsize, in input blocks, for histogram/scatter\n"
        << "  --join-task-grain          taskloop grainsize, in partitions, for local joins\n"
        << "  --offset-task-grain        taskloop grainsize, in partitions, for offset/prefix-related loops\n";
}
static bool is_power_of_two(std::uint32_t x) {
    return x != 0 && (x & (x - 1U)) == 0;
}

// ------------------------------------------------------------
// OpenMP task configuration
// ------------------------------------------------------------
struct OmpTaskConfig {
    int partition_threads = 1;
    int join_threads = 1;
    std::size_t partition_block_size = 65536;

    // taskloop grainsize values.
    // partition_task_grain is measured in input blocks.
    // join_task_grain and offset_task_grain are measured in partitions.
    int partition_task_grain = 1;
    int join_task_grain = 1;
    int offset_task_grain = 1;

    // Kept only for compatibility with the same runner/grid format used by
    // hashjoin_omp_loop.cpp. They do not control task scheduling.
    std::string partition_schedule_name = "taskloop";
    std::string join_schedule_name = "taskloop";
    int partition_chunk = 0;
    int join_chunk = 0;
};

static int checked_positive_int(std::uint64_t value, const char* name) {
    if (value == 0 || value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(std::string("Invalid ") + name + ": must be in [1, INT_MAX]");
    }
    return static_cast<int>(value);
}

static int checked_nonnegative_int(std::uint64_t value, const char* name) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(std::string("Invalid ") + name + ": too large");
    }
    return static_cast<int>(value);
}

static int grain_from_optional(std::uint64_t explicit_grain,
                               bool has_explicit_grain,
                               std::uint64_t fallback_chunk,
                               const char* name) {
    if (has_explicit_grain) {
        return checked_positive_int(explicit_grain, name);
    }
    if (fallback_chunk != 0) {
        return checked_positive_int(fallback_chunk, name);
    }
    return 1;
}


// ------------------------------------------------------------
// Dataset generation configuration
// ------------------------------------------------------------
struct DatasetConfig {
    std::string type = "uniform";
    std::string name = "uniform";
    double skew_fraction = 0.0;
    double skew_partition_fraction = 1.0;
};

static inline std::uint32_t compute_partition_id(std::uint64_t key, std::uint32_t P);

static bool parse_percentage_token(const std::string& token, double& out) {
    try {
        std::size_t parsed = 0;
        const double percent = std::stod(token, &parsed);
        if (parsed != token.size()) {
            return false;
        }
        out = percent / 100.0;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

static bool parse_dataset_type(const std::string& value, DatasetConfig& cfg) {
    if (value == "uniform") {
        cfg.type = value;
        cfg.name = "uniform";
        cfg.skew_fraction = 0.0;
        cfg.skew_partition_fraction = 1.0;
        return true;
    }

    constexpr std::string_view prefix = "skewed_";
    if (value.rfind(std::string(prefix), 0) != 0) {
        std::cerr << "Error: dataset-type must be uniform or skewed_RECORDPCT_PARTITIONPCT.\n";
        return false;
    }

    const std::string payload = value.substr(prefix.size());
    const std::size_t separator = payload.find('_');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= payload.size()) {
        std::cerr << "Error: skewed dataset-type must use the format skewed_RECORDPCT_PARTITIONPCT.\n";
        return false;
    }

    double skew_fraction = 0.0;
    double skew_partition_fraction = 0.0;
    if (!parse_percentage_token(payload.substr(0, separator), skew_fraction) ||
        !parse_percentage_token(payload.substr(separator + 1), skew_partition_fraction)) {
        std::cerr << "Error: skewed dataset-type percentages must be numeric.\n";
        return false;
    }

    cfg.type = value;
    cfg.name = "skewed";
    cfg.skew_fraction = skew_fraction;
    cfg.skew_partition_fraction = skew_partition_fraction;
    return true;
}

static bool validate_dataset_config(const DatasetConfig& cfg) {
    if (cfg.name != "uniform" && cfg.name != "skewed") {
        std::cerr << "Error: dataset must be either 'uniform' or 'skewed'.\n";
        return false;
    }
    if (cfg.skew_fraction < 0.0 || cfg.skew_fraction > 1.0) {
        std::cerr << "Error: skew-fraction must be in [0, 1].\n";
        return false;
    }
    if (cfg.skew_partition_fraction <= 0.0 || cfg.skew_partition_fraction > 1.0) {
        std::cerr << "Error: skew-partition-fraction must be in (0, 1].\n";
        return false;
    }
    return true;
}

static std::vector<std::uint64_t> build_skew_key_pool(std::uint32_t P,
                                                      std::uint64_t max_key,
                                                      double skew_partition_fraction) {
    const std::size_t hot_partitions = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(static_cast<double>(P) * skew_partition_fraction)));

    std::vector<std::uint64_t> pool;
    if (max_key == 0) {
        if (compute_partition_id(0, P) < hot_partitions) {
            pool.push_back(0);
        }
        return pool;
    }

    for (std::uint64_t key = 0; key < max_key; ++key) {
        if (compute_partition_id(key, P) < hot_partitions) {
            pool.push_back(key);
        }
    }
    return pool;
}

// Deterministic pseudo-random generation
// ------------------------------------------------------------
static inline std::uint64_t splitmix64_mix(std::uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}
static inline std::uint64_t splitmix64(std::uint64_t x) {
    return splitmix64_mix(x + 0x9e3779b97f4a7c15ULL);
}
static inline std::uint64_t splitmix64_next(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    return splitmix64_mix(state);
}

static std::vector<Record> generate_relation(std::size_t n,
                                            std::uint64_t seed,
                                            std::uint64_t max_key,
                                            std::uint32_t P,
                                            const DatasetConfig& dataset_cfg) {
    std::vector<Record> out(n);
    std::uint64_t state = seed;

    std::vector<std::uint64_t> skew_key_pool;
    const bool use_skew = (dataset_cfg.name == "skewed" && dataset_cfg.skew_fraction > 0.0);

    if (use_skew) {
        skew_key_pool = build_skew_key_pool(P, max_key, dataset_cfg.skew_partition_fraction);
        if (skew_key_pool.empty()) {
            throw std::runtime_error("Unable to generate skewed dataset: no keys map to the selected hot partitions. Increase max-key or skew-partition-fraction.");
        }
    }

    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t r = splitmix64_next(state);
        const double u = static_cast<double>(r >> 11) * (1.0 / 9007199254740992.0);

        if (use_skew && u < dataset_cfg.skew_fraction) {
            out[i].key = skew_key_pool[r % skew_key_pool.size()];
        } else {
            out[i].key = (max_key == 0) ? 0ULL : (r % max_key);
        }
    }
    return out;
}

// ------------------------------------------------------------
// Intentionally simple partition mapping
// ------------------------------------------------------------
static inline std::uint32_t compute_partition_id(std::uint64_t key, std::uint32_t P) {
    const std::uint32_t mask = P - 1U;
    key ^= key >> 33;
    key ^= key >> 17;
    key ^= key >> 9;
    return static_cast<std::uint32_t>(key) & mask;
}

// ------------------------------------------------------------
// Blocked histogram for task-based OpenMP partitioning
// ------------------------------------------------------------
struct HistogramResult {
    std::vector<std::size_t> hist;
    std::vector<std::size_t> block_hist; // flattened: block_hist[block * P + pid]
    std::size_t num_blocks = 0;
};

static HistogramResult compute_histogram(const std::vector<Record>& data,
                                         std::uint32_t P,
                                         const OmpTaskConfig& cfg) {
    const std::size_t n = data.size();
    const std::size_t block_size = cfg.partition_block_size;
    const std::size_t num_blocks = (n + block_size - 1) / block_size;
    const std::size_t P_size = static_cast<std::size_t>(P);
    const int partition_grain = cfg.partition_task_grain;
    const int offset_grain = cfg.offset_task_grain;

    HistogramResult out;
    out.hist.assign(P_size, 0);
    out.block_hist.assign(num_blocks * P_size, 0);
    out.num_blocks = num_blocks;

    // First pass: create tasks over input blocks. Each task writes a disjoint
    // block histogram, so there are no atomics and no races.
    #pragma omp parallel num_threads(cfg.partition_threads) default(none) shared(data, out) firstprivate(P, P_size, block_size, num_blocks, partition_grain)
    {
        #pragma omp single nowait
        {
            #pragma omp taskloop grainsize(partition_grain)
            for (std::int64_t b_signed = 0; b_signed < static_cast<std::int64_t>(num_blocks); ++b_signed) {
                const std::size_t b = static_cast<std::size_t>(b_signed);
                const std::size_t begin = b * block_size;
                const std::size_t end = std::min(begin + block_size, data.size());
                std::size_t* local = out.block_hist.data() + b * P_size;

                for (std::size_t i = begin; i < end; ++i) {
                    const std::uint32_t pid = compute_partition_id(data[i].key, P);
                    ++local[pid];
                }
            }
        }
    }

    // Reduction across blocks. Expressed as a taskloop over partitions.
    #pragma omp parallel num_threads(cfg.partition_threads) default(none) shared(out) firstprivate(P, P_size, num_blocks, offset_grain)
    {
        #pragma omp single nowait
        {
            #pragma omp taskloop grainsize(offset_grain)
            for (std::int64_t pid_signed = 0; pid_signed < static_cast<std::int64_t>(P); ++pid_signed) {
                const std::size_t pid = static_cast<std::size_t>(pid_signed);
                std::size_t sum = 0;
                for (std::size_t b = 0; b < num_blocks; ++b) {
                    sum += out.block_hist[b * P_size + pid];
                }
                out.hist[pid] = sum;
            }
        }
    }

    return out;
}

// ------------------------------------------------------------
// Prefix sum (exclusive scan)
// ------------------------------------------------------------
static std::vector<std::size_t> exclusive_prefix_sum(const std::vector<std::size_t>& hist) {
    std::vector<std::size_t> begin(hist.size(), 0);

    std::size_t running = 0;
    for (std::size_t p = 0; p < hist.size(); ++p) {
        begin[p] = running;
        running += hist[p];
    }
    return begin;
}

// ------------------------------------------------------------
// Scatter into a partitioned array
// ------------------------------------------------------------
static std::vector<Record> scatter_partitioned(const std::vector<Record>& data,
                                               std::uint32_t P,
                                               const std::vector<std::size_t>& begin,
                                               const HistogramResult& hist_result,
                                               const OmpTaskConfig& cfg) {
    std::vector<Record> out(data.size());

    const std::size_t num_blocks = hist_result.num_blocks;
    const std::size_t block_size = cfg.partition_block_size;
    const std::size_t P_size = static_cast<std::size_t>(P);
    const int partition_grain = cfg.partition_task_grain;
    const int offset_grain = cfg.offset_task_grain;

    // block_offsets[block, pid] is the first output position reserved for
    // records of partition pid coming from input block block.
    std::vector<std::size_t> block_offsets(num_blocks * P_size, 0);

    // Prefix across blocks for each partition. This creates independent tasks
    // over pid; each task writes a disjoint column of block_offsets.
    #pragma omp parallel num_threads(cfg.partition_threads) default(none) shared(block_offsets, hist_result, begin) firstprivate(P, P_size, num_blocks, offset_grain)
    {
        #pragma omp single nowait
        {
            #pragma omp taskloop grainsize(offset_grain)
            for (std::int64_t pid_signed = 0; pid_signed < static_cast<std::int64_t>(P); ++pid_signed) {
                const std::size_t pid = static_cast<std::size_t>(pid_signed);
                std::size_t running = begin[pid];
                for (std::size_t b = 0; b < num_blocks; ++b) {
                    block_offsets[b * P_size + pid] = running;
                    running += hist_result.block_hist[b * P_size + pid];
                }
            }
        }
    }

    // Scatter one taskloop iteration per input block. Each block has precomputed
    // disjoint output intervals, so the write is race-free without atomics.
    #pragma omp parallel num_threads(cfg.partition_threads) default(none) shared(data, out, block_offsets) firstprivate(P, P_size, block_size, num_blocks, partition_grain)
    {
        #pragma omp single nowait
        {
            #pragma omp taskloop grainsize(partition_grain)
            for (std::int64_t b_signed = 0; b_signed < static_cast<std::int64_t>(num_blocks); ++b_signed) {
                const std::size_t b = static_cast<std::size_t>(b_signed);
                const std::size_t in_begin = b * block_size;
                const std::size_t in_end = std::min(in_begin + block_size, data.size());

                std::vector<std::size_t> next(P_size);
                for (std::size_t pid = 0; pid < P_size; ++pid) {
                    next[pid] = block_offsets[b * P_size + pid];
                }

                for (std::size_t i = in_begin; i < in_end; ++i) {
                    const std::uint32_t pid = compute_partition_id(data[i].key, P);
                    out[next[pid]++] = data[i];
                }
            }
        }
    }

    return out;
}

// ------------------------------------------------------------
// Partitioned relation metadata
// ------------------------------------------------------------
struct PartitionedRelation {
    std::vector<Record> data;
    std::vector<std::size_t> begin;
    std::vector<std::size_t> end;
};

// ------------------------------------------------------------
// Full partitioning pipeline for one relation
// ------------------------------------------------------------
static PartitionedRelation partition_relation(const std::vector<Record>& rel,
                                              std::uint32_t P,
                                              const OmpTaskConfig& cfg) {
    const auto hist_result = compute_histogram(rel, P, cfg);
    const auto begin = exclusive_prefix_sum(hist_result.hist);
    auto data = scatter_partitioned(rel, P, begin, hist_result, cfg);

    std::vector<std::size_t> end(P, 0);
    for (std::uint32_t pid = 0; pid < P; ++pid) {
        end[pid] = begin[pid] + hist_result.hist[pid];
    }

    return PartitionedRelation{
        .data = std::move(data),
        .begin = begin,
        .end = end
    };
}

// ------------------------------------------------------------
// Join result
// ------------------------------------------------------------
struct JoinResult {
    std::uint64_t join_count = 0;
    std::uint64_t checksum1 = 0;
    std::uint64_t checksum2 = 0;
    double part_time_sec = 0.0;
    double join_time_sec = 0.0;
};

// ------------------------------------------------------------
// Local join on one partition
// ------------------------------------------------------------
static JoinResult join_one_partition(const PartitionedRelation& Rpart,
                                     const PartitionedRelation& Spart,
                                     std::uint32_t pid) {
    JoinResult result{};

    const std::size_t r_begin = Rpart.begin[pid];
    const std::size_t r_end = Rpart.end[pid];
    const std::size_t s_begin = Spart.begin[pid];
    const std::size_t s_end = Spart.end[pid];

    if (r_begin == r_end || s_begin == s_end) {
        return result;
    }

    std::unordered_map<std::uint64_t, std::uint32_t> countR;
    countR.reserve((r_end - r_begin) * 2);

    for (std::size_t i = r_begin; i < r_end; ++i) {
        ++countR[Rpart.data[i].key];
    }

    for (std::size_t i = s_begin; i < s_end; ++i) {
        const std::uint64_t key = Spart.data[i].key;
        const auto it = countR.find(key);
        if (it != countR.end()) {
            const std::uint64_t multiplicity = it->second;

            result.join_count += multiplicity;
            result.checksum1 += splitmix64(key) * multiplicity;
            result.checksum2 += splitmix64(key ^ 0x9e3779b97f4a7c15ULL) * multiplicity;
        }
    }

    return result;
}

// ------------------------------------------------------------
// Full task-based partitioned hash join
// ------------------------------------------------------------
static JoinResult partitioned_hash_join(const std::vector<Record>& R,
                                        const std::vector<Record>& S,
                                        std::uint32_t p,
                                        const OmpTaskConfig& cfg) {
    JoinResult result{};

    // Phase 1: partition both relations. They are processed one after the other
    // to avoid nested parallel regions and to keep benchmarking interpretable.
    double t0 = get_time();
    const PartitionedRelation Rpart = partition_relation(R, p, cfg);
    const PartitionedRelation Spart = partition_relation(S, p, cfg);
    double t1 = get_time();
    result.part_time_sec = t1 - t0;

    // Phase 2 + 3: one taskloop over partitions. Each task writes only the
    // result slot of its own partition; the final accumulation is sequential to
    // avoid atomics/critical sections in the measured join work.
    t0 = get_time();
    std::vector<JoinResult> partial(static_cast<std::size_t>(p));
    const int join_grain = cfg.join_task_grain;

    #pragma omp parallel num_threads(cfg.join_threads) default(none) shared(partial, Rpart, Spart) firstprivate(p, join_grain)
    {
        #pragma omp single nowait
        {
            #pragma omp taskloop grainsize(join_grain)
            for (std::int64_t pid_signed = 0; pid_signed < static_cast<std::int64_t>(p); ++pid_signed) {
                const std::uint32_t pid = static_cast<std::uint32_t>(pid_signed);
                partial[pid] = join_one_partition(Rpart, Spart, pid);
            }
        }
    }

    for (std::uint32_t pid = 0; pid < p; ++pid) {
        result.join_count += partial[pid].join_count;
        result.checksum1 += partial[pid].checksum1;
        result.checksum2 += partial[pid].checksum2;
    }

    t1 = get_time();
    result.join_time_sec = t1 - t0;

    return result;
}

// ------------------------------------------------------------
// Verifier for very small inputs
// ------------------------------------------------------------
static JoinResult naive_join_verifier(const std::vector<Record>& R,
                                      const std::vector<Record>& S) {
    JoinResult result{};

    for (const auto& r : R) {
        for (const auto& s : S) {
            if (r.key == s.key) {
                result.join_count += 1;
                result.checksum1 += splitmix64(r.key);
                result.checksum2 += splitmix64(r.key ^ 0x9e3779b97f4a7c15ULL);
            }
        }
    }
    return result;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main(int argc, char** argv) {
    std::uint64_t nr = 0, ns = 0, seed = 0, max_key = 0, p = 0;
    std::uint64_t part_threads_u64 = 0;
    std::uint64_t join_threads_u64 = 0;
    std::uint64_t partition_block_size_u64 = 0;
    std::uint64_t partition_chunk_u64 = 0;
    std::uint64_t join_chunk_u64 = 0;
    std::uint64_t partition_task_grain_u64 = 0;
    std::uint64_t join_task_grain_u64 = 0;
    std::uint64_t offset_task_grain_u64 = 0;
    std::string dataset_type;
    std::string partition_schedule_name;
    std::string join_schedule_name;

    DatasetConfig dataset_cfg{};

    if (!read_arg_u64(argc, argv, {"-nr"}, nr) ||
        !read_arg_u64(argc, argv, {"-ns"}, ns) ||
        !read_arg_u64(argc, argv, {"-seed"}, seed) ||
        !read_arg_u64(argc, argv, {"-max-key"}, max_key) ||
        !read_arg_u64(argc, argv, {"-p"}, p) ||
        !read_arg_string(argc, argv, {"--dataset-type", "-dataset-type"}, dataset_type) ||
        !read_arg_u64(argc, argv, {"--partition-threads", "-partition-threads"}, part_threads_u64) ||
        !read_arg_u64(argc, argv, {"--join-threads", "-join-threads"}, join_threads_u64) ||
        !read_arg_string(argc, argv, {"--partition-schedule", "-partition-schedule"}, partition_schedule_name) ||
        !read_arg_string(argc, argv, {"--join-schedule", "-join-schedule"}, join_schedule_name) ||
        !read_arg_u64(argc, argv, {"--partition-chunk", "-partition-chunk"}, partition_chunk_u64) ||
        !read_arg_u64(argc, argv, {"--join-chunk", "-join-chunk"}, join_chunk_u64) ||
        !read_arg_u64(argc, argv, {"--partition-block-size", "-partition-block-size"}, partition_block_size_u64) ||
        !read_arg_u64(argc, argv, {"--partition-task-grain", "-partition-task-grain"}, partition_task_grain_u64) ||
        !read_arg_u64(argc, argv, {"--join-task-grain", "-join-task-grain"}, join_task_grain_u64) ||
        !read_arg_u64(argc, argv, {"--offset-task-grain", "-offset-task-grain"}, offset_task_grain_u64)) {
        usage(argv[0]);
        return 1;
    }

    if (!parse_dataset_type(dataset_type, dataset_cfg)) {
        return 1;
    }
    if (!validate_dataset_config(dataset_cfg)) {
        return 1;
    }
    if (p > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "Error: P too large.\n";
        return 1;
    }
    if (partition_block_size_u64 == 0) {
        std::cerr << "Error: partition block size must be greater than zero.\n";
        return 1;
    }
    if (partition_task_grain_u64 == 0) {
        std::cerr << "Error: partition task grain must be greater than zero.\n";
        return 1;
    }
    if (join_task_grain_u64 == 0) {
        std::cerr << "Error: join task grain must be greater than zero.\n";
        return 1;
    }
    if (offset_task_grain_u64 == 0) {
        std::cerr << "Error: offset task grain must be greater than zero.\n";
        return 1;
    }

    OmpTaskConfig cfg{};
    try {
        cfg.partition_threads = checked_positive_int(part_threads_u64, "partition thread count");
        cfg.join_threads = checked_positive_int(join_threads_u64, "join thread count");
        cfg.partition_chunk = checked_nonnegative_int(partition_chunk_u64, "partition chunk size");
        cfg.join_chunk = checked_nonnegative_int(join_chunk_u64, "join chunk size");
        cfg.partition_task_grain = checked_positive_int(partition_task_grain_u64, "partition task grain");
        cfg.join_task_grain = checked_positive_int(join_task_grain_u64, "join task grain");
        cfg.offset_task_grain = checked_positive_int(offset_task_grain_u64, "offset task grain");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    cfg.partition_block_size = static_cast<std::size_t>(partition_block_size_u64);
    cfg.partition_schedule_name = partition_schedule_name;
    cfg.join_schedule_name = join_schedule_name;

    // Keep benchmark runs controlled. Affinity can still be set externally with
    // OMP_PROC_BIND and OMP_PLACES.
    omp_set_dynamic(0);

    const std::uint32_t P = static_cast<std::uint32_t>(p);

    if (!is_power_of_two(P)) {
        std::cerr << "Error: in this reference implementation, P must be a power of two.\n";
        return 1;
    }

    const std::size_t NR = static_cast<std::size_t>(nr);
    const std::size_t NS = static_cast<std::size_t>(ns);

    const auto R = generate_relation(NR, seed, max_key, P, dataset_cfg);
    const auto S = generate_relation(NS, seed ^ 0xdeadebdecdeedef1ULL, max_key, P, dataset_cfg);

    double t0 = get_time();
    const JoinResult result = partitioned_hash_join(R, S, P, cfg);
    double t1 = get_time();
    const double tot_time_sec = t1 - t0;

    std::cout << "dataset-type=" << dataset_cfg.type << "\n";
    std::cout << "join_count=" << result.join_count << "\n";
    std::cout << "checksum1=" << result.checksum1 << "\n";
    std::cout << "checksum2=" << result.checksum2 << "\n";

    if (NR <= 500 && NS <= 500) {
        const JoinResult naive = naive_join_verifier(R, S);
        std::cout << "naive_join_count=" << naive.join_count << "\n";
        std::cout << "naive_checksum1=" << naive.checksum1 << "\n";
        std::cout << "naive_checksum2=" << naive.checksum2 << "\n";
    }

    const std::uint64_t total_elements = NR + NS;
    const double part_throughput = compute_throughput(total_elements, result.part_time_sec);
    const double join_throughput = compute_throughput(total_elements, result.join_time_sec);
    const double total_throughput = compute_throughput(total_elements, tot_time_sec);

    const ResultMap results_map = {
        {"checksum1", std::to_string(result.checksum1)},
        {"checksum2", std::to_string(result.checksum2)},
        {"join_count", std::to_string(result.join_count)},
        {"join_throughput", std::to_string(join_throughput)},
        {"total_throughput", std::to_string(total_throughput)},
        {"partition_time", std::to_string(result.part_time_sec)},
        {"partition_throughput", std::to_string(part_throughput)},
        {"join_time", std::to_string(result.join_time_sec)},
        {"partition_threads", std::to_string(cfg.partition_threads)},
        {"join_threads", std::to_string(cfg.join_threads)},
        {"partition_schedule", cfg.partition_schedule_name},
        {"join_schedule", cfg.join_schedule_name},
        {"partition_chunk", std::to_string(cfg.partition_chunk)},
        {"join_chunk", std::to_string(cfg.join_chunk)},
        {"partition_task_grain", std::to_string(cfg.partition_task_grain)},
        {"join_task_grain", std::to_string(cfg.join_task_grain)},
        {"offset_task_grain", std::to_string(cfg.offset_task_grain)},
        {"partition_block_size", std::to_string(cfg.partition_block_size)},
        {"dataset_type", dataset_cfg.type},
        {"dataset", dataset_cfg.name},
        {"skew_fraction", std::to_string(dataset_cfg.skew_fraction)},
        {"skew_partition_fraction", std::to_string(dataset_cfg.skew_partition_fraction)},
        {"max_key", std::to_string(max_key)},
        {"nr", std::to_string(NR)},
        {"ns", std::to_string(NS)},
        {"time_sec", std::to_string(tot_time_sec)}
    };

    const std::string filepath = "results/" + std::filesystem::path(argv[0]).stem().string() + ".csv";
    append_to_csv(filepath, results_map);

    return 0;
}
