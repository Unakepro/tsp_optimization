#include "config.hpp"

#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#ifndef TSP_PROJECT_ROOT
#define TSP_PROJECT_ROOT "."
#endif

std::filesystem::path project_root() {
    if (const char* env = std::getenv("TSP_PROJECT_ROOT")) {
        if (*env != '\0') {
            return std::filesystem::path(env);
        }
    }
    return std::filesystem::path(TSP_PROJECT_ROOT);
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = text.find_last_not_of(" \t\r\n");

    return text.substr(first, last - first + 1);
}

std::string bool_text(bool value) {
    return value ? "true" : "false";
}

std::ofstream open_output_file(const std::filesystem::path& directory, const std::string& filename) {
    const auto output_dir = directory.is_absolute() ? directory : project_root() / directory;
    std::filesystem::create_directories(output_dir);

    const auto path = output_dir / filename;

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }

    out << std::setprecision(10);

    return out;
}

ConfigMap read_config(const std::filesystem::path& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open config file: " + path.string());
    }

    ConfigMap values;
    std::string line;

    while (std::getline(file, line)) {
        const std::string stripped = trim(line);

        if (stripped.empty() || stripped[0] == '#') {
            continue;
        }

        const auto equals = stripped.find('=');

        if (equals == std::string::npos) {
            throw std::runtime_error("invalid config line in " + path.string() + ": " + line);
        }
        values[trim(stripped.substr(0, equals))] = trim(stripped.substr(equals + 1));
    }

    return values;
}

bool config_exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

namespace {

const std::string& require(const ConfigMap& values, const std::string& key) {
    const auto found = values.find(key);

    if (found == values.end()) {
        throw std::runtime_error("missing config key: " + key);
    }

    return found->second;
}

}

double parse_double(const std::string& text) {
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used == text.size()) {
            return value;
        }
    }
    catch (const std::exception&) {}
    throw std::runtime_error("invalid number: '" + text + "'");
}

int parse_int(const std::string& text) {
    try {
        std::size_t used = 0;
        const long value = std::stol(text, &used);

        if (used == text.size() && value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
            return static_cast<int>(value);
        }
    }
    catch (const std::exception&) {}
    throw std::runtime_error("invalid integer: '" + text + "'");
}

double config_double(const ConfigMap& values, const std::string& key) {
    return parse_double(require(values, key));
}

int config_int(const ConfigMap& values, const std::string& key) {
    return parse_int(require(values, key));
}

bool config_bool(const ConfigMap& values, const std::string& key) {
    const std::string& text = require(values, key);

    if (text == "true" || text == "1") {
        return true;
    }
    if (text == "false" || text == "0") {
        return false;
    }

    throw std::runtime_error("invalid boolean for config key " + key + ": " + text);
}

SaParams sa_params_from(const ConfigMap& values) {
    SaParams params;

    params.start_temp = config_double(values, "start_temp");
    params.end_temp = config_double(values, "end_temp");
    params.cooling = config_double(values, "cooling");
    params.two_opt = config_bool(values, "two_opt");

    return params;
}

GaParams ga_params_from(const ConfigMap& values) {
    GaParams params;

    params.population = config_int(values, "population");
    params.mutation = config_double(values, "mutation");
    params.two_opt = config_bool(values, "two_opt");

    return params;
}

AcoParams aco_params_from(const ConfigMap& values) {
    AcoParams params;

    params.ants = config_int(values, "ants");
    params.alpha = config_double(values, "alpha");
    params.beta = config_double(values, "beta");
    params.evaporation = config_double(values, "evaporation");
    params.two_opt = config_bool(values, "two_opt");

    return params;
}

std::string describe(const SaParams& params) {
    std::ostringstream text;

    text << "start_temp=" << params.start_temp << ";end_temp=" << params.end_temp
         << ";cooling=" << params.cooling << ";two_opt=" << bool_text(params.two_opt);

    return text.str();
}

std::string describe(const GaParams& params) {
    std::ostringstream text;

    text << "population=" << params.population << ";mutation=" << params.mutation
         << ";two_opt=" << bool_text(params.two_opt);

    return text.str();
}

std::string describe(const AcoParams& params) {
    std::ostringstream text;

    text << "ants=" << params.ants << ";alpha=" << params.alpha << ";beta=" << params.beta
         << ";evaporation=" << params.evaporation << ";two_opt=" << bool_text(params.two_opt);

    return text.str();
}

std::filesystem::path default_config_path(const std::string& algorithm) {
    return project_root() / "configs" / "default" / (algorithm + ".conf");
}
