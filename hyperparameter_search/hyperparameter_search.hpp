#ifndef hyperparameter_search
#define hyperparameter_search

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Cities/city.hpp"
#include "../sa/sim_an.hpp"
#include "../genetic/genetic.hpp"
#include "../aco/aco.hpp"

const int repeats = 30;

inline constexpr std::uint32_t SA_SEARCH_ID = 0x5A500001u;
inline constexpr std::uint32_t GA_SEARCH_ID = 0x6A500001u;
inline constexpr std::uint32_t ACO_SEARCH_ID = 0xAC050001u;
inline constexpr std::uint32_t PARAM_SEED_ID = 0xC0FFEE01u;

struct SearchStats {
    double best_cost = std::numeric_limits<double>::max();
    double mean_cost = 0.0;
    double stddev_cost = 0.0;
    double mean_time = 0.0;
    double best_known = 0.0;
    double gap_percent = 0.0;
    std::uint32_t first_run_seed = 0;
};

inline std::string dataset_name(const std::string& path) {
    const size_t slash_pos = path.find_last_of("/\\");
    std::string name = slash_pos == std::string::npos ? path : path.substr(slash_pos + 1);

    const size_t dot_pos = name.find_last_of('.');
    if (dot_pos != std::string::npos) {
        name = name.substr(0, dot_pos);
    }

    return name;
}

inline std::unordered_map<std::string, double> load_best_known_solutions(const std::string& filename) {
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

inline double best_known_for_dataset(const std::string& dataset) {
    static const auto solutions = load_best_known_solutions("tsplib/solutions");
    const auto found = solutions.find(dataset_name(dataset));
    return found == solutions.end() ? 0.0 : found->second;
}

inline std::uint32_t derive_search_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, size_t dataset_index, size_t trial_index, size_t repeat_index) {
    const std::uint32_t trial_seed = derive_run_seed(base_seed, algorithm_id, dataset_index, trial_index);
    return derive_run_seed(trial_seed, algorithm_id ^ PARAM_SEED_ID, repeat_index, 0);
}

inline std::uint32_t derive_parameter_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, size_t dataset_index, size_t trial_index) {
    return derive_run_seed(base_seed, algorithm_id ^ PARAM_SEED_ID, dataset_index, trial_index);
}

template <typename Solver>
SearchStats run_parameter_set(const std::string& file, std::uint32_t base_seed, std::uint32_t algorithm_id, size_t dataset_index, size_t trial_index, Solver solver) {
    std::vector<double> costs;
    costs.reserve(repeats);

    double time_sum = 0.0;

    for (int r = 0; r < repeats; ++r) {
        const auto run_seed = derive_search_seed(base_seed, algorithm_id, dataset_index, trial_index, static_cast<size_t>(r));
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
    stats.first_run_seed = derive_search_seed(base_seed, algorithm_id, dataset_index, trial_index, 0);
    stats.best_known = best_known_for_dataset(file);

    double cost_sum = 0.0;
    for (double cost : costs) {
        stats.best_cost = std::min(stats.best_cost, cost);
        cost_sum += cost;
    }

    stats.mean_cost = cost_sum / costs.size();
    stats.mean_time = time_sum / costs.size();

    double variance = 0.0;
    for (double cost : costs) {
        const double diff = cost - stats.mean_cost;
        variance += diff * diff;
    }
    stats.stddev_cost = std::sqrt(variance / costs.size());

    if (stats.best_known > 0.0) {
        stats.gap_percent = 100.0 * (stats.best_cost - stats.best_known) / stats.best_known;
    }

    return stats;
}

inline void write_stats(std::ofstream& out, const SearchStats& stats) {
    out << stats.best_cost << "," << stats.mean_cost << "," << stats.stddev_cost << "," << stats.mean_time << ",";
    if (stats.best_known > 0.0) {
        out << stats.best_known << "," << stats.gap_percent;
    } else {
        out << ",";
    }
    out << "\n";
}

void random_search_sa(const std::vector<std::string>& datasets, std::uint32_t base_seed = DEFAULT_RANDOM_SEED) {
    std::ofstream out("sa_random_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,temperature,cooling,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    const int random_trials = 20;
    const int generations = 1000000;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < random_trials; ++t) {
            const auto param_seed = derive_parameter_seed(base_seed, SA_SEARCH_ID, dataset_index, static_cast<size_t>(t));
            set_random_seed(param_seed);

            std::uniform_int_distribution<> temp_dist(1000, 20000);
            std::uniform_real_distribution<> cooling_dist(0.9999, 0.99999);

            const int temp = temp_dist(gen);
            const double cooling = cooling_dist(gen);

            const auto stats = run_parameter_set(file, base_seed, SA_SEARCH_ID, dataset_index, static_cast<size_t>(t), [&](std::vector<City>& cities) {
                sa_optimization(cities, temp, 1e-3, cooling, generations);
            });

            out << "SA," << name << ",random," << t << "," << base_seed << "," << param_seed << "," << stats.first_run_seed << "," << repeats
                << "," << temp << "," << cooling << "," << generations << ",";
            write_stats(out, stats);

            std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cooling
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

void grid_search_sa(const std::vector<std::string>& datasets, std::uint32_t base_seed = DEFAULT_RANDOM_SEED) {
    std::ofstream out("sa_grid_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,temperature,cooling,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    std::vector<int> start_temperatures = {1000, 5000, 10000};
    std::vector<double> cooling_rates = {0.99, 0.999, 0.9999, 0.99999};

    const int generations = 1000000;
    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);
        size_t trial_index = 0;

        for (auto temp : start_temperatures) {
            for (double cool_rate : cooling_rates) {
                const auto param_seed = derive_parameter_seed(base_seed, SA_SEARCH_ID, dataset_index, trial_index);

                const auto stats = run_parameter_set(file, base_seed, SA_SEARCH_ID, dataset_index, trial_index, [&](std::vector<City>& cities) {
                    sa_optimization(cities, temp, 1e-3, cool_rate, generations);
                });

                out << "SA," << name << ",grid," << trial_index << "," << base_seed << "," << param_seed << "," << stats.first_run_seed << "," << repeats
                    << "," << temp << "," << cool_rate << "," << generations << ",";
                write_stats(out, stats);

                std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                          << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";

                ++trial_index;
            }
        }
    }
}

void random_search_genetic(const std::vector<std::string>& datasets, std::uint32_t base_seed = DEFAULT_RANDOM_SEED) {
    std::ofstream out("ga_random_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,population,mutation,generations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    const int random_trials = 20;
    const int generations = 1000;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < random_trials; ++t) {
            const auto param_seed = derive_parameter_seed(base_seed, GA_SEARCH_ID, dataset_index, static_cast<size_t>(t));
            set_random_seed(param_seed);

            std::uniform_int_distribution<> pop_dist(10, 100);
            std::uniform_real_distribution<> mutation_dist(0.01, 0.1);

            const int pop = pop_dist(gen);
            const double mut = mutation_dist(gen);

            const auto stats = run_parameter_set(file, base_seed, GA_SEARCH_ID, dataset_index, static_cast<size_t>(t), [&](std::vector<City>& cities) {
                genetic_optimization(cities, mut, pop, generations);
            });

            out << "GA," << name << ",random," << t << "," << base_seed << "," << param_seed << "," << stats.first_run_seed << "," << repeats
                << "," << pop << "," << mut << "," << generations << ",";
            write_stats(out, stats);

            std::cout << "[GA] " << name << " pop=" << pop << " mut=" << mut
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

void grid_search_genetic(const std::vector<std::string>& datasets, std::uint32_t base_seed = DEFAULT_RANDOM_SEED) {
    std::ofstream out("ga_grid_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,population,mutation,generations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    std::vector<int> populations = {10, 50, 100};
    std::vector<double> mutation_rates = {0.01, 0.05, 0.1};

    const int generations = 1000;
    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);
        size_t trial_index = 0;

        for (auto pop : populations) {
            for (double mut : mutation_rates) {
                const auto param_seed = derive_parameter_seed(base_seed, GA_SEARCH_ID, dataset_index, trial_index);

                const auto stats = run_parameter_set(file, base_seed, GA_SEARCH_ID, dataset_index, trial_index, [&](std::vector<City>& cities) {
                    genetic_optimization(cities, mut, pop, generations);
                });

                out << "GA," << name << ",grid," << trial_index << "," << base_seed << "," << param_seed << "," << stats.first_run_seed << "," << repeats
                    << "," << pop << "," << mut << "," << generations << ",";
                write_stats(out, stats);

                std::cout << "[GA] " << name << " pop=" << pop << " mut=" << mut
                          << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";

                ++trial_index;
            }
        }
    }
}

void grid_search_aco(const std::vector<std::string>& datasets, std::uint32_t base_seed = DEFAULT_RANDOM_SEED) {
    std::ofstream out("aco_grid_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,ants,alpha,beta,evaporation,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    std::vector<int> ants_list = {10, 20, 40};
    std::vector<double> alpha_list = {1.0, 1.5};
    std::vector<double> beta_list = {3.0, 5.0};
    std::vector<double> evap_list = {0.1, 0.3, 0.5};

    const int iterations = 100;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);
        size_t trial_index = 0;

        for (int ants : ants_list) {
            for (double alpha : alpha_list) {
                for (double beta : beta_list) {
                    for (double evap : evap_list) {
                        const auto param_seed = derive_parameter_seed(base_seed, ACO_SEARCH_ID, dataset_index, trial_index);

                        const auto stats = run_parameter_set(file, base_seed, ACO_SEARCH_ID, dataset_index, trial_index, [&](std::vector<City>& cities) {
                            aco(cities, ants, alpha, beta, evap, iterations);
                        });

                        out << "ACO," << name << ",grid," << trial_index << "," << base_seed << "," << param_seed << "," << stats.first_run_seed << "," << repeats
                            << "," << ants << "," << alpha << "," << beta << "," << evap << "," << iterations << ",";
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

void random_search_aco(const std::vector<std::string>& datasets, std::uint32_t base_seed = DEFAULT_RANDOM_SEED) {
    std::ofstream out("aco_random_search.csv");
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,ants,alpha,beta,evaporation,iterations,best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    const int iterations = 100;
    const int trials = 10;

    for (size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < trials; ++t) {
            const auto param_seed = derive_parameter_seed(base_seed, ACO_SEARCH_ID, dataset_index, static_cast<size_t>(t));
            set_random_seed(param_seed);

            std::uniform_int_distribution<> ants_dist(10, 40);
            std::uniform_real_distribution<> alpha_dist(1.0, 2);
            std::uniform_real_distribution<> beta_dist(3.0, 7.0);
            std::uniform_real_distribution<> evap_dist(0.1, 0.5);

            const int ants = ants_dist(gen);
            const double alpha = alpha_dist(gen);
            const double beta = beta_dist(gen);
            const double evap = evap_dist(gen);

            const auto stats = run_parameter_set(file, base_seed, ACO_SEARCH_ID, dataset_index, static_cast<size_t>(t), [&](std::vector<City>& cities) {
                aco(cities, ants, alpha, beta, evap, iterations);
            });

            out << "ACO," << name << ",random," << t << "," << base_seed << "," << param_seed << "," << stats.first_run_seed << "," << repeats
                << "," << ants << "," << alpha << "," << beta << "," << evap << "," << iterations << ",";
            write_stats(out, stats);

            std::cout << "[ACO] " << name << " ants=" << ants
                      << " alpha=" << alpha << " beta=" << beta << " evap=" << evap
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

#endif
