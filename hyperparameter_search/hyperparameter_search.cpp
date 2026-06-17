#include "hyperparameter_search.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Cities/city.hpp"
#include "../sa/sim_an.hpp"
#include "../genetic/genetic.hpp"
#include "../aco/aco.hpp"

namespace {

inline constexpr std::uint32_t SA_SEARCH_ID = 0x5A500001u;
inline constexpr std::uint32_t GA_SEARCH_ID = 0x6A500001u;
inline constexpr std::uint32_t ACO_SEARCH_ID = 0xAC050001u;
inline constexpr std::uint32_t PARAM_SEED_ID = 0xC0FFEE01u;
const std::filesystem::path SEARCH_RESULTS_DIR = "search_results";

struct SearchStats {
    double best_cost = std::numeric_limits<double>::max();
    double mean_cost = 0.0;
    double stddev_cost = 0.0;
    double mean_time = 0.0;
    double best_known = 0.0;
    double gap_percent = 0.0;
    std::uint32_t first_run_seed = 0;
};

std::string dataset_name(const std::string& path) {
    const size_t slash_pos = path.find_last_of("/\\");
    std::string name = slash_pos == std::string::npos ? path : path.substr(slash_pos + 1);

    const size_t dot_pos = name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        name = name.substr(0, dot_pos);
    }

    return name;
}

std::unordered_map<std::string, double> load_best_known_solutions(const std::string& filename) {
    std::unordered_map<std::string, double> solutions;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return solutions;
    }

    std::string line;
    while (std::getline(file, line)) {
        const size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        const std::string name = trim(line.substr(0, colon_pos));
        std::istringstream value_stream(line.substr(colon_pos + 1));
        double value = 0.0;
        if (value_stream >> value) {
            solutions[name] = value;
        }
    }

    return solutions;
}

std::ofstream open_search_results_file(const std::string& filename) {
    std::filesystem::create_directories(SEARCH_RESULTS_DIR);
    const auto output_path = SEARCH_RESULTS_DIR / filename;

    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open search results file: " + output_path.string());
    }

    return out;
}

double best_known_for_dataset(const std::string& dataset) {
    static const auto solutions = load_best_known_solutions("tsplib/solutions");
    const auto found = solutions.find(dataset_name(dataset));
    return found == solutions.end() ? 0.0 : found->second;
}

std::uint32_t derive_parameter_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, size_t dataset_index, size_t trial_index) {
    return derive_run_seed(base_seed, algorithm_id ^ PARAM_SEED_ID, dataset_index, trial_index);
}

std::uint32_t derive_search_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, size_t dataset_index, size_t trial_index, size_t repeat_index) {
    const std::uint32_t trial_seed = derive_run_seed(base_seed, algorithm_id, dataset_index, trial_index);
    return derive_run_seed(trial_seed, algorithm_id ^ PARAM_SEED_ID, repeat_index, 0);
}

template <typename Solver>
SearchStats run_parameter_set(const std::string& file, const HyperparameterSearchConfig& config, std::uint32_t algorithm_id, size_t dataset_index, size_t trial_index, Solver solver) {
    std::vector<double> costs;
    costs.reserve(static_cast<size_t>(config.repeats));

    double time_sum = 0.0;

    for (int r = 0; r < config.repeats; ++r) {
        const auto run_seed = derive_search_seed(config.base_seed, algorithm_id, dataset_index, trial_index, static_cast<size_t>(r));
        set_random_seed(run_seed);

        std::vector<City> cities;
        readfile(cities, file);

        const auto start = std::chrono::high_resolution_clock::now();
        solver(cities);
        const auto end = std::chrono::high_resolution_clock::now();

        const double cost = total_cost(cities);
        costs.push_back(cost);
        time_sum += std::chrono::duration<double>(end - start).count();
    }

    SearchStats stats;
    stats.first_run_seed = derive_search_seed(config.base_seed, algorithm_id, dataset_index, trial_index, 0);
    stats.best_known = best_known_for_dataset(file);

    double cost_sum = 0.0;
    for (double cost: costs) {
        stats.best_cost = std::min(stats.best_cost, cost);
        cost_sum += cost;
    }

    stats.mean_cost = cost_sum / costs.size();
    stats.mean_time = time_sum / costs.size();

    double variance = 0.0;
    for (double cost: costs) {
        const double diff = cost - stats.mean_cost;
        variance += diff * diff;
    }
    stats.stddev_cost = std::sqrt(variance / costs.size());

    if (stats.best_known > 0.0) {
        stats.gap_percent = 100.0 * (stats.best_cost - stats.best_known) / stats.best_known;
    }

    return stats;
}

void write_stats(std::ofstream& out, const SearchStats& stats) {
    out << stats.best_cost << "," << stats.mean_cost << "," << stats.stddev_cost << "," << stats.mean_time << ",";
    if (stats.best_known > 0.0) {
        out << stats.best_known << "," << stats.gap_percent;
    } else {
        out << ",";
    }
    out << "\n";
}

int budgeted_iterations(const HyperparameterSearchConfig& config, int work_per_iteration) {
    if (work_per_iteration <= 0) {
        throw std::invalid_argument("work per iteration must be positive");
    }

    return std::max(1, config.evaluation_budget / work_per_iteration);
}

void random_search_sa(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    std::ofstream out = open_search_results_file("sa_random_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,temperature,cooling,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    const int random_trials = 20;
    const int iterations = config.evaluation_budget;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < random_trials; ++t) {
            const auto param_seed = derive_parameter_seed(config.base_seed, SA_SEARCH_ID, dataset_index, static_cast<size_t>(t));
            set_random_seed(param_seed);

            std::uniform_int_distribution<> temp_dist(1000, 20000);
            std::uniform_real_distribution<> cooling_dist(0.9999, 0.99999);

            const int temp = temp_dist(gen);
            const double cooling = cooling_dist(gen);

            const auto stats = run_parameter_set(file, config, SA_SEARCH_ID, dataset_index, static_cast<size_t>(t), [&](std::vector<City>& cities) {
                sa_optimization(cities, temp, 1e-3, cooling, iterations);
            });

            out << "SA," << name << ",random," << t << "," << config.base_seed << "," << param_seed << "," << stats.first_run_seed << "," << config.repeats
                << "," << config.evaluation_budget << "," << temp << "," << cooling << "," << iterations << ",";
            write_stats(out, stats);

            std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cooling
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

void grid_search_sa(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    std::ofstream out = open_search_results_file("sa_grid_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,temperature,cooling,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    std::vector<int> start_temperatures = {1000, 5000, 10000};
    std::vector<double> cooling_rates = {0.99, 0.999, 0.9999, 0.99999};

    const int iterations = config.evaluation_budget;
    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);
        size_t trial_index = 0;

        for (auto temp: start_temperatures) {
            for (double cool_rate: cooling_rates) {
                const auto param_seed = derive_parameter_seed(config.base_seed, SA_SEARCH_ID, dataset_index, trial_index);

                const auto stats = run_parameter_set(file, config, SA_SEARCH_ID, dataset_index, trial_index, [&](std::vector<City>& cities) {
                    sa_optimization(cities, temp, 1e-3, cool_rate, iterations);
                });

                out << "SA," << name << ",grid," << trial_index << "," << config.base_seed << "," << param_seed << "," << stats.first_run_seed << "," << config.repeats
                    << "," << config.evaluation_budget << "," << temp << "," << cool_rate << "," << iterations << ",";
                write_stats(out, stats);

                std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                          << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";

                ++trial_index;
            }
        }
    }
}

void random_search_genetic(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    std::ofstream out = open_search_results_file("ga_random_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,population,mutation,generations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    const int random_trials = 20;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < random_trials; ++t) {
            const auto param_seed = derive_parameter_seed(config.base_seed, GA_SEARCH_ID, dataset_index, static_cast<size_t>(t));
            set_random_seed(param_seed);

            std::uniform_int_distribution<> pop_dist(10, 100);
            std::uniform_real_distribution<> mutation_dist(0.01, 0.1);

            const int pop = pop_dist(gen);
            const double mut = mutation_dist(gen);
            const int generations = budgeted_iterations(config, pop);

            const auto stats = run_parameter_set(file, config, GA_SEARCH_ID, dataset_index, static_cast<size_t>(t), [&](std::vector<City>& cities) {
                genetic_optimization(cities, mut, pop, generations);
            });

            out << "GA," << name << ",random," << t << "," << config.base_seed << "," << param_seed << "," << stats.first_run_seed << "," << config.repeats
                << "," << config.evaluation_budget << "," << pop << "," << mut << "," << generations << ",";
            write_stats(out, stats);

            std::cout << "[GA] " << name << " pop=" << pop << " mut=" << mut
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

void grid_search_genetic(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    std::ofstream out = open_search_results_file("ga_grid_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,population,mutation,generations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    std::vector<int> populations = {10, 50, 100};
    std::vector<double> mutation_rates = {0.01, 0.05, 0.1};

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);
        size_t trial_index = 0;

        for (auto pop: populations) {
            for (double mut: mutation_rates) {
                const auto param_seed = derive_parameter_seed(config.base_seed, GA_SEARCH_ID, dataset_index, trial_index);
                const int generations = budgeted_iterations(config, pop);

                const auto stats = run_parameter_set(file, config, GA_SEARCH_ID, dataset_index, trial_index, [&](std::vector<City>& cities) {
                    genetic_optimization(cities, mut, pop, generations);
                });

                out << "GA," << name << ",grid," << trial_index << "," << config.base_seed << "," << param_seed << "," << stats.first_run_seed << "," << config.repeats
                    << "," << config.evaluation_budget << "," << pop << "," << mut << "," << generations << ",";
                write_stats(out, stats);

                std::cout << "[GA] " << name << " pop=" << pop << " mut=" << mut
                          << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";

                ++trial_index;
            }
        }
    }
}

void grid_search_aco(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    std::ofstream out = open_search_results_file("aco_grid_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,ants,alpha,beta,evaporation,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    std::vector<int> ants_list = {10, 20, 40};
    std::vector<double> alpha_list = {1.0, 1.5};
    std::vector<double> beta_list = {3.0, 5.0};
    std::vector<double> evap_list = {0.1, 0.3, 0.5};

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);
        size_t trial_index = 0;

        for (int ants: ants_list) {
            for (double alpha: alpha_list) {
                for (double beta: beta_list) {
                    for (double evap: evap_list) {
                        const auto param_seed = derive_parameter_seed(config.base_seed, ACO_SEARCH_ID, dataset_index, trial_index);
                        const int iterations = budgeted_iterations(config, ants);

                        const auto stats = run_parameter_set(file, config, ACO_SEARCH_ID, dataset_index, trial_index, [&](std::vector<City>& cities) {
                            aco(cities, ants, alpha, beta, evap, iterations);
                        });

                        out << "ACO," << name << ",grid," << trial_index << "," << config.base_seed << "," << param_seed << "," << stats.first_run_seed << "," << config.repeats
                            << "," << config.evaluation_budget << "," << ants << "," << alpha << "," << beta << "," << evap << "," << iterations << ",";
                        write_stats(out, stats);

                        std::cout << "[ACO] " << name << " ants=" << ants
                                  << " alpha=" << alpha << " beta=" << beta << " evap=" << evap
                                  << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";

                        ++trial_index;
                    }
                }
            }
        }
    }
}

void random_search_aco(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    std::ofstream out = open_search_results_file("aco_random_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,ants,alpha,beta,evaporation,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    const int trials = 10;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < trials; ++t) {
            const auto param_seed = derive_parameter_seed(config.base_seed, ACO_SEARCH_ID, dataset_index, static_cast<size_t>(t));
            set_random_seed(param_seed);

            std::uniform_int_distribution<> ants_dist(10, 40);
            std::uniform_real_distribution<> alpha_dist(1.0, 2);
            std::uniform_real_distribution<> beta_dist(3.0, 7.0);
            std::uniform_real_distribution<> evap_dist(0.1, 0.5);

            const int ants = ants_dist(gen);
            const double alpha = alpha_dist(gen);
            const double beta = beta_dist(gen);
            const double evap = evap_dist(gen);
            const int iterations = budgeted_iterations(config, ants);

            const auto stats = run_parameter_set(file, config, ACO_SEARCH_ID, dataset_index, static_cast<size_t>(t), [&](std::vector<City>& cities) {
                aco(cities, ants, alpha, beta, evap, iterations);
            });

            out << "ACO," << name << ",random," << t << "," << config.base_seed << "," << param_seed << "," << stats.first_run_seed << "," << config.repeats
                << "," << config.evaluation_budget << "," << ants << "," << alpha << "," << beta << "," << evap << "," << iterations << ",";
            write_stats(out, stats);

            std::cout << "[ACO] " << name << " ants=" << ants
                      << " alpha=" << alpha << " beta=" << beta << " evap=" << evap
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

std::uint32_t parse_seed_argument(const std::string& text) {
    size_t parsed_chars = 0;
    unsigned long seed_value = 0;
    try {
        seed_value = std::stoul(text, &parsed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument("seed must be an unsigned 32-bit integer");
    }

    if (parsed_chars != text.size() || seed_value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("seed must be an unsigned 32-bit integer");
    }

    return static_cast<std::uint32_t>(seed_value);
}

int parse_repeats_argument(const std::string& text) {
    size_t parsed_chars = 0;
    int parsed_repeats = 0;
    try {
        parsed_repeats = std::stoi(text, &parsed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument("repeats must be a positive integer");
    }

    if (parsed_chars != text.size() || parsed_repeats <= 0) {
        throw std::invalid_argument("repeats must be a positive integer");
    }

    return parsed_repeats;
}

int parse_evaluation_budget_argument(const std::string& text) {
    size_t parsed_chars = 0;
    int parsed_budget = 0;
    try {
        parsed_budget = std::stoi(text, &parsed_chars);
    } catch (const std::exception&) {
        throw std::invalid_argument("evaluation budget must be a positive integer");
    }

    if (parsed_chars != text.size() || parsed_budget <= 0) {
        throw std::invalid_argument("evaluation budget must be a positive integer");
    }

    return parsed_budget;
}

DatasetSet parse_dataset_set_argument(const std::string& text) {
    if (text == "tuning") {
        return DatasetSet::Tuning;
    }
    if (text == "final") {
        return DatasetSet::FinalBenchmark;
    }

    throw std::invalid_argument("dataset set must be either tuning or final");
}

const char* dataset_set_name(DatasetSet dataset_set) {
    return dataset_set == DatasetSet::Tuning ? "tuning" : "final";
}

HyperparameterSearchConfig parse_arguments(int argc, char* argv[]) {
    if (argc > 5) {
        throw std::invalid_argument("usage: tsp_hyperparameter_search [seed] [repeats] [tuning|final] [evaluation_budget]");
    }

    HyperparameterSearchConfig config;
    if (argc >= 2) {
        config.base_seed = parse_seed_argument(argv[1]);
    }
    if (argc >= 3) {
        config.repeats = parse_repeats_argument(argv[2]);
    }
    if (argc >= 4) {
        config.dataset_set = parse_dataset_set_argument(argv[3]);
    }
    if (argc >= 5) {
        config.evaluation_budget = parse_evaluation_budget_argument(argv[4]);
    }

    return config;
}

}  // namespace

std::vector<std::string> tuning_datasets() {
    return {
        "tsplib/tests/eil51.tsp",
        "tsplib/tests/st70.tsp",
        "tsplib/tests/pr144.tsp"
    };
}

std::vector<std::string> final_benchmark_datasets() {
    return {
        "tsplib/tests/berlin52.tsp",
        "tsplib/tests/eil76.tsp",
        "tsplib/tests/kroA100.tsp",
        "tsplib/tests/kroB100.tsp",
        "tsplib/tests/ch130.tsp"
    };
}

void run_all_hyperparameter_searches(const HyperparameterSearchConfig& config) {
    if (config.repeats <= 0) {
        throw std::invalid_argument("repeats must be a positive integer");
    }
    if (config.evaluation_budget <= 0) {
        throw std::invalid_argument("evaluation budget must be a positive integer");
    }

    const auto datasets = config.dataset_set == DatasetSet::Tuning ? tuning_datasets() : final_benchmark_datasets();

    std::cout << "Using base seed: " << config.base_seed << "\n";
    std::cout << "Using repeats: " << config.repeats << "\n";
    std::cout << "Using dataset set: " << dataset_set_name(config.dataset_set) << "\n";
    std::cout << "Using evaluation budget: " << config.evaluation_budget << "\n";

    random_search_aco(datasets, config);
    grid_search_aco(datasets, config);

    grid_search_sa(datasets, config);
    random_search_sa(datasets, config);

    random_search_genetic(datasets, config);
    grid_search_genetic(datasets, config);
}

int main(int argc, char* argv[]) {
    try {
        run_all_hyperparameter_searches(parse_arguments(argc, argv));
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }

    return 0;
}
