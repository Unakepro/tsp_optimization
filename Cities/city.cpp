#include "city.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parse_tsplib_field(const std::string& line, const std::string& key, std::string& value) {
    const auto colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
        const std::string lhs = trim(line.substr(0, colon_pos));
        if (lhs != key) {
            return false;
        }

        value = trim(line.substr(colon_pos + 1));
        return true;
    }

    std::istringstream iss(line);
    std::string lhs;
    iss >> lhs;
    if (lhs != key) {
        return false;
    }

    std::getline(iss, value);
    value = trim(value);
    return true;
}

}

std::mt19937 gen(DEFAULT_RANDOM_SEED);

void set_random_seed(std::uint32_t seed) {
    gen.seed(seed);
}

std::uint32_t derive_run_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, std::size_t dataset_index, std::size_t repeat_index) {
    std::uint32_t seed = base_seed;
    seed ^= algorithm_id + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint32_t>(dataset_index) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint32_t>(repeat_index) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

int tsplibEuc2dDistance(const City& a, const City& b) {
    const double dx = a.point.first - b.point.first;
    const double dy = a.point.second - b.point.second;
    const double distance = std::sqrt(dx * dx + dy * dy);

    return static_cast<int>(std::floor(distance + 0.5));
}

double euclideanDistance(const City& a, const City& b) {
    return tsplibEuc2dDistance(a, b);
}

void validate_tsp_input(const std::vector<City>& cities, const std::string& algorithm_name) {
    if (cities.size() < 2) {
        throw std::invalid_argument(algorithm_name + " requires at least two cities.");
    }

    std::vector<bool> used_ids(cities.size(), false);
    for (const auto& city: cities) {
        if (city.id <= 0 || static_cast<std::size_t>(city.id) > cities.size()) {
            throw std::invalid_argument(algorithm_name + " requires city ids in range 1..number of cities.");
        }
        if (used_ids[static_cast<std::size_t>(city.id - 1)]) {
            throw std::invalid_argument(algorithm_name + " requires unique city ids.");
        }
        if (!std::isfinite(city.point.first) || !std::isfinite(city.point.second)) {
            throw std::invalid_argument(algorithm_name + " requires finite city coordinates.");
        }

        used_ids[static_cast<std::size_t>(city.id - 1)] = true;
    }
}

bool is_valid_tour(const std::vector<City>& cities) {
    try {
        validate_tsp_input(cities, "Tour");
    } catch (const std::invalid_argument&) {
        return false;
    }

    return true;
}

std::vector<double> build_distance_matrix(const std::vector<City>& cities) {
    validate_tsp_input(cities, "Distance matrix");

    const std::size_t n = cities.size();
    std::vector<double> distance_matrix(n * n, 0.0);

    for (const auto& from: cities) {
        const auto from_index = static_cast<std::size_t>(from.id - 1);
        for (const auto& to: cities) {
            const auto to_index = static_cast<std::size_t>(to.id - 1);
            if (from_index != to_index) {
                distance_matrix[matrix_index(from_index, to_index, n)] = euclideanDistance(from, to);
            }
        }
    }

    return distance_matrix;
}

double total_cost(const std::vector<City>& cities) {
    if (cities.size() < 2) {
        return 0.0;
    }

    double total_distance = 0;

    for (std::size_t i = 1; i < cities.size(); ++i) {
        total_distance += euclideanDistance(cities[i - 1], cities[i]);
    }

    total_distance += euclideanDistance(cities[cities.size() - 1], cities[0]);

    return total_distance;
}

double total_cost(const std::vector<City>& cities, const std::vector<double>& distance_matrix) {
    const std::size_t n = cities.size();
    if (n < 2) {
        return 0.0;
    }
    if (distance_matrix.size() != n * n) {
        throw std::invalid_argument("Distance matrix size does not match tour size.");
    }

    double total_distance = 0.0;

    for (std::size_t i = 1; i < n; ++i) {
        const auto previous_id = static_cast<std::size_t>(cities[i - 1].id - 1);
        const auto current_id = static_cast<std::size_t>(cities[i].id - 1);
        total_distance += distance_matrix[matrix_index(previous_id, current_id, n)];
    }

    const auto last_id = static_cast<std::size_t>(cities[n - 1].id - 1);
    const auto first_id = static_cast<std::size_t>(cities[0].id - 1);
    total_distance += distance_matrix[matrix_index(last_id, first_id, n)];

    return total_distance;
}

void apply_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix) {
    const std::size_t n = path.size();
    if (n < 4) {
        return;
    }
    if (distance_matrix.size() != n * n) {
        throw std::invalid_argument("Distance matrix size does not match tour size.");
    }

    bool improved = true;

    while (improved) {
        improved = false;

        for (std::size_t i = 1; i < n - 1; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const std::size_t next_j = (j + 1) % n;

                const auto a = static_cast<std::size_t>(path[i - 1].id - 1);
                const auto b = static_cast<std::size_t>(path[i].id - 1);
                const auto c = static_cast<std::size_t>(path[j].id - 1);
                const auto d = static_cast<std::size_t>(path[next_j].id - 1);

                double old_cost = distance_matrix[matrix_index(a, b, n)] +
                                  distance_matrix[matrix_index(c, d, n)];

                double new_cost = distance_matrix[matrix_index(a, c, n)] +
                                  distance_matrix[matrix_index(b, d, n)];

                if (new_cost < old_cost) {
                    std::reverse(path.begin() + i, path.begin() + j + 1);
                    improved = true;
                }
            }
        }
    }
}

void readfile(std::vector<City>& cities, const std::string& filename) {
    std::string line;
    bool reading_coords = false;
    bool found_coord_section = false;
    int dimension = -1;
    std::string edge_weight_type;
    std::vector<City> parsed_cities;
    std::unordered_set<int> used_ids;

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open TSPLIB file: " + filename);
    }

    while (std::getline(file, line)) {
        const std::string stripped_line = trim(line);
        if (stripped_line.empty()) {
            continue;
        }

        if (line.find("NODE_COORD_SECTION") != std::string::npos) {
            reading_coords = true;
            found_coord_section = true;
            continue;
        }
        if (line.find("EOF") != std::string::npos) {
            break;
        }

        if (reading_coords) {
            std::istringstream iss(line);
            City c;
            if (!(iss >> c.id >> c.point.first >> c.point.second)) {
                throw std::runtime_error("Invalid coordinate line in " + filename + ": " + line);
            }

            if (!used_ids.insert(c.id).second) {
                throw std::runtime_error("Duplicate city id in " + filename + ": " + std::to_string(c.id));
            }

            parsed_cities.push_back(c);
            continue;
        }

        std::string value;
        if (parse_tsplib_field(line, "DIMENSION", value)) {
            try {
                std::size_t parsed_chars = 0;
                dimension = std::stoi(value, &parsed_chars);
                if (parsed_chars != value.size() || dimension <= 0) {
                    throw std::invalid_argument("invalid dimension");
                }
            } catch (const std::exception&) {
                throw std::runtime_error("Invalid DIMENSION in " + filename + ": " + value);
            }
        } else if (parse_tsplib_field(line, "EDGE_WEIGHT_TYPE", value)) {
            edge_weight_type = value;
        }
    }

    if (dimension <= 0) {
        throw std::runtime_error("Missing or invalid DIMENSION in TSPLIB file: " + filename);
    }
    if (edge_weight_type.empty()) {
        throw std::runtime_error("Missing EDGE_WEIGHT_TYPE in TSPLIB file: " + filename);
    }
    if (edge_weight_type != "EUC_2D") {
        throw std::runtime_error("Unsupported EDGE_WEIGHT_TYPE in " + filename + ": " + edge_weight_type);
    }
    if (!found_coord_section) {
        throw std::runtime_error("Missing NODE_COORD_SECTION in TSPLIB file: " + filename);
    }
    if (parsed_cities.size() != static_cast<std::size_t>(dimension)) {
        throw std::runtime_error("DIMENSION mismatch in " + filename + ": expected " + std::to_string(dimension) + " cities, parsed " + std::to_string(parsed_cities.size()));
    }

    for (const auto& city: parsed_cities) {
        if (city.id < 1 || city.id > dimension) {
            throw std::runtime_error("City id out of supported range 1..DIMENSION in " + filename + ": " + std::to_string(city.id));
        }
        if (!std::isfinite(city.point.first) || !std::isfinite(city.point.second)) {
            throw std::runtime_error("Non-finite city coordinate in " + filename + " for city id " + std::to_string(city.id));
        }
    }

    std::sort(parsed_cities.begin(), parsed_cities.end());

    cities = std::move(parsed_cities);
}
