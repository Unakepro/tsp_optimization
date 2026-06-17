#include <chrono>
#include <cstdint>
#include <exception>
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
#include "hyperparameter_search/hyperparameter_search.hpp"


constexpr std::uint32_t ACO_SEED_ID = 0xA0C0;
constexpr std::uint32_t GA_SEED_ID = 0x6A;
constexpr std::uint32_t SA_SEED_ID = 0x5A;

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

void run_tests_aco(const std::string& filename, const std::vector<std::string>& datasets, size_t ants, double alpha, double beta, double evap, size_t epochs, std::uint32_t base_seed) {
    std::ofstream out(filename);
    out << "algorithm,dataset,base_seed,ants,alpha,beta,evaporation,epochs,stat,cost,avg_time_sec\n";

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < repeats; ++r) {
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

        double avg_cost = cost_sum / repeats;
        double avg_time = time_sum / repeats;

        out << "ACO," << name << "," << base_seed << "," << ants << "," << alpha << "," << beta << "," << evap
            << "," <<  epochs << ",avg," << avg_cost << "," << avg_time << "\n";

        out << "ACO," << name << "," << base_seed << "," << ants << "," << alpha << "," << beta << "," << evap
            << ","  <<  epochs << ",min," << min_cost << "," << avg_time << "\n";

        std::cout << "[ACO] " << name << " ants=" << ants
                    << " alpha=" << alpha << " beta=" << beta << " evap=" << evap << " seed=" << base_seed
                    << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 

void run_tests_genetic(const std::string& filename, const std::vector<std::string>& datasets, double mutation_rate, size_t population_size, size_t epochs, std::uint32_t base_seed) {
    std::ofstream out(filename);
    out << "algorithm,dataset,base_seed,mutation,population,epochs,stat,cost,avg_time_sec\n";
    
    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < repeats; ++r) {
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

        double avg_cost = cost_sum / repeats;
        double avg_time = time_sum / repeats;

        out << "GA," << name << "," << base_seed << "," << mutation_rate << "," << population_size << "," << epochs
            << ",avg," << avg_cost << "," << avg_time << "\n";

        out << "GA," << name << "," << base_seed << "," << mutation_rate << "," << population_size << "," << epochs
            << ",min," << min_cost << "," << avg_time << "\n";

        std::cout << "[GA] " << name << " pop=" << population_size << " mut=" << mutation_rate
                          << " seed=" << base_seed << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 


void run_tests_sa(const std::string& filename, const std::vector<std::string>& datasets, double temp, double end_temp, double cool_rate, size_t max_epochs, std::uint32_t base_seed) {    
    std::ofstream out(filename);
    out << "algorithm,dataset,base_seed,temperature,end_temperature,cooling,epochs,stat,cost,avg_time_sec\n";

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < repeats; ++r) {
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

        double avg_cost = cost_sum / repeats;
        double avg_time = time_sum / repeats;

        out << "SA," << name << "," << base_seed << "," << temp << "," << end_temp << "," << cool_rate << "," << max_epochs
            << ",avg," << avg_cost << "," << avg_time << "\n";

        out << "SA," << name << "," << base_seed << "," << temp << "," << end_temp << "," << cool_rate << "," << max_epochs
            << ",min," << min_cost << "," << avg_time << "\n";

        std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                << " seed=" << base_seed << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 

void search_parameters() {
    std::vector<std::string> datasets_search = {
        "tsplib/tests/eil51.tsp",
        "tsplib/tests/pr144.tsp",
        "tsplib/tests/rl5934.tsp"
    };

    random_search_aco(datasets_search);
    grid_search_aco(datasets_search);

    grid_search_sa(datasets_search);
    random_search_sa(datasets_search);

    random_search_genetic(datasets_search);
    grid_search_genetic(datasets_search);

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


    run_tests_aco("aco_results_small" , small_datasets, 20, 1, 5, 0.5, 300, base_seed);
    run_tests_sa("sa_results_small" , small_datasets, 10000, 1e-3, 0.99999, 1000000, base_seed);
    run_tests_genetic("ga_results_small" , small_datasets, 0.1, 100, 1000, base_seed);
}
