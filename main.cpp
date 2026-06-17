#include <algorithm>
#include <chrono>
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
constexpr int BENCHMARK_REPEATS = 30;
constexpr std::size_t BENCHMARK_EVALUATION_BUDGET = 100000;
const std::filesystem::path RESULTS_DIR = "results";

struct BenchmarkStats {
    double min_cost = std::numeric_limits<double>::max();
    double avg_cost = 0.0;
    double avg_time = 0.0;
};

std::size_t budgeted_iterations(std::size_t evaluation_budget, std::size_t work_per_iteration) {
    if (evaluation_budget == 0 || work_per_iteration == 0) {
        throw std::invalid_argument("evaluation budget and work per iteration must be positive");
    }

    return std::max<std::size_t>(1, evaluation_budget / work_per_iteration);
}

std::size_t scheduled_sa_iterations(double start_temp, double end_temp, double alpha, std::size_t max_steps) {
    if (max_steps == 0) {
        throw std::invalid_argument("max steps must be positive");
    }

    std::size_t iterations = 0;
    double temperature = start_temp;
    while (iterations < max_steps) {
        ++iterations;
        temperature *= alpha;
        if (temperature <= end_temp) {
            break;
        }
    }

    return iterations;
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

std::uint32_t parse_seed(int argc, char* argv[]) {
    if (argc <= 1) {
        return DEFAULT_RANDOM_SEED;
    }
    if (argc > 2) {
        throw std::invalid_argument("Usage: tsp_optimization [seed]");
    }

    std::string seed_text = argv[1];
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

template <typename Solver>
BenchmarkStats run_repeated_benchmark(const std::string& file, std::size_t dataset_index, std::uint32_t base_seed, std::uint32_t algorithm_id, Solver solver) {
    BenchmarkStats stats;
    double cost_sum = 0.0;
    double time_sum = 0.0;

    for (int repeat = 0; repeat < BENCHMARK_REPEATS; ++repeat) {
        set_random_seed(derive_run_seed(base_seed, algorithm_id, dataset_index, static_cast<std::size_t>(repeat)));

        std::vector<City> cities;
        readfile(cities, file);

        const auto start = std::chrono::high_resolution_clock::now();
        solver(cities);
        const auto end = std::chrono::high_resolution_clock::now();

        const double cost = total_cost(cities);
        stats.min_cost = std::min(stats.min_cost, cost);
        cost_sum += cost;
        time_sum += std::chrono::duration<double>(end - start).count();
    }

    stats.avg_cost = cost_sum / BENCHMARK_REPEATS;
    stats.avg_time = time_sum / BENCHMARK_REPEATS;
    return stats;
}

template <typename Solver, typename RowPrefix, typename LogResult>
void run_benchmark(const std::string& filename,
                   const std::vector<std::string>& datasets,
                   const std::string& csv_header,
                   std::uint32_t algorithm_id,
                   std::uint32_t base_seed,
                   Solver solver,
                   RowPrefix row_prefix,
                   LogResult log_result) {
    std::ofstream out = open_results_file(filename);
    out << csv_header << "\n";

    for (std::size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = dataset_filename(file);
        const auto stats = run_repeated_benchmark(file, dataset_index, base_seed, algorithm_id, solver);
        const std::string prefix = row_prefix(name);

        out << prefix << ",avg," << stats.avg_cost << "," << stats.avg_time << "\n";
        out << prefix << ",min," << stats.min_cost << "," << stats.avg_time << "\n";

        log_result(name, stats);
    }
}

void run_tests_aco(const std::string& filename, const std::vector<std::string>& datasets, std::size_t ants, double alpha, double beta, double evap, std::size_t evaluation_budget, std::uint32_t base_seed) {
    const std::size_t epochs = budgeted_iterations(evaluation_budget, ants);

    auto row_prefix = [=](const std::string& name) {
        std::ostringstream row;
        row << "ACO," << name << "," << base_seed << "," << ants << "," << alpha << "," << beta << "," << evap
            << "," << evaluation_budget << "," << epochs;
        return row.str();
    };

    auto log_result = [=](const std::string& name, const BenchmarkStats& stats) {
        std::cout << "[ACO] " << name << " ants=" << ants
                  << " alpha=" << alpha << " beta=" << beta << " evap=" << evap << " seed=" << base_seed
                  << " budget=" << evaluation_budget << " epochs=" << epochs
                  << " -> cost=" << stats.avg_cost << ", time=" << stats.avg_time << "s\n";
    };

    run_benchmark(
        filename,
        datasets,
        "algorithm,dataset,base_seed,ants,alpha,beta,evaporation,evaluation_budget,epochs,stat,cost,avg_time_sec",
        ACO_SEED_ID,
        base_seed,
        [=](std::vector<City>& cities) { aco(cities, ants, alpha, beta, evap, epochs); },
        row_prefix,
        log_result);
}

void run_tests_genetic(const std::string& filename, const std::vector<std::string>& datasets, double mutation_rate, std::size_t population_size, std::size_t evaluation_budget, std::uint32_t base_seed) {
    const std::size_t epochs = budgeted_iterations(evaluation_budget, population_size);

    auto row_prefix = [=](const std::string& name) {
        std::ostringstream row;
        row << "GA," << name << "," << base_seed << "," << mutation_rate << "," << population_size
            << "," << evaluation_budget << "," << epochs;
        return row.str();
    };

    auto log_result = [=](const std::string& name, const BenchmarkStats& stats) {
        std::cout << "[GA] " << name << " pop=" << population_size << " mut=" << mutation_rate
                  << " seed=" << base_seed << " budget=" << evaluation_budget << " epochs=" << epochs
                  << " -> cost=" << stats.avg_cost << ", time=" << stats.avg_time << "s\n";
    };

    run_benchmark(
        filename,
        datasets,
        "algorithm,dataset,base_seed,mutation,population,evaluation_budget,epochs,stat,cost,avg_time_sec",
        GA_SEED_ID,
        base_seed,
        [=](std::vector<City>& cities) { genetic_optimization(cities, mutation_rate, population_size, epochs); },
        row_prefix,
        log_result);
}

void run_tests_sa(const std::string& filename, const std::vector<std::string>& datasets, double temp, double end_temp, double cool_rate, std::size_t evaluation_budget, std::uint32_t base_seed) {
    const std::size_t epochs = scheduled_sa_iterations(temp, end_temp, cool_rate, evaluation_budget);

    auto row_prefix = [=](const std::string& name) {
        std::ostringstream row;
        row << "SA," << name << "," << base_seed << "," << temp << "," << end_temp << "," << cool_rate
            << "," << evaluation_budget << "," << epochs;
        return row.str();
    };

    auto log_result = [=](const std::string& name, const BenchmarkStats& stats) {
        std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                  << " seed=" << base_seed << " budget=" << evaluation_budget << " epochs=" << epochs
                  << " -> cost=" << stats.avg_cost << ", time=" << stats.avg_time << "s\n";
    };

    run_benchmark(
        filename,
        datasets,
        "algorithm,dataset,base_seed,temperature,end_temperature,cooling,evaluation_budget,epochs,stat,cost,avg_time_sec",
        SA_SEED_ID,
        base_seed,
        [=](std::vector<City>& cities) { sa_optimization(cities, temp, end_temp, cool_rate, epochs); },
        row_prefix,
        log_result);
}

int main(int argc, char* argv[]) {
    std::uint32_t base_seed = DEFAULT_RANDOM_SEED;
    try {
        base_seed = parse_seed(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    std::cout << "Using base seed: " << base_seed << "\n";

    std::vector<std::string> small_datasets = {
        "tsplib/tests/eil51.tsp",
        "tsplib/tests/berlin52.tsp",
        "tsplib/tests/st70.tsp",
        "tsplib/tests/eil76.tsp",
        "tsplib/tests/kroA100.tsp",
        "tsplib/tests/kroB100.tsp"
    };

    run_tests_aco("aco_results_small", small_datasets, 20, 1, 5, 0.5, BENCHMARK_EVALUATION_BUDGET, base_seed);
    run_tests_sa("sa_results_small", small_datasets, 10000, 1e-3, 0.99999, BENCHMARK_EVALUATION_BUDGET, base_seed);
    run_tests_genetic("ga_results_small", small_datasets, 0.1, 100, BENCHMARK_EVALUATION_BUDGET, base_seed);
}
