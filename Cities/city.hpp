#ifndef CITY
#define CITY

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

inline std::random_device rd;
inline std::mt19937 gen(rd());


struct City {
    int id;
    std::pair<double, double> point;

    
    bool operator<(const City& other) const {
        return id < other.id;
    }

    bool operator==(const City& other) const {
        return id == other.id && point == other.point;
    }

};

inline int tsplibEuc2dDistance(const City& a, const City& b) {
    const double dx = a.point.first - b.point.first;
    const double dy = a.point.second - b.point.second;
    
    const double distance = std::sqrt(dx * dx + dy * dy);
    
    return static_cast<int>(std::floor(distance + 0.5));
}

inline double euclideanDistance(const City& a, const City& b) {
    return tsplibEuc2dDistance(a, b);
}


double total_cost(const std::vector<City>& cities) {
    double total_distance = 0;
    
    for(size_t i = 1; i < cities.size(); ++i) {
        total_distance += euclideanDistance(cities[i-1], cities[i]);
    }

    total_distance += euclideanDistance(cities[cities.size()-1], cities[0]);

    return total_distance;
}

inline int idx(int i, int j, int n) {
    return i * n + j;
}

void apply_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix) {
    bool improved = true;
    size_t n = path.size();

    while (improved) {
        improved = false;
        for (size_t i = 1; i < n - 1; ++i) {
            for (size_t j = i + 1; j < n - 1; ++j) {   
                if (j - i == 1) continue;

                double d1 = distance_matrix[idx(path[i - 1].id-1, path[i].id-1, n)] + distance_matrix[idx(path[j].id-1, path[j+1].id-1, n)];
                double d2 = distance_matrix[idx(path[i - 1].id-1, path[j].id-1, n)] + distance_matrix[idx(path[i].id-1, path[j+1].id-1, n)];
                
                if (d2 < d1) {
                    std::reverse(path.begin() + i, path.begin() + j + 1);
                    improved = true;
                }
            }
        }
    }
}

inline std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

inline bool parse_tsplib_field(const std::string& line, const std::string& key, std::string& value) {
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
        if (line.find("EOF") != std::string::npos) break;

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
                size_t parsed_chars = 0;
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
    if (parsed_cities.size() != static_cast<size_t>(dimension)) {
        throw std::runtime_error("DIMENSION mismatch in " + filename + ": expected " + std::to_string(dimension) + " cities, parsed " + std::to_string(parsed_cities.size()));
    }

    for (const auto& city : parsed_cities) {
        if (city.id < 1 || city.id > dimension) {
            throw std::runtime_error("City id out of supported range 1..DIMENSION in " + filename + ": " + std::to_string(city.id));
        }
        if (!std::isfinite(city.point.first) || !std::isfinite(city.point.second)) {
            throw std::runtime_error("Non-finite city coordinate in " + filename + " for city id " + std::to_string(city.id));
        }
    }

    cities = std::move(parsed_cities);
}


#endif
