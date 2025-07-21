#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "sa/sim_an.hpp"
#include "genetic/genetic.hpp"
#include "aco/aco.hpp"
#include "hyperparameter_search/hyperparameter_search.hpp"


void run_tests_aco(const std::string& filename, const std::vector<std::string>& datasets, size_t ants, double alpha, double beta, double evap, size_t epochs) {
    std::ofstream out(filename);

    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < repeats; ++r) {
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

        out << "ACO," << name << "," << ants << "," << alpha << "," << beta << "," << evap
            << "," <<  epochs << "," << avg_cost << "," << avg_time << "\n";

        out << "ACO," << name << "," << ants << "," << alpha << "," << beta << "," << evap
            << ","  <<  epochs << "," << min_cost << "," << avg_time << "\n";

        std::cout << "[ACO] " << name << " ants=" << ants
                    << " alpha=" << alpha << " beta=" << beta << " evap=" << evap
                    << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 

void run_tests_genetic(const std::string& filename, const std::vector<std::string>& datasets, double mutation_rate, size_t population_size, size_t epochs) {
    std::ofstream out(filename);
    
    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < repeats; ++r) {
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

        out << "GA," << name << "," << mutation_rate << "," << population_size << "," << epochs << ","
            << "," << avg_cost << "," << avg_time << "\n";

        out << "GA," << name << "," << mutation_rate << "," << population_size << "," << epochs << "," 
            << "," << min_cost << "," << avg_time << "\n";

        std::cout << "[GA] " << name << " pop=" << population_size << " mut=" << mutation_rate
                          << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
    }
} 


void run_tests_sa(const std::string& filename, const std::vector<std::string>& datasets, double temp, double end_temp, double cool_rate, size_t max_epochs) {    
    std::ofstream out(filename);

    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        double min_cost = std::numeric_limits<double>::max();
        double cost_sum = 0;
        double time_sum = 0;

        for (int r = 0; r < repeats; ++r) {
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

        out << "SA," << name << "," << temp << "," << end_temp << "," << cool_rate << "," << max_epochs << ","
            << "," << avg_cost << "," << avg_time << "\n";

        out << "SA," << name << "," << temp << "," << end_temp << "," << cool_rate << "," << max_epochs << ","
            << "," << min_cost << "," << avg_time << "\n";

        std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
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
    


int main() {

     std::vector<std::string> small_datasets = {
        "tsplib/tests/eil51.tsp",
        "tsplib/tests/berlin52.tsp",
        "tsplib/tests/st70.tsp",
        "tsplib/tests/eil76.tsp",
        "tsplib/tests/kroA100.tsp",
        "tsplib/tests/kroB100.tsp"
    };


    run_tests_aco("aco_results_small" , small_datasets, 20, 1, 5, 0.5, 300);
    run_tests_sa("sa_results_small" , small_datasets, 10000, 1e-3, 0.99999, 1000000);
    run_tests_genetic("ga_results_small" , small_datasets, 0.1, 100, 40000);
}