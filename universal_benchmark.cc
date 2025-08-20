/* Benchmarks a mix of operations for a compile-time specified key-value pair */

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <pcg/pcg_random.hpp>
#include <test_util.hh>

#include "universal_gen.hh"
#include "universal_table_wrapper.hh"
#include <numa.h>
#include <sched.h>
#include <pthread.h>

/* Run-time parameters -- operation mix and table configuration */

// The following specify what percentage of operations should be of each type.
// They must add up to 100, but by default are all 0.
size_t g_read_percentage = 0;
size_t g_insert_percentage = 0;
size_t g_erase_percentage = 0;
size_t g_update_percentage = 0;
size_t g_upsert_percentage = 0;
size_t g_num_numa_nodes = 0;

// The initial capacity of the table, specified as a power of 2.
size_t g_initial_capacity = 25;
// The percentage of the initial table capacity should we fill the table to
// before running the benchmark.
size_t g_prefill_percentage = 0;
// Total number of operations we are running, specified as a percentage of the
// initial capacity. This can exceed 100.
size_t g_total_ops_percentage = 75;

// Number of threads to run with
size_t g_threads = std::thread::hardware_concurrency();

// Seed for random number generator. If left at the default (0), we'll generate
// a random seed.
size_t g_seed = 0;

const char *args[] = {
    "--reads",   "--inserts",   "--erases",
    "--updates", "--upserts",   "--initial-capacity",
    "--prefill", "--total-ops", "--num-threads",
    "--seed", "--numa-nodes",
};

size_t *arg_vars[] = {
    &g_read_percentage,
    &g_insert_percentage,
    &g_erase_percentage,
    &g_update_percentage,
    &g_upsert_percentage,
    &g_initial_capacity,
    &g_prefill_percentage,
    &g_total_ops_percentage,
    &g_threads,
    &g_seed,
    &g_num_numa_nodes,
};

const char *arg_descriptions[] = {
    "Percentage of mix that is reads", "Percentage of mix that is inserts",
    "Percentage of mix that is erases", "Percentage of mix that is updates",
    "Percentage of mix that is upserts",
    "Initial capacity of table, as a power of 2",
    "Percentage of final size to pre-fill table",
    "Number of operations, as a percentage of the initial capacity. This can "
    "exceed 100",
    "Number of threads", "Seed for random number generator",
    "Number of NUMA nodes / table shards to split the workload across",
};

#define XSTR(s) STR(s)
#define STR(s) #s

const char *description =
    "A benchmark that can run an arbitrary mixture of "
    "table operations.\nThe sum of read, insert, erase, update, and upsert "
    "percentages must be 100.\nMap type is " TABLE_TYPE
    "<" XSTR(KEY) ", " XSTR(VALUE) ">.";

void check_percentage(size_t value, const char *name) {
  if (value > 100) {
    std::string msg("Percentage for `");
    msg += name;
    msg += "` cannot exceed 100\n";
    throw std::runtime_error(msg.c_str());
  }
}

enum Ops {
  READ,
  INSERT,
  ERASE,
  UPDATE,
  UPSERT,
};

void gen_nums(std::vector<uint64_t> &nums, pcg64_oneseq_once_insecure &rng) {
  for (uint64_t &num : nums) {
    num = rng();
  }
}

void gen_keys(std::vector<uint64_t> &nums,
              std::vector<Gen<KEY>::storage_type> &keys) {
  const size_t n = nums.size();
  for (size_t i = 0; i < n; ++i) {
    keys[i] = Gen<KEY>::storage_key(nums[i]);
  }
}

void prefill(Table &tbl, const std::vector<Gen<KEY>::storage_type> &keys,
             const size_t prefill_elems) {
  Gen<VALUE>::storage_type local_value = Gen<VALUE>::storage_value();
  for (size_t i = 0; i < prefill_elems; ++i) {
    ASSERT_TRUE(
        tbl.insert(Gen<KEY>::get(keys[i]), Gen<VALUE>::get(local_value)));
  }
}

void mix(Table &tbl, const size_t num_ops, const std::array<Ops, 100> &op_mix,
         const std::vector<Gen<KEY>::storage_type> &keys,
         const size_t prefill_elems, std::vector<size_t> &samples) {
  Sampler sampler(num_ops);
  Gen<VALUE>::storage_type local_value = Gen<VALUE>::storage_value();
  // Invariant: erase_seq <= insert_seq
  // Invariant: insert_seq < numkeys
  const size_t numkeys = keys.size();
  size_t erase_seq = 0;
  size_t insert_seq = prefill_elems;
  // These variables are initialized out here so we don't create new variables
  // in the switch statement.
  size_t n;
  VALUE v;
  // Convenience functions for getting the nth key and value
  auto key = [&keys](size_t n) {
    assert(n < keys.size());
    return Gen<KEY>::get(keys[n]);
  };
  // The upsert function is just the identity
  auto upsert_fn = [](VALUE &v) { return; };
  // Use an LCG over the keys array to iterate over the keys in a pseudorandom
  // order, for find operations
  assert(1UL << static_cast<size_t>(floor(log2(numkeys))) == numkeys);
  assert(numkeys > 4);
  size_t find_seq = 0;
  const size_t a = numkeys / 2 + 1;
  const size_t c = numkeys / 4 - 1;
  const size_t find_seq_mask = numkeys - 1;
  auto find_seq_update = [&find_seq, &a, &c, &find_seq_mask, &numkeys]() {
    find_seq = (a * find_seq + c) & find_seq_mask;
  };
  // Run the operation mix for num_ops operations
  for (size_t i = 0; i < num_ops;) {
    for (size_t j = 0; j < 100 && i < num_ops; ++i, ++j) {
      sampler.iter();
      switch (op_mix[j]) {
      case READ:
        // If `find_seq` is between `erase_seq` and `insert_seq`, then it
        // should be in the table.
        ASSERT_EQ(find_seq >= erase_seq && find_seq < insert_seq,
                  tbl.read(key(find_seq), v));
        find_seq_update();
        break;
      case INSERT:
        // Insert sequence number `insert_seq`. This should always
        // succeed and be inserting a new value.
        ASSERT_TRUE(tbl.insert(key(insert_seq), Gen<VALUE>::get(local_value)));
        ++insert_seq;
        break;
      case ERASE:
        // If `erase_seq` == `insert_seq`, the table should be empty, so
        // we pick a random index to unsuccessfully erase. Otherwise we
        // erase `erase_seq`.
        if (erase_seq == insert_seq) {
          ASSERT_TRUE(!tbl.erase(key(find_seq)));
          find_seq_update();
        } else {
          ASSERT_TRUE(tbl.erase(key(erase_seq++)));
        }
        break;
      case UPDATE:
        // Same as find, except we update to the same default value
        ASSERT_EQ(find_seq >= erase_seq && find_seq < insert_seq,
                  tbl.update(key(find_seq), Gen<VALUE>::get(local_value)));
        find_seq_update();
        break;
      case UPSERT:
        // Pick a number from the full distribution, but cap it to the
        // insert_seq, so we don't insert a number greater than
        // insert_seq.
        n = std::min(find_seq, insert_seq);
        find_seq_update();
        tbl.upsert(key(n), upsert_fn, Gen<VALUE>::get(local_value));
        if (n == insert_seq) {
          ++insert_seq;
        }
        break;
      }
    }
  }
  sampler.store(samples);
}

int main(int argc, char **argv) {
  try {
    // Parse parameters and check them.
    parse_flags(argc, argv, description, args, arg_vars, arg_descriptions,
                sizeof(args) / sizeof(const char *), nullptr, nullptr, nullptr,
                0);
    check_percentage(g_read_percentage, "reads");
    check_percentage(g_insert_percentage, "inserts");
    check_percentage(g_erase_percentage, "erases");
    check_percentage(g_update_percentage, "updates");
    check_percentage(g_upsert_percentage, "upserts");
    check_percentage(g_prefill_percentage, "prefill");
    if (g_read_percentage + g_insert_percentage + g_erase_percentage +
            g_update_percentage + g_upsert_percentage !=
        100) {
      throw std::runtime_error("Operation mix percentages must sum to 100\n");
    }
    if (g_seed == 0) {
      g_seed = std::random_device()();
    }

    // Detect number of NUMA nodes if not specified
        if (g_num_numa_nodes == 0) {
        if (numa_available() == -1) {
                throw std::runtime_error("NUMA is not available on this system.\n");
        }
        g_num_numa_nodes = numa_max_node() + 1;
        fprintf(stderr, "Detected %zu NUMA nodes\n", g_num_numa_nodes);
        }


    pcg64_oneseq_once_insecure base_rng(g_seed);

    const size_t initial_capacity = 1UL << g_initial_capacity;
    const size_t total_ops = initial_capacity * g_total_ops_percentage / 100;

    // Pre-generate an operation mix based on our percentages.
    std::array<Ops, 100> op_mix;
    auto *op_mix_p = &op_mix[0];
    for (size_t i = 0; i < g_read_percentage; ++i) {
      *op_mix_p++ = READ;
    }
    for (size_t i = 0; i < g_insert_percentage; ++i) {
      *op_mix_p++ = INSERT;
    }
    for (size_t i = 0; i < g_erase_percentage; ++i) {
      *op_mix_p++ = ERASE;
    }
    for (size_t i = 0; i < g_update_percentage; ++i) {
      *op_mix_p++ = UPDATE;
    }
    for (size_t i = 0; i < g_upsert_percentage; ++i) {
      *op_mix_p++ = UPSERT;
    }
    std::shuffle(op_mix.begin(), op_mix.end(), base_rng);

    // Pre-generate all the keys we'd want to insert. In case the insert +
    // upsert percentage is too low, lower bound by the table capacity.
    std::cerr << "Generating keys\n";
    const size_t prefill_elems = initial_capacity * g_prefill_percentage / 100;
    // We won't be running through `op_mix` more than ceil(total_ops / 100),
    // so calculate that ceiling and multiply by the number of inserts and
    // upserts to get an upper bound on how many elements we'll be
    // inserting.
    const size_t max_insert_ops =
        (total_ops + 99) / 100 * (g_insert_percentage + g_upsert_percentage);
    const size_t insert_keys =
        std::max(initial_capacity, max_insert_ops) + prefill_elems;
    // Round this quantity up to a power of 2, so that we can use an LCG to
    // cycle over the array "randomly".
    const size_t insert_keys_per_thread =
        1UL << static_cast<size_t>(
            ceil(log2((insert_keys + g_threads - 1) / g_threads)));
    // Can't do this in parallel, because the random number generator is
    // single-threaded.
        // === New: partition keys across NUMA nodes ===
        const size_t total_keys = insert_keys_per_thread * g_threads;

        // Step 1: generate all random numbers in one big array
        std::vector<uint64_t> all_nums(total_keys);
        gen_nums(all_nums, base_rng);

        // Step 2: convert them to keys
        std::vector<Gen<KEY>::storage_type> all_keys(total_keys);
        gen_keys(all_nums, all_keys);

        // Step 3: partition keys across NUMA nodes using hash % num_nodes
        std::vector<std::vector<Gen<KEY>::storage_type>> keys_per_node(g_num_numa_nodes);
        for (size_t i = 0; i < total_keys; ++i) {
                size_t node = std::hash<Gen<KEY>::storage_type>{}(all_keys[i]) % g_num_numa_nodes;
                keys_per_node[node].push_back(all_keys[i]);
        }

    for (size_t i = 0; i < g_num_numa_nodes; ++i) {
        fprintf(stderr, "NUMA node %zu has %zu keys\n", i, keys_per_node[i].size());
    }

    // Create one table per NUMA node

    std::vector<Table> shards;
    shards.reserve(g_num_numa_nodes);
    for (size_t i = 0; i < g_num_numa_nodes; ++i) {
        shards.emplace_back(initial_capacity);
    }

        std::cerr << "Pre-filling table shards\n";
        std::vector<std::thread> prefill_threads;

        for (size_t node = 0; node < g_num_numa_nodes; ++node) {
        const size_t num_keys = keys_per_node[node].size();
        const size_t threads_per_node = g_threads / g_num_numa_nodes;
        const size_t keys_per_thread = num_keys / threads_per_node;

        for (size_t t = 0; t < threads_per_node; ++t) {
                prefill_threads.emplace_back([&, node, t]() {
                // Pin thread to CPUs on its NUMA node
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
                    if (numa_node_of_cpu(cpu) == (int)node) {
                    CPU_SET(cpu, &cpuset);
                    }
                }
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

                int cpu = sched_getcpu();
                fprintf(stderr, "[prefill n%zu t%zu] cpu=%d\n", node, t, cpu);

                size_t begin = t * keys_per_thread;
                size_t end = (t == threads_per_node - 1) ? num_keys : (t + 1) * keys_per_thread;

                std::vector<Gen<KEY>::storage_type> local_keys(
                        keys_per_node[node].begin() + begin,
                        keys_per_node[node].begin() + end);

                prefill(shards[node], local_keys, local_keys.size());
                });
        }
        }

for (auto &t : prefill_threads) { t.join(); }

    // Run the operation mix, timed
    std::vector<std::thread> mix_threads;
    std::vector<std::vector<size_t>> samples;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::cerr << "Running operations\n";

    for (size_t node = 0; node < g_num_numa_nodes; ++node) {
        const size_t num_keys = keys_per_node[node].size();
        const size_t threads_per_node = g_threads / g_num_numa_nodes;
        const size_t keys_per_thread = num_keys / threads_per_node;
        const size_t ops_per_thread = total_ops / g_threads;

        for (size_t t = 0; t < threads_per_node; ++t) {
            mix_threads.emplace_back([&, node, t]() {

                // Pin thread to CPUs on its NUMA node
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
                    if (numa_node_of_cpu(cpu) == (int)node) {
                        CPU_SET(cpu, &cpuset);
                    }
                }
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

                int cpu = sched_getcpu();
                fprintf(stderr, "[mix    n%zu t%zu] cpu=%d\n", node, t, cpu);

                size_t begin = t * keys_per_thread;
                size_t end = (t == threads_per_node - 1) ? num_keys : (t + 1) * keys_per_thread;

                std::vector<Gen<KEY>::storage_type> local_keys(
                    keys_per_node[node].begin() + begin,
                    keys_per_node[node].begin() + end);

                samples.emplace_back();
                mix(shards[node], ops_per_thread, op_mix, local_keys, local_keys.size(), samples.back());
            });
        }
    }

    for (auto &t : mix_threads) { t.join(); }

    auto end_time = std::chrono::high_resolution_clock::now();
    double seconds_elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(end_time -
                                                                  start_time)
            .count();
    // Print out args, preprocessor constants, and results in JSON format
    std::stringstream argstr;
    argstr << args[0] << " " << *arg_vars[0];
    for (size_t i = 1; i < sizeof(args) / sizeof(args[0]); ++i) {
      argstr << " " << args[i] << " " << *arg_vars[i];
    }
    // Average together the allocator samples from each thread. If
    // TRACKING_ALLOCATOR is turned off, the samples should all be empty,
    // and this list should end up empty.
    std::stringstream samplestr;
    samplestr << "[";
    const size_t total_samples = samples[0].size();
    for (size_t i = 0; i < total_samples; ++i) {
      size_t total = 0;
      for (size_t j = 0; j < g_threads; ++j) {
        total += samples.at(j).at(i);
      }
      size_t avg = total / g_threads;
      samplestr << avg;
      if (i < total_samples - 1) {
        samplestr << ",";
      }
    }
    samplestr << "]";
    const char *json_format = R"({
    "args": "%s",
    "key": "%s",
    "key_size": "%zu",
    "value": "%s",
    "value_size": "%zu",
    "NUMA_table": "%s",
    "output": {
        "total_ops": {
            "name": "Total Operations",
            "units": "count",
            "value": %zu
        },
        "time_elapsed": {
            "name": "Time Elapsed",
            "units": "seconds",
            "value": %.4f
        },
        "throughput": {
            "name": "Throughput",
            "units": "count/seconds",
            "value": %.4f
        },
        "memory_samples": {
            "name": "Memory Samples",
            "units": "[bytes]",
            "value": %s
        }
    }
}
)";
    printf(json_format, argstr.str().c_str(), XSTR(KEY), Gen<KEY>::key_size,
           XSTR(VALUE), Gen<VALUE>::value_size, TABLE, total_ops,
           seconds_elapsed, total_ops / seconds_elapsed,
           samplestr.str().c_str());
  } catch (const std::exception &e) {
    std::cerr << e.what();
    std::exit(1);
  }
}
