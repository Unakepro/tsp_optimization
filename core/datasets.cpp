#include "datasets.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "config.hpp"

namespace {

bool valid_group(const std::string& group) {
    return group == "small" || group == "medium" || group == "large" || group == "huge";
}

std::unordered_map<std::string, double> load_solutions() {
    std::unordered_map<std::string, double> solutions;

    const auto solutions_path = project_root() / "tsplib" / "solutions";

    std::ifstream file(solutions_path);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open TSPLIB solutions file: " + solutions_path.string());
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string name = trim(line.substr(0, colon));
        std::istringstream value_stream(line.substr(colon + 1));

        double value = 0.0;

        if (value_stream >> value) {
            solutions[name] = value;
        }
    }

    return solutions;
}

}

bool is_dataset_group(const std::string& group) {
    return valid_group(group);
}

std::vector<Dataset> load_dataset_group(const std::string& group) {
    if (!valid_group(group)) {
        throw std::runtime_error("unknown dataset group: " + group +
                                 " (expected small, medium, large or huge)");
    }

    const auto list_path = project_root() / "benchmark_sets" / (group + ".txt");

    std::ifstream file(list_path);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open dataset group file: " + list_path.string());
    }

    std::vector<Dataset> datasets;
    std::string line;
    while (std::getline(file, line)) {
        const std::string name = trim(line);
        if (name.empty() || name[0] == '#') {
            continue;
        }
        const auto path = project_root() / "tsplib" / "tests" / (name + ".tsp");
        datasets.push_back({name, path.string(), group});
    }

    if (datasets.empty()) {
        throw std::runtime_error("dataset group is empty: " + list_path.string());
    }

    return datasets;
}

double best_known_for(const std::string& instance_name) {
    static const auto solutions = load_solutions();
    const auto found = solutions.find(instance_name);

    return found == solutions.end() ? 0.0 : found->second;
}
