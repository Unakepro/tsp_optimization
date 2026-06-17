#include <chrono>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include "sa/sim_an.hpp"
#include "genetic/genetic.hpp"
#include "aco/aco.hpp"


constexpr std::uint32_t ACO_SEED_ID = 0xA0C0;
constexpr std::uint32_t GA_SEED_ID = 0x6A;
constexpr std::uint32_t SA_SEED_ID = 0x5A;
constexpr int BENCHMARK_REPEATS = 30;
constexpr size_t BENCHMARK_EVALUATION_BUDGET = 100000;
const std::filesystem::path RESULTS_DIR = "results";

size_t budgeted_iterations(size_t evaluation_budget, size_t work_per_iteration) {
    if (evaluation_budget == 0 || work_per_iteration == 0) {
        throw std::invalid_argument("evaluation budget and work per iteration must be positive");
    }

    return std::max<size_t>(1, evaluation_budget / work_per_iteration);
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

std::uint32_t parse_seed(int argc, char* argv[]) {
    if (argc <= 1) {
        return DEFAULT_RANDOM_SEED;
    }
    if (argc > 2) {
        throw std::invalid_argument("Usage: tsp_optimization [seed]");
    }

    std::string seed_text = argv[1];
    size_t parsed_chars = 0;
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

void run_tests_aco(const std::string& filename, const std::vector<std::string>& datasets, size_t ants, double alpha, double beta, double evap, size_t evaluation_budget, std::uint32_t base_seed) {
    std::ofstream out = open_results_file(filename);
    out << "algorithm,dataset,base_seed,ants,alpha,beta,evaporation,evaluation_budget,epochs,stat,cost,avg_time_sec\n";
    const size_t epochs = budgeted_iterations(evaluation_budget, ants);

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < BENCHMARK_REPEATS; ++r) {
            set_random_seed(derive_run_seed(base_seed, ACO_SEED_ID, dataset_index, r));

            std::vector<City> cities;
            readfile(cities, file);
            
            // std::cout << cities.size();
            auto start = std::chrono::high_resolution_clock::now();
            aco(cities, ants, alpha, beta, evap, epochs);
            auto end = std::chrono::high_resolution_clock::now();

            min_cost = std::min(min_cost, total_cost(cities));
            cost_sum += total_cost(cities);
            time_sum += std::chrono::duration<double>(end - start).count();
        }

        double avg_cost = cost_sum / BENCHMARK_REPEATS;
        double avg_time = time_sum / BENCHMARK_REPEATS;

        out << "ACO," << name << "," << base_seed << "," << ants << "," << alpha << "," << beta << "," << evap
            << "," << evaluation_budget << "," << epochs << ",avg," << avg_cost << "," << avg_time << "\n";

        out << "ACO," << name << "," << base_seed << "," << ants << "," << alpha << "," << beta << "," << evap
            << "," << evaluation_budget << "," << epochs << ",min," << min_cost << "," << avg_time << "\n";

        std::cout << "[ACO] " << name << " ants=" << ants
                    << " alpha=" << alpha << " beta=" << beta << " evap=" << evap << " seed=" << base_seed
                    << " budget=" << evaluation_budget << " epochs=" << epochs
                    << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 

void run_tests_genetic(const std::string& filename, const std::vector<std::string>& datasets, double mutation_rate, size_t population_size, size_t evaluation_budget, std::uint32_t base_seed) {
    std::ofstream out = open_results_file(filename);
    out << "algorithm,dataset,base_seed,mutation,population,evaluation_budget,epochs,stat,cost,avg_time_sec\n";
    const size_t epochs = budgeted_iterations(evaluation_budget, population_size);
    
    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < BENCHMARK_REPEATS; ++r) {
            set_random_seed(derive_run_seed(base_seed, GA_SEED_ID, dataset_index, r));

            std::vector<City> cities;
            readfile(cities, file);

            auto start = std::chrono::high_resolution_clock::now();
            genetic_optimization(cities, mutation_rate, population_size, epochs);
            auto end = std::chrono::high_resolution_clock::now();

            min_cost = std::min(min_cost, total_cost(cities));
            cost_sum += total_cost(cities);
            time_sum += std::chrono::duration<double>(end - start).count();
        }

        double avg_cost = cost_sum / BENCHMARK_REPEATS;
        double avg_time = time_sum / BENCHMARK_REPEATS;

        out << "GA," << name << "," << base_seed << "," << mutation_rate << "," << population_size
            << "," << evaluation_budget << "," << epochs << ",avg," << avg_cost << "," << avg_time << "\n";

        out << "GA," << name << "," << base_seed << "," << mutation_rate << "," << population_size
            << "," << evaluation_budget << "," << epochs << ",min," << min_cost << "," << avg_time << "\n";

        std::cout << "[GA] " << name << " pop=" << population_size << " mut=" << mutation_rate
                          << " seed=" << base_seed << " budget=" << evaluation_budget << " epochs=" << epochs
                          << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 


void run_tests_sa(const std::string& filename, const std::vector<std::string>& datasets, double temp, double end_temp, double cool_rate, size_t evaluation_budget, std::uint32_t base_seed) {
    std::ofstream out = open_results_file(filename);
    out << "algorithm,dataset,base_seed,temperature,end_temperature,cooling,evaluation_budget,epochs,stat,cost,avg_time_sec\n";
    const size_t max_epochs = evaluation_budget;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < BENCHMARK_REPEATS; ++r) {
            set_random_seed(derive_run_seed(base_seed, SA_SEED_ID, dataset_index, r));

            std::vector<City> cities;
            readfile(cities, file);

            auto start = std::chrono::high_resolution_clock::now();
            sa_optimization(cities, temp, end_temp, cool_rate, max_epochs);
            auto end = std::chrono::high_resolution_clock::now();

            min_cost = std::min(min_cost, total_cost(cities));
            cost_sum += total_cost(cities);
            time_sum += std::chrono::duration<double>(end - start).count();
        }

        double avg_cost = cost_sum / BENCHMARK_REPEATS;
        double avg_time = time_sum / BENCHMARK_REPEATS;

        out << "SA," << name << "," << base_seed << "," << temp << "," << end_temp << "," << cool_rate
            << "," << evaluation_budget << "," << max_epochs << ",avg," << avg_cost << "," << avg_time << "\n";

        out << "SA," << name << "," << base_seed << "," << temp << "," << end_temp << "," << cool_rate
            << "," << evaluation_budget << "," << max_epochs << ",min," << min_cost << "," << avg_time << "\n";

        std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                << " seed=" << base_seed << " budget=" << evaluation_budget << " epochs=" << max_epochs
                << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
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


    run_tests_aco("aco_results_small" , small_datasets, 20, 1, 5, 0.5, BENCHMARK_EVALUATION_BUDGET, base_seed);
    run_tests_sa("sa_results_small" , small_datasets, 10000, 1e-3, 0.99999, BENCHMARK_EVALUATION_BUDGET, base_seed);
    run_tests_genetic("ga_results_small" , small_datasets, 0.1, 100, BENCHMARK_EVALUATION_BUDGET, base_seed);
}
