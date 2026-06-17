#include "hyperparameter_search.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../aco/aco.hpp"
#include "../Cities/city.hpp"
#include "../genetic/genetic.hpp"
#include "../sa/sim_an.hpp"

namespace {

inline constexpr std::uint32_t SA_SEARCH_ID = 0x5A500001u;
inline constexpr std::uint32_t GA_SEARCH_ID = 0x6A500001u;
inline constexpr std::uint32_t ACO_SEARCH_ID = 0xAC050001u;
inline constexpr std::uint32_t PARAM_SEED_ID = 0xC0FFEE01u;
const std::filesystem::path SEARCH_RESULTS_DIR = "search_results";

#ifndef TSP_PROJECT_ROOT
#define TSP_PROJECT_ROOT "."
#endif

struct SearchStats {
    double best_cost = std::numeric_limits<double>::max();
    double mean_cost = 0.0;
    double stddev_cost = 0.0;
    double mean_time = 0.0;
    double best_known = 0.0;
    double gap_percent = 0.0;
    std::uint32_t first_run_seed = 0;
};

struct SearchTrial {
    std::size_t index = 0;
    std::uint32_t param_seed = 0;
    std::string parameter_values;
    std::string log_parameters;
    std::function<void(std::vector<City>&)> solve;
};

std::filesystem::path project_root() {
    return std::filesystem::path(TSP_PROJECT_ROOT);
}

std::string tsplib_test_file(const std::string& filename) {
    return (project_root() / "tsplib" / "tests" / filename).string();
}

std::string trim_text(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string dataset_name(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

std::string dataset_filename(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::unordered_map<std::string, double> load_best_known_solutions(const std::filesystem::path& filename) {
    std::unordered_map<std::string, double> solutions;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return solutions;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        const std::string name = trim_text(line.substr(0, colon_pos));
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
    static const auto solutions = load_best_known_solutions(project_root() / "tsplib" / "solutions");
    const auto found = solutions.find(dataset_name(dataset));
    return found == solutions.end() ? 0.0 : found->second;
}

std::uint32_t derive_parameter_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, std::size_t dataset_index, std::size_t trial_index) {
    return derive_run_seed(base_seed, algorithm_id ^ PARAM_SEED_ID, dataset_index, trial_index);
}

std::uint32_t derive_search_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, std::size_t dataset_index, std::size_t trial_index, std::size_t repeat_index) {
    const std::uint32_t trial_seed = derive_run_seed(base_seed, algorithm_id, dataset_index, trial_index);
    return derive_run_seed(trial_seed, algorithm_id ^ PARAM_SEED_ID, repeat_index, 0);
}

SearchStats run_parameter_set(const std::string& file,
                              const HyperparameterSearchConfig& config,
                              std::uint32_t algorithm_id,
                              std::size_t dataset_index,
                              std::size_t trial_index,
                              const std::function<void(std::vector<City>&)>& solver) {
    std::vector<double> costs;
    costs.reserve(static_cast<std::size_t>(config.repeats));

    double time_sum = 0.0;

    for (int repeat = 0; repeat < config.repeats; ++repeat) {
        const auto run_seed = derive_search_seed(config.base_seed, algorithm_id, dataset_index, trial_index, static_cast<std::size_t>(repeat));
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

int scheduled_sa_iterations(int start_temp, double end_temp, double alpha, int max_steps) {
    if (max_steps <= 0) {
        throw std::invalid_argument("max steps must be positive");
    }

    int iterations = 0;
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

std::string sa_values(int temperature, double cooling, int iterations) {
    std::ostringstream values;
    values << temperature << "," << cooling << "," << iterations;
    return values.str();
}

std::string ga_values(int population, double mutation, int generations) {
    std::ostringstream values;
    values << population << "," << mutation << "," << generations;
    return values.str();
}

std::string aco_values(int ants, double alpha, double beta, double evaporation, int iterations) {
    std::ostringstream values;
    values << ants << "," << alpha << "," << beta << "," << evaporation << "," << iterations;
    return values.str();
}

std::string sa_log(int temperature, double cooling) {
    std::ostringstream log;
    log << "temp=" << temperature << " cool=" << cooling;
    return log.str();
}

std::string ga_log(int population, double mutation) {
    std::ostringstream log;
    log << "pop=" << population << " mut=" << mutation;
    return log.str();
}

std::string aco_log(int ants, double alpha, double beta, double evaporation) {
    std::ostringstream log;
    log << "ants=" << ants << " alpha=" << alpha << " beta=" << beta << " evap=" << evaporation;
    return log.str();
}

std::vector<SearchTrial> random_sa_trials(const HyperparameterSearchConfig& config, std::size_t dataset_index) {
    std::vector<SearchTrial> trials;
    const int random_trials = 20;

    for (int t = 0; t < random_trials; ++t) {
        const std::size_t trial_index = static_cast<std::size_t>(t);
        const auto param_seed = derive_parameter_seed(config.base_seed, SA_SEARCH_ID, dataset_index, trial_index);
        set_random_seed(param_seed);

        std::uniform_int_distribution<> temp_dist(1000, 20000);
        std::uniform_real_distribution<> cooling_dist(0.9999, 0.99999);

        const int temperature = temp_dist(gen);
        const double cooling = cooling_dist(gen);
        const int iterations = scheduled_sa_iterations(temperature, 1e-3, cooling, config.evaluation_budget);

        trials.push_back({
            trial_index,
            param_seed,
            sa_values(temperature, cooling, iterations),
            sa_log(temperature, cooling),
            [=](std::vector<City>& cities) { sa_optimization(cities, temperature, 1e-3, cooling, iterations); }
        });
    }

    return trials;
}

std::vector<SearchTrial> grid_sa_trials(const HyperparameterSearchConfig& config, std::size_t dataset_index) {
    std::vector<SearchTrial> trials;
    std::vector<int> start_temperatures = {1000, 5000, 10000};
    std::vector<double> cooling_rates = {0.99, 0.999, 0.9999, 0.99999};
    std::size_t trial_index = 0;

    for (int temperature: start_temperatures) {
        for (double cooling: cooling_rates) {
            const auto param_seed = derive_parameter_seed(config.base_seed, SA_SEARCH_ID, dataset_index, trial_index);
            const int iterations = scheduled_sa_iterations(temperature, 1e-3, cooling, config.evaluation_budget);

            trials.push_back({
                trial_index,
                param_seed,
                sa_values(temperature, cooling, iterations),
                sa_log(temperature, cooling),
                [=](std::vector<City>& cities) { sa_optimization(cities, temperature, 1e-3, cooling, iterations); }
            });

            ++trial_index;
        }
    }

    return trials;
}

std::vector<SearchTrial> random_genetic_trials(const HyperparameterSearchConfig& config, std::size_t dataset_index) {
    std::vector<SearchTrial> trials;
    const int random_trials = 20;

    for (int t = 0; t < random_trials; ++t) {
        const std::size_t trial_index = static_cast<std::size_t>(t);
        const auto param_seed = derive_parameter_seed(config.base_seed, GA_SEARCH_ID, dataset_index, trial_index);
        set_random_seed(param_seed);

        std::uniform_int_distribution<> pop_dist(10, 100);
        std::uniform_real_distribution<> mutation_dist(0.01, 0.1);

        const int population = pop_dist(gen);
        const double mutation = mutation_dist(gen);
        const int generations = budgeted_iterations(config, population);

        trials.push_back({
            trial_index,
            param_seed,
            ga_values(population, mutation, generations),
            ga_log(population, mutation),
            [=](std::vector<City>& cities) { genetic_optimization(cities, mutation, population, generations); }
        });
    }

    return trials;
}

std::vector<SearchTrial> grid_genetic_trials(const HyperparameterSearchConfig& config, std::size_t dataset_index) {
    std::vector<SearchTrial> trials;
    std::vector<int> populations = {10, 50, 100};
    std::vector<double> mutation_rates = {0.01, 0.05, 0.1};
    std::size_t trial_index = 0;

    for (int population: populations) {
        for (double mutation: mutation_rates) {
            const auto param_seed = derive_parameter_seed(config.base_seed, GA_SEARCH_ID, dataset_index, trial_index);
            const int generations = budgeted_iterations(config, population);

            trials.push_back({
                trial_index,
                param_seed,
                ga_values(population, mutation, generations),
                ga_log(population, mutation),
                [=](std::vector<City>& cities) { genetic_optimization(cities, mutation, population, generations); }
            });

            ++trial_index;
        }
    }

    return trials;
}

std::vector<SearchTrial> random_aco_trials(const HyperparameterSearchConfig& config, std::size_t dataset_index) {
    std::vector<SearchTrial> trials;
    const int random_trials = 10;

    for (int t = 0; t < random_trials; ++t) {
        const std::size_t trial_index = static_cast<std::size_t>(t);
        const auto param_seed = derive_parameter_seed(config.base_seed, ACO_SEARCH_ID, dataset_index, trial_index);
        set_random_seed(param_seed);

        std::uniform_int_distribution<> ants_dist(10, 40);
        std::uniform_real_distribution<> alpha_dist(1.0, 2.0);
        std::uniform_real_distribution<> beta_dist(3.0, 7.0);
        std::uniform_real_distribution<> evap_dist(0.1, 0.5);

        const int ants = ants_dist(gen);
        const double alpha = alpha_dist(gen);
        const double beta = beta_dist(gen);
        const double evaporation = evap_dist(gen);
        const int iterations = budgeted_iterations(config, ants);

        trials.push_back({
            trial_index,
            param_seed,
            aco_values(ants, alpha, beta, evaporation, iterations),
            aco_log(ants, alpha, beta, evaporation),
            [=](std::vector<City>& cities) { aco(cities, ants, alpha, beta, evaporation, iterations); }
        });
    }

    return trials;
}

std::vector<SearchTrial> grid_aco_trials(const HyperparameterSearchConfig& config, std::size_t dataset_index) {
    std::vector<SearchTrial> trials;
    std::vector<int> ants_list = {10, 20, 40};
    std::vector<double> alpha_list = {1.0, 1.5};
    std::vector<double> beta_list = {3.0, 5.0};
    std::vector<double> evap_list = {0.1, 0.3, 0.5};
    std::size_t trial_index = 0;

    for (int ants: ants_list) {
        for (double alpha: alpha_list) {
            for (double beta: beta_list) {
                for (double evaporation: evap_list) {
                    const auto param_seed = derive_parameter_seed(config.base_seed, ACO_SEARCH_ID, dataset_index, trial_index);
                    const int iterations = budgeted_iterations(config, ants);

                    trials.push_back({
                        trial_index,
                        param_seed,
                        aco_values(ants, alpha, beta, evaporation, iterations),
                        aco_log(ants, alpha, beta, evaporation),
                        [=](std::vector<City>& cities) { aco(cities, ants, alpha, beta, evaporation, iterations); }
                    });

                    ++trial_index;
                }
            }
        }
    }

    return trials;
}

template <typename TrialFactory>
void run_search(const std::vector<std::string>& datasets,
                const HyperparameterSearchConfig& config,
                const std::string& output_file,
                const std::string& algorithm,
                const std::string& search_type,
                std::uint32_t algorithm_id,
                const std::string& parameter_header,
                TrialFactory make_trials) {
    std::ofstream out = open_search_results_file(output_file);
    out << "algorithm,dataset,search_type,trial,base_seed,param_seed,first_run_seed,runs,evaluation_budget,"
        << parameter_header << ",best_cost,mean_cost,stddev_cost,mean_time_sec,best_known,gap_percent\n";

    for (std::size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& file = datasets[dataset_index];
        const std::string name = dataset_filename(file);
        const auto trials = make_trials(config, dataset_index);

        for (const auto& trial: trials) {
            const auto stats = run_parameter_set(file, config, algorithm_id, dataset_index, trial.index, trial.solve);

            out << algorithm << "," << name << "," << search_type << "," << trial.index << "," << config.base_seed
                << "," << trial.param_seed << "," << stats.first_run_seed << "," << config.repeats
                << "," << config.evaluation_budget << "," << trial.parameter_values << ",";
            write_stats(out, stats);

            std::cout << "[" << algorithm << "] " << name << " " << trial.log_parameters
                      << " -> best=" << stats.best_cost << ", mean=" << stats.mean_cost
                      << ", gap=" << stats.gap_percent << "%\n";
        }
    }
}

void random_search_sa(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    run_search(datasets, config, "sa_random_search.csv", "SA", "random", SA_SEARCH_ID, "temperature,cooling,iterations", random_sa_trials);
}

void grid_search_sa(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    run_search(datasets, config, "sa_grid_search.csv", "SA", "grid", SA_SEARCH_ID, "temperature,cooling,iterations", grid_sa_trials);
}

void random_search_genetic(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    run_search(datasets, config, "ga_random_search.csv", "GA", "random", GA_SEARCH_ID, "population,mutation,generations", random_genetic_trials);
}

void grid_search_genetic(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    run_search(datasets, config, "ga_grid_search.csv", "GA", "grid", GA_SEARCH_ID, "population,mutation,generations", grid_genetic_trials);
}

void random_search_aco(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    run_search(datasets, config, "aco_random_search.csv", "ACO", "random", ACO_SEARCH_ID, "ants,alpha,beta,evaporation,iterations", random_aco_trials);
}

void grid_search_aco(const std::vector<std::string>& datasets, const HyperparameterSearchConfig& config) {
    run_search(datasets, config, "aco_grid_search.csv", "ACO", "grid", ACO_SEARCH_ID, "ants,alpha,beta,evaporation,iterations", grid_aco_trials);
}

std::uint32_t parse_seed_argument(const std::string& text) {
    std::size_t parsed_chars = 0;
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
    std::size_t parsed_chars = 0;
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
    std::size_t parsed_chars = 0;
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
        tsplib_test_file("eil51.tsp"),
        tsplib_test_file("st70.tsp"),
        tsplib_test_file("pr144.tsp")
    };
}

std::vector<std::string> final_benchmark_datasets() {
    return {
        tsplib_test_file("berlin52.tsp"),
        tsplib_test_file("eil76.tsp"),
        tsplib_test_file("kroA100.tsp"),
        tsplib_test_file("kroB100.tsp"),
        tsplib_test_file("ch130.tsp")
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
