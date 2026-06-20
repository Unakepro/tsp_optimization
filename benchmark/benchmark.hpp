#ifndef TSP_BENCHMARK_BENCHMARK
#define TSP_BENCHMARK_BENCHMARK

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

struct BenchmarkConfig {
    std::string benchmark_mode = "timed";
    std::string group = "small";
    std::string algorithm = "all";
    std::string params = "default";
    std::filesystem::path custom_config;
    std::string label;
    std::optional<bool> two_opt_override;

    double time_limit = 5.0;
    std::uint32_t seed = 42;
    int repeats = 3;

    std::size_t min_iters = 50;
    std::size_t stable_window = 25;
    double improvement_eps = 1e-4;
    double plateau_seconds = 60.0;
    std::size_t max_iters = 100000;
};

void run_benchmark(const BenchmarkConfig& config);

#endif
