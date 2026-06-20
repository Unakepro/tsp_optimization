#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

#include "benchmark/benchmark.hpp"
#include "core/datasets.hpp"

namespace {

const char* USAGE =
    "TSP Optimizer — compare Simulated Annealing, Genetic Algorithm and Ant Colony Optimization.\n"
    "\n"
    "Usage:\n"
    "  tsp_optimizer --benchmark-mode timed --set small|medium|large|huge --time-limit 10s\n"
    "                [--algorithm sa|ga|aco|all] [--params default|custom]\n"
    "                [--config FILE] [--two-opt true|false]\n"
    "                [--label NAME] [--seed N] [--repeats N]\n"
    "  tsp_optimizer --benchmark-mode stable --set small|medium|large|huge\n"
    "                [--algorithm sa|ga|aco|all] [--params default|custom]\n"
    "                [--config FILE] [--two-opt true|false]\n"
    "                [--min-iters 50] [--window 25] [--epsilon 0.0001]\n"
    "                [--plateau-time 60s] [--max-iters N]\n"
    "                (for SA, iters mean completed annealing restarts)\n"
    "                [--label NAME] [--seed N] [--repeats N]\n"
    "\n"
    "Examples:\n"
    "  tsp_optimizer --benchmark-mode timed --set medium --time-limit 10s "
    "--algorithm all --params default\n"
    "  tsp_optimizer --benchmark-mode stable --set large --algorithm all "
    "--params default\n";

using Args = std::map<std::string, std::string>;

Args parse_args(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--help" || flag == "-h") {
            args["help"] = "true";
            continue;
        }
        if (flag.rfind("--", 0) != 0) {
            throw std::invalid_argument("unexpected argument: " + flag);
        }
        if (i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0) {
            throw std::invalid_argument("missing value for " + flag);
        }
        const std::string key = flag.substr(2);
        if (args.find(key) != args.end()) {
            throw std::invalid_argument("duplicate option " + flag);
        }
        args[key] = argv[++i];
    }
    return args;
}

bool has(const Args& args, const std::string& key) {
    return args.find(key) != args.end();
}

std::string get(const Args& args, const std::string& key, const std::string& fallback) {
    const auto found = args.find(key);
    return found == args.end() ? fallback : found->second;
}

std::string require(const Args& args, const std::string& key) {
    const auto found = args.find(key);
    if (found == args.end()) {
        throw std::invalid_argument("missing required option --" + key);
    }
    return found->second;
}

void reject_unknown_args(const Args& args, std::initializer_list<const char*> allowed) {
    for (const auto& item: args) {
        const auto& key = item.first;
        bool known = false;
        for (const char* allowed_key: allowed) {
            if (key == allowed_key) {
                known = true;
                break;
            }
        }
        if (!known) {
            throw std::invalid_argument("unknown option --" + key);
        }
    }
}

int parse_int(const std::string& text, const std::string& name) {
    try {
        std::size_t used = 0;
        const long value = std::stol(text, &used);
        if (used != text.size() || value <= 0 || value > std::numeric_limits<int>::max()) {
            throw std::invalid_argument("bad");
        }
        return static_cast<int>(value);
    } catch (const std::exception&) {
        throw std::invalid_argument(name + " must be a positive integer");
    }
}

std::size_t parse_size(const std::string& text, const std::string& name) {
    return static_cast<std::size_t>(parse_int(text, name));
}

std::uint32_t parse_seed(const std::string& text) {
    try {
        std::size_t used = 0;
        const unsigned long value = std::stoul(text, &used);
        if (used != text.size() || value > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("bad");
        }
        return static_cast<std::uint32_t>(value);
    } catch (const std::exception&) {
        throw std::invalid_argument("seed must be an unsigned 32-bit integer");
    }
}

double parse_seconds(const std::string& text, const std::string& name) {
    std::string value = text;
    if (!value.empty() && (value.back() == 's' || value.back() == 'S')) {
        value.pop_back();
    }
    try {
        std::size_t used = 0;
        const double seconds = std::stod(value, &used);
        if (used != value.size() || !(seconds > 0.0)) {
            throw std::invalid_argument("bad");
        }
        return seconds;
    } catch (const std::exception&) {
        throw std::invalid_argument(name + " must be a positive number of seconds, e.g. 10s");
    }
}

double parse_positive_double(const std::string& text, const std::string& name) {
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size() || !(value > 0.0)) {
            throw std::invalid_argument("bad");
        }
        return value;
    } catch (const std::exception&) {
        throw std::invalid_argument(name + " must be a positive number");
    }
}

bool parse_bool_option(const std::string& text, const std::string& name) {
    if (text == "true" || text == "1" || text == "yes" || text == "on") {
        return true;
    }
    if (text == "false" || text == "0" || text == "no" || text == "off") {
        return false;
    }
    throw std::invalid_argument(name + " must be true or false");
}

void run_bench(const Args& args) {
    BenchmarkConfig config;
    config.benchmark_mode = require(args, "benchmark-mode");
    if (config.benchmark_mode == "timed") {
        reject_unknown_args(args, {"benchmark-mode", "set", "time-limit", "algorithm", "params",
                                   "config", "two-opt", "label", "seed", "repeats"});
    } else if (config.benchmark_mode == "stable") {
        reject_unknown_args(args, {"benchmark-mode", "set", "algorithm", "params", "config", "two-opt",
                                   "label", "seed", "repeats", "min-iters", "window", "epsilon",
                                   "plateau-time", "max-iters"});
    } else {
        throw std::invalid_argument("--benchmark-mode must be timed or stable");
    }

    config.group = require(args, "set");
    if (!is_dataset_group(config.group)) {
        throw std::invalid_argument("--set must be small, medium, large or huge");
    }
    config.algorithm = get(args, "algorithm", "all");
    config.params = get(args, "params", "default");
    config.label = get(args, "label", "");
    if (has(args, "two-opt")) {
        config.two_opt_override = parse_bool_option(require(args, "two-opt"), "--two-opt");
    }
    if (config.params == "custom") {
        config.custom_config = require(args, "config");
    } else if (has(args, "config")) {
        throw std::invalid_argument("--config is only valid with --params custom");
    }
    config.seed = parse_seed(get(args, "seed", "42"));
    config.repeats = parse_int(get(args, "repeats", config.group == "huge" ? "1" : "3"), "repeats");

    if (config.benchmark_mode == "timed") {
        config.time_limit = parse_seconds(require(args, "time-limit"), "--time-limit");
    } else {
        config.min_iters = parse_size(get(args, "min-iters", "50"), "min-iters");
        config.stable_window = parse_size(get(args, "window", "25"), "window");
        config.improvement_eps = parse_positive_double(get(args, "epsilon", "0.0001"), "epsilon");
        config.plateau_seconds = parse_seconds(get(args, "plateau-time", "60s"), "--plateau-time");
        config.max_iters = parse_size(get(args, "max-iters", "100000"), "max-iters");
    }

    run_benchmark(config);
}

}

int main(int argc, char* argv[]) {
    try {
        const Args args = parse_args(argc, argv);
        if (has(args, "help") || argc == 1) {
            std::cout << USAGE;
            return has(args, "help") ? 0 : 1;
        }

        run_bench(args);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n\n" << USAGE;
        return 1;
    }
    return 0;
}
