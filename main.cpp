#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "aco/aco.hpp"
#include "genetic/genetic.hpp"
#include "sa/sim_an.hpp"

constexpr std::uint32_t ACO_SEED_ID = 0xA0C0;
constexpr std::uint32_t GA_SEED_ID = 0x6A;
constexpr std::uint32_t SA_SEED_ID = 0x5A;
constexpr int BUDGET_BENCHMARK_REPEATS = 30;
constexpr int FULL_BENCHMARK_REPEATS = 5;
constexpr std::size_t BUDGET_EVALUATION_BUDGET = 100000;
const std::filesystem::path RESULTS_DIR = "results";

#ifndef TSP_PROJECT_ROOT
#define TSP_PROJECT_ROOT "."
#endif

enum class BenchmarkMode {
    Budget,
    Full
};

struct MainConfig {
    std::uint32_t base_seed = DEFAULT_RANDOM_SEED;
    BenchmarkMode mode = BenchmarkMode::Budget;
};

struct BenchmarkStats {
    double min_cost = std::numeric_limits<double>::max();
    double avg_cost = 0.0;
    double stddev_cost = 0.0;
    double avg_time = 0.0;
};

std::size_t budgeted_iterations(std::size_t evaluation_budget, std::size_t work_per_iteration) {
    if (evaluation_budget == 0 || work_per_iteration == 0) {
        throw std::invalid_argument("evaluation budget and work per iteration must be positive");
    }

    return std::max<std::size_t>(1, evaluation_budget / work_per_iteration);
}

std::filesystem::path project_root() {
    return std::filesystem::path(TSP_PROJECT_ROOT);
}

std::string tsplib_test_file(const std::string& filename) {
    return (project_root() / "tsplib" / "tests" / filename).string();
}

double cooling_rate_for_budget(double start_temp, double end_temp, std::size_t steps) {
    if (steps == 0) {
        throw std::invalid_argument("SA steps must be positive");
    }
    if (!std::isfinite(start_temp) || !std::isfinite(end_temp) || start_temp <= 0.0 || end_temp <= 0.0 || end_temp >= start_temp) {
        throw std::invalid_argument("SA temperatures must be finite, positive, and decreasing");
    }

    return std::exp(std::log(end_temp / start_temp) / static_cast<double>(steps));
}

std::ofstream open_results_file(const std::string& filename) {
    std::filesystem::create_directories(RESULTS_DIR);
    const auto output_path = RESULTS_DIR / filename;

    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open results file: " + output_path.string());
    }

    return out;
}

std::string dataset_filename(const std::string& file) {
    return std::filesystem::path(file).filename().string();
}

std::uint32_t parse_seed_text(const std::string& seed_text) {
    std::size_t parsed_chars = 0;
    unsigned long seed_value = 0;
    try {
        seed_value = std::stoul(seed_text, &parsed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument("Seed must be an unsigned 32-bit integer.");
    }

    if (parsed_chars != seed_text.size() || seed_value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Seed must be an unsigned 32-bit integer.");
    }

    return static_cast<std::uint32_t>(seed_value);
}

BenchmarkMode parse_mode_text(const std::string& mode_text) {
    if (mode_text == "budget") {
        return BenchmarkMode::Budget;
    }
    if (mode_text == "full") {
        return BenchmarkMode::Full;
    }

    throw std::invalid_argument("Mode must be either budget or full.");
}

std::string mode_name(BenchmarkMode mode) {
    return mode == BenchmarkMode::Budget ? "budget" : "full";
}

MainConfig parse_arguments(int argc, char* argv[]) {
    if (argc > 3) {
        throw std::invalid_argument("Usage: tsp_optimization [seed] [budget|full]");
    }

    MainConfig config;
    if (argc >= 2) {
        config.base_seed = parse_seed_text(argv[1]);
    }
    if (argc >= 3) {
        config.mode = parse_mode_text(argv[2]);
    }

    return config;
}

template <typename Solver>
BenchmarkStats run_repeated_benchmark(const std::string& file, std::size_t dataset_index, std::uint32_t base_seed, std::uint32_t algorithm_id, int repeats, Solver solver) {
    BenchmarkStats stats;
    std::vector<double> costs;
    costs.reserve(static_cast<std::size_t>(repeats));
    double cost_sum = 0.0;
    double time_sum = 0.0;

    for (int repeat = 0; repeat < repeats; ++repeat) {
        set_random_seed(derive_run_seed(base_seed, algorithm_id, dataset_index, static_cast<std::size_t>(repeat)));

        std::vector<City> cities;
        readfile(cities, file);

        const auto start = std::chrono::high_resolution_clock::now();
        solver(cities);
        const auto end = std::chrono::high_resolution_clock::now();

        const double cost = total_cost(cities);
        stats.min_cost = std::min(stats.min_cost, cost);
        costs.push_back(cost);
        cost_sum += cost;
        time_sum += std::chrono::duration<double>(end - start).count();
    }

    stats.avg_cost = cost_sum / repeats;
    stats.avg_time = time_sum / repeats;

    double variance_sum = 0.0;
    for (double cost: costs) {
        const double diff = cost - stats.avg_cost;
        variance_sum += diff * diff;
    }
    stats.stddev_cost = std::sqrt(variance_sum / repeats);

    return stats;
}

template <typename Solver, typename RowPrefix, typename LogResult>
void run_benchmark(const std::string& filename,
                   const std::vector<std::string>& datasets,
                   const std::string& csv_header,
                   std::uint32_t algorithm_id,
                   std::uint32_t base_seed,
                   int repeats,
                   Solver solver,
                   RowPrefix row_prefix,
                   LogResult log_result) {
    std::ofstream out = open_results_file(filename);
    out << csv_header << "\n";

    for (std::size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = dataset_filename(file);
        const auto stats = run_repeated_benchmark(file, dataset_index, base_seed, algorithm_id, repeats, solver);
        const std::string prefix = row_prefix(name);

        out << prefix << ",avg," << stats.avg_cost << "," << stats.avg_time << "\n";
        out << prefix << ",min," << stats.min_cost << "," << stats.avg_time << "\n";
        out << prefix << ",stddev," << stats.stddev_cost << "," << stats.avg_time << "\n";

        log_result(name, stats);
    }
}

void run_tests_aco(const std::string& filename,
                   const std::vector<std::string>& datasets,
                   std::size_t ants,
                   double alpha,
                   double beta,
                   double evap,
                   std::size_t evaluation_budget,
                   std::uint32_t base_seed,
                   int repeats,
                   BenchmarkMode mode,
                   bool use_two_opt) {
    const std::size_t epochs = budgeted_iterations(evaluation_budget, ants);

    auto row_prefix = [=](const std::string& name) {
        std::ostringstream row;
        row << "ACO," << mode_name(mode) << "," << name << "," << base_seed << "," << repeats << "," << ants << "," << alpha << "," << beta << "," << evap
            << "," << use_two_opt << "," << evaluation_budget << "," << epochs;
        return row.str();
    };

    auto log_result = [=](const std::string& name, const BenchmarkStats& stats) {
        std::cout << "[ACO] " << name << " ants=" << ants
                  << " alpha=" << alpha << " beta=" << beta << " evap=" << evap << " seed=" << base_seed
                  << " repeats=" << repeats << " mode=" << mode_name(mode) << " budget=" << evaluation_budget << " epochs=" << epochs
                  << " -> cost=" << stats.avg_cost << ", time=" << stats.avg_time << "s\n";
    };

    run_benchmark(
        filename,
        datasets,
        "algorithm,mode,dataset,base_seed,repeats,ants,alpha,beta,evaporation,two_opt,evaluation_budget,epochs,stat,cost,avg_time_sec",
        ACO_SEED_ID,
        base_seed,
        repeats,
        [=](std::vector<City>& cities) { aco(cities, ants, alpha, beta, evap, epochs, use_two_opt); },
        row_prefix,
        log_result);
}

void run_tests_genetic(const std::string& filename,
                       const std::vector<std::string>& datasets,
                       double mutation_rate,
                       std::size_t population_size,
                       std::size_t evaluation_budget,
                       std::uint32_t base_seed,
                       int repeats,
                       BenchmarkMode mode,
                       bool use_two_opt) {
    const std::size_t epochs = budgeted_iterations(evaluation_budget, population_size);

    auto row_prefix = [=](const std::string& name) {
        std::ostringstream row;
        row << "GA," << mode_name(mode) << "," << name << "," << base_seed << "," << repeats << "," << mutation_rate << "," << population_size
            << "," << use_two_opt << "," << evaluation_budget << "," << epochs;
        return row.str();
    };

    auto log_result = [=](const std::string& name, const BenchmarkStats& stats) {
        std::cout << "[GA] " << name << " pop=" << population_size << " mut=" << mutation_rate
                  << " seed=" << base_seed << " repeats=" << repeats << " mode=" << mode_name(mode) << " budget=" << evaluation_budget << " epochs=" << epochs
                  << " -> cost=" << stats.avg_cost << ", time=" << stats.avg_time << "s\n";
    };

    run_benchmark(
        filename,
        datasets,
        "algorithm,mode,dataset,base_seed,repeats,mutation,population,two_opt,evaluation_budget,epochs,stat,cost,avg_time_sec",
        GA_SEED_ID,
        base_seed,
        repeats,
        [=](std::vector<City>& cities) { genetic_optimization(cities, mutation_rate, population_size, epochs, use_two_opt); },
        row_prefix,
        log_result);
}

void run_tests_sa(const std::string& filename,
                  const std::vector<std::string>& datasets,
                  double temp,
                  double end_temp,
                  double cool_rate,
                  std::size_t evaluation_budget,
                  std::uint32_t base_seed,
                  int repeats,
                  BenchmarkMode mode) {
    const std::size_t epochs = evaluation_budget;

    auto row_prefix = [=](const std::string& name) {
        std::ostringstream row;
        row << "SA," << mode_name(mode) << "," << name << "," << base_seed << "," << repeats << "," << temp << "," << end_temp << "," << cool_rate
            << "," << evaluation_budget << "," << epochs;
        return row.str();
    };

    auto log_result = [=](const std::string& name, const BenchmarkStats& stats) {
        std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                  << " seed=" << base_seed << " repeats=" << repeats << " mode=" << mode_name(mode) << " budget=" << evaluation_budget << " epochs=" << epochs
                  << " -> cost=" << stats.avg_cost << ", time=" << stats.avg_time << "s\n";
    };

    run_benchmark(
        filename,
        datasets,
        "algorithm,mode,dataset,base_seed,repeats,temperature,end_temperature,cooling,evaluation_budget,epochs,stat,cost,avg_time_sec",
        SA_SEED_ID,
        base_seed,
        repeats,
        [=](std::vector<City>& cities) { sa_optimization(cities, temp, end_temp, cool_rate, epochs); },
        row_prefix,
        log_result);
}

std::vector<std::string> benchmark_datasets() {
    return {
        tsplib_test_file("eil51.tsp"),
        tsplib_test_file("berlin52.tsp"),
        tsplib_test_file("st70.tsp"),
        tsplib_test_file("eil76.tsp"),
        tsplib_test_file("kroA100.tsp"),
        tsplib_test_file("kroB100.tsp")
    };
}

void run_budget_benchmarks(const std::vector<std::string>& datasets, std::uint32_t base_seed) {
    const double sa_start_temp = 10000.0;
    const double sa_end_temp = 1e-3;
    const double sa_cooling = cooling_rate_for_budget(sa_start_temp, sa_end_temp, BUDGET_EVALUATION_BUDGET);

    run_tests_aco("aco_results_budget", datasets, 20, 1, 5, 0.5, BUDGET_EVALUATION_BUDGET, base_seed, BUDGET_BENCHMARK_REPEATS, BenchmarkMode::Budget, false);
    run_tests_sa("sa_results_budget", datasets, sa_start_temp, sa_end_temp, sa_cooling, BUDGET_EVALUATION_BUDGET, base_seed, BUDGET_BENCHMARK_REPEATS, BenchmarkMode::Budget);
    run_tests_genetic("ga_results_budget", datasets, 0.1, 100, BUDGET_EVALUATION_BUDGET, base_seed, BUDGET_BENCHMARK_REPEATS, BenchmarkMode::Budget, false);
}

void run_full_benchmarks(const std::vector<std::string>& datasets, std::uint32_t base_seed) {
    constexpr std::size_t full_sa_steps = 300000;
    constexpr std::size_t full_aco_ants = 24;
    constexpr std::size_t full_aco_epochs = 800;
    constexpr std::size_t full_ga_population = 120;
    constexpr std::size_t full_ga_generations = 500;

    const double sa_start_temp = 10000.0;
    const double sa_end_temp = 1e-3;
    const double sa_cooling = cooling_rate_for_budget(sa_start_temp, sa_end_temp, full_sa_steps);

    run_tests_aco("aco_results_full", datasets, full_aco_ants, 1.3, 5.0, 0.45, full_aco_ants * full_aco_epochs, base_seed, FULL_BENCHMARK_REPEATS, BenchmarkMode::Full, true);
    run_tests_sa("sa_results_full", datasets, sa_start_temp, sa_end_temp, sa_cooling, full_sa_steps, base_seed, FULL_BENCHMARK_REPEATS, BenchmarkMode::Full);
    run_tests_genetic("ga_results_full", datasets, 0.12, full_ga_population, full_ga_population * full_ga_generations, base_seed, FULL_BENCHMARK_REPEATS, BenchmarkMode::Full, true);
}

int main(int argc, char* argv[]) {
    MainConfig config;
    try {
        config = parse_arguments(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    std::cout << "Using base seed: " << config.base_seed << "\n";
    std::cout << "Using benchmark mode: " << mode_name(config.mode) << "\n";

    const auto datasets = benchmark_datasets();

    if (config.mode == BenchmarkMode::Budget) {
        run_budget_benchmarks(datasets, config.base_seed);
    } else {
        run_full_benchmarks(datasets, config.base_seed);
    }
}
