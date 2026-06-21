#include "benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../algorithms/aco.hpp"
#include "../algorithms/genetic.hpp"
#include "../algorithms/sa.hpp"
#include "../core/config.hpp"
#include "../core/datasets.hpp"
#include "../core/tsp.hpp"

namespace {

constexpr std::uint32_t SA_ID = 0x005Au;
constexpr std::uint32_t GA_ID = 0x006Au;
constexpr std::uint32_t ACO_ID = 0xA0C0u;

struct AlgorithmRunner {
    std::string name;
    std::uint32_t id;
    std::string params_text;
    std::string unit;
    std::function<SolveResult(std::vector<City>&, const StopCondition&)> solve;
};

ConfigMap load_params(const std::string& algorithm, const BenchmarkConfig& config) {
    if (config.params == "default") {
        return read_config(default_config_path(algorithm));
    }
    if (config.params == "custom") {
        return read_config(config.custom_config);
    }
    throw std::runtime_error("unknown params source: " + config.params + " (expected default or custom)");
}

std::vector<AlgorithmRunner> build_runners(const BenchmarkConfig& config) {
    const bool all = config.algorithm == "all";
    std::vector<AlgorithmRunner> runners;

    if (all || config.algorithm == "sa") {
        SaParams p = sa_params_from(load_params("sa", config));

        if (config.two_opt_override.has_value()) {
            p.two_opt = *config.two_opt_override;
        }

        runners.push_back({"SA", SA_ID, describe(p), "restart_attempt",
                           [p](std::vector<City>& c, const StopCondition& s) { return sa_solve(c, p, s); }});
    }
    if (all || config.algorithm == "ga") {
        GaParams p = ga_params_from(load_params("ga", config));

        if (config.two_opt_override.has_value()) {
            p.two_opt = *config.two_opt_override;
        }

        runners.push_back({"GA", GA_ID, describe(p), "generation",
                           [p](std::vector<City>& c, const StopCondition& s) { return ga_solve(c, p, s); }});
    }
    if (all || config.algorithm == "aco") {
        AcoParams p = aco_params_from(load_params("aco", config));

        if (config.two_opt_override.has_value()) {
            p.two_opt = *config.two_opt_override;
        }

        runners.push_back({"ACO", ACO_ID, describe(p), "epoch",
                           [p](std::vector<City>& c, const StopCondition& s) { return aco_solve(c, p, s); }});
    }

    if (runners.empty()) {
        throw std::runtime_error("unknown algorithm: " + config.algorithm + " (expected sa, ga, aco or all)");
    }

    return runners;
}

StopCondition stop_for(const BenchmarkConfig& config) {
    if (config.benchmark_mode == "timed") {
        return time_limit(config.time_limit);
    }
    if (config.benchmark_mode == "stable") {
        return until_stable(config.min_iters, config.stable_window, config.improvement_eps,
                            config.plateau_seconds, config.max_iters);
    }

    throw std::runtime_error("unknown benchmark mode: " + config.benchmark_mode + " (expected timed or stable)");
}

double mean(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

double stddev(const std::vector<double>& values, double average) {
    double variance = 0.0;

    for (double value: values) {
        variance += (value - average) * (value - average);
    }

    const std::size_t denom = values.size() > 1 ? values.size() - 1 : 1;

    return std::sqrt(variance / static_cast<double>(denom));
}

double gap_percent(double cost, double best_known) {
    return best_known > 0.0 ? 100.0 * (cost - best_known) / best_known : 0.0;
}

std::string number_token(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(6) << value;
    std::string token = text.str();

    while (!token.empty() && token.back() == '0') {
        token.pop_back();
    }

    if (!token.empty() && token.back() == '.') {
        token.pop_back();
    }

    std::replace(token.begin(), token.end(), '.', 'p');

    return token;
}

std::string filename_token(const std::string& text) {
    std::string token;

    for (char ch: text) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
            token.push_back(ch);
        }
        else if (ch == '.') {
            token.push_back('p');
        }
        else if (!token.empty() && token.back() != '_') {
            token.push_back('_');
        }
    }

    while (!token.empty() && token.back() == '_') {
        token.pop_back();
    }

    return token.empty() ? "run" : token;
}

std::string output_label(const BenchmarkConfig& config) {
    if (!config.label.empty()) {
        return filename_token(config.label);
    }
    if (config.params == "custom" && !config.custom_config.empty()) {
        return filename_token(config.custom_config.stem().string());
    }

    return "";
}

std::string output_filename(const BenchmarkConfig& config) {
    std::string name = "benchmark_" + config.benchmark_mode + "_" + config.group +
                       "_" + config.algorithm + "_" + config.params +
                       "_seed" + std::to_string(config.seed);
    const std::string label = output_label(config);

    if (!label.empty()) {
        name += "_" + label;
    }
    if (config.benchmark_mode == "timed") {
        name += "_" + number_token(config.time_limit) + "s";
    }
    else {
        name += "_min" + std::to_string(config.min_iters) +
                "_window" + std::to_string(config.stable_window) +
                "_eps" + number_token(config.improvement_eps) +
                "_plateau" + number_token(config.plateau_seconds) + "s" +
                "_iters" + std::to_string(config.max_iters);
    }

    return name + ".csv";
}

std::size_t sa_progress_interval(const std::string& group) {
    if (group == "huge") {
        return 1;
    }
    if (group == "large") {
        return 5;
    }
    if (group == "medium") {
        return 25;
    }

    return 50;
}

}

void run_benchmark(const BenchmarkConfig& config) {
    if (config.repeats <= 0) {
        throw std::invalid_argument("repeats must be positive");
    }
    if (config.benchmark_mode == "timed" && !(config.time_limit > 0.0)) {
        throw std::invalid_argument("time limit must be positive");
    }
    if (config.benchmark_mode == "stable") {
        if (config.min_iters == 0) {
            throw std::invalid_argument("min-iters must be positive");
        }
        if (config.stable_window == 0) {
            throw std::invalid_argument("window must be positive");
        }
        if (!(config.improvement_eps > 0.0) || !std::isfinite(config.improvement_eps)) {
            throw std::invalid_argument("epsilon must be finite and positive");
        }
        if (!(config.plateau_seconds > 0.0) || config.max_iters == 0) {
            throw std::invalid_argument("stable mode requires positive plateau-time and max-iters");
        }
    }
    if (config.params == "custom") {
        if (config.algorithm == "all") {
            throw std::invalid_argument("custom params require a single --algorithm, not 'all'");
        }
        if (!config_exists(config.custom_config)) {
            throw std::invalid_argument("config file not found: " + config.custom_config.string());
        }
    }

    const auto datasets = load_dataset_group(config.group);
    const auto runners = build_runners(config);
    const StopCondition base_stop = stop_for(config);

    const std::string output_name = output_filename(config);
    std::ofstream out = open_output_file("results", output_name);
    out << "algorithm,size_class,dataset,n,seed,repeats,parameters,unit,best_cost,mean_cost,stddev_cost,"
        << "best_known,best_gap_percent,mean_gap_percent,mean_time_sec,mean_units,"
        << "stable_runs,time_limit_runs,iteration_limit_runs\n";

    std::cout << "Benchmark mode=" << config.benchmark_mode << " group=" << config.group
              << " algorithm=" << config.algorithm << " params=" << config.params << " seed=" << config.seed
              << " repeats=" << config.repeats;

    const std::string label = output_label(config);

    if (!label.empty()) {
        std::cout << " label=" << label;
    }
    if (config.benchmark_mode == "timed") {
        std::cout << " time_limit=" << config.time_limit << "s";
    } else {
        std::cout << " min_iters=" << config.min_iters
                  << " window=" << config.stable_window
                  << " epsilon=" << config.improvement_eps
                  << " plateau_time=" << config.plateau_seconds << "s";
    }
    std::cout << "\n";

    for (std::size_t dataset_index = 0; dataset_index < datasets.size(); ++dataset_index) {
        const auto& dataset = datasets[dataset_index];
        const double best_known = best_known_for(dataset.name);

        for (const auto& runner: runners) {
            std::vector<double> costs;
            std::vector<double> times;
            std::vector<double> units;
            int stable_runs = 0;
            int time_limit_runs = 0;
            int iteration_limit_runs = 0;
            std::size_t n = 0;

            for (int repeat = 0; repeat < config.repeats; ++repeat) {
                set_random_seed(derive_run_seed(config.seed, runner.id, dataset_index, static_cast<std::size_t>(repeat)));

                std::vector<City> cities;
                readfile(cities, dataset.path);
                n = cities.size();

                StopCondition stop = base_stop;
                if (runner.name == "SA") {
                    stop.progress_interval = sa_progress_interval(config.group);
                    stop.progress_callback = [&](std::size_t restarts_done, double best_cost) {
                        std::cout << "    [SA] " << dataset.name << " repeat " << (repeat + 1)
                                  << "/" << config.repeats << " restarts=" << restarts_done
                                  << " best=" << best_cost << "\n" << std::flush;
                    };
                }

                std::cout << "  [" << runner.name << "] " << dataset.name << " repeat "
                          << (repeat + 1) << "/" << config.repeats << " started\n" << std::flush;

                const auto start = std::chrono::steady_clock::now();
                const SolveResult result = runner.solve(cities, stop);
                const auto end = std::chrono::steady_clock::now();
                if (runner.name == "SA" && result.stop_reason == StopReason::TimeLimit && result.restarts == 0) {
                    std::cout << "    [SA] warning: time limit expired before one full annealing restart completed\n"
                              << std::flush;
                }

                validate_tour_input(cities, runner.name + " benchmark result");
                costs.push_back(total_cost(cities));
                times.push_back(std::chrono::duration<double>(end - start).count());
                units.push_back(static_cast<double>(result.iterations));
                stable_runs += result.stop_reason == StopReason::Stable ? 1 : 0;
                time_limit_runs += result.stop_reason == StopReason::TimeLimit ? 1 : 0;
                iteration_limit_runs += result.stop_reason == StopReason::IterationLimit ? 1 : 0;
            }

            const double best_cost = *std::min_element(costs.begin(), costs.end());
            const double mean_cost = mean(costs);
            const double best_gap = gap_percent(best_cost, best_known);
            const double mean_gap = gap_percent(mean_cost, best_known);

            out << runner.name << "," << dataset.size_class << "," << dataset.name << "," << n << ","
                << config.seed << "," << config.repeats << "," << runner.params_text << "," << runner.unit << ","
                << best_cost << "," << mean_cost << "," << stddev(costs, mean_cost) << ",";
            if (best_known > 0.0) {
                out << best_known << "," << best_gap << "," << mean_gap;
            } else {
                out << ",,";
            }
            out << "," << mean(times) << "," << mean(units) << ","
                << stable_runs << "," << time_limit_runs << "," << iteration_limit_runs << "\n";

            std::cout << "  [" << runner.name << "] " << dataset.name << " (n=" << n << ") best=" << best_cost
                      << " mean=" << mean_cost << " best_gap=";
            if (best_known > 0.0) {
                std::cout << best_gap << "%";
            }
            else {
                std::cout << "n/a";
            }

            std::cout << " time=" << mean(times) << "s";

            if (runner.name == "SA") {
                std::cout << " restart_attempts=" << mean(units);
            }
            if (config.benchmark_mode == "stable") {
                if (runner.name != "SA") {
                    std::cout << " " << runner.unit << "s=" << mean(units);
                }
                std::cout << " stable=" << stable_runs << "/" << config.repeats
                          << " time_limit=" << time_limit_runs << "/" << config.repeats;
            }

            std::cout << "\n";
        }
    }

    std::cout << "Wrote results/" << output_name << "\n";
}
