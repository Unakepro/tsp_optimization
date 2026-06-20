#include "tsp.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

std::string trim_line(const std::string& text) {
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
        if (trim_line(line.substr(0, colon_pos)) != key) {
            return false;
        }
        value = trim_line(line.substr(colon_pos + 1));
        return true;
    }

    std::istringstream iss(line);
    std::string lhs;
    iss >> lhs;
    if (lhs != key) {
        return false;
    }

    std::getline(iss, value);
    value = trim_line(value);

    return true;
}

}

std::mt19937 gen(DEFAULT_RANDOM_SEED);

void set_random_seed(std::uint32_t seed) {
    gen.seed(seed);
}

std::uint32_t derive_run_seed(std::uint32_t base_seed, std::uint32_t algorithm_id,
                              std::size_t dataset_index, std::size_t repeat_index) {
    std::uint32_t seed = base_seed;

    seed ^= algorithm_id + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint32_t>(dataset_index) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::uint32_t>(repeat_index) + 0x9e3779b9u + (seed << 6) + (seed >> 2);

    return seed;
}

int tsplib_distance(const City& a, const City& b) {
    const double dx = a.point.first - b.point.first;
    const double dy = a.point.second - b.point.second;

    return static_cast<int>(std::floor(std::sqrt(dx * dx + dy * dy) + 0.5));
}

void validate_tour_input(const std::vector<City>& cities, const std::string& algorithm_name) {
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
        validate_tour_input(cities, "Tour");
    }
    catch (const std::invalid_argument&) {
        return false;
    }

    return true;
}

std::vector<double> build_distance_matrix(const std::vector<City>& cities) {
    validate_tour_input(cities, "Distance matrix");

    const std::size_t n = cities.size();

    std::vector<double> distance_matrix(n * n, 0.0);
    for (const auto& from: cities) {
        const auto from_index = static_cast<std::size_t>(from.id - 1);

        for (const auto& to: cities) {
            const auto to_index = static_cast<std::size_t>(to.id - 1);
            if (from_index != to_index) {
                distance_matrix[matrix_index(from_index, to_index, n)] = tsplib_distance(from, to);
            }
        }
    }

    return distance_matrix;
}

double total_cost(const std::vector<City>& cities) {
    if (cities.size() < 2) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 1; i < cities.size(); ++i) {
        total += tsplib_distance(cities[i - 1], cities[i]);
    }

    total += tsplib_distance(cities.back(), cities.front());

    return total;
}

double total_cost(const std::vector<City>& cities, const std::vector<double>& distance_matrix) {
    const std::size_t n = cities.size();

    if (n < 2) {
        return 0.0;
    }
    if (distance_matrix.size() != n * n) {
        throw std::invalid_argument("Distance matrix size does not match tour size.");
    }
    validate_tour_input(cities, "Matrix tour cost");

    return total_cost_unchecked(cities, distance_matrix);
}

double total_cost_unchecked(const std::vector<City>& cities, const std::vector<double>& distance_matrix) {
    const std::size_t n = cities.size();

    if (n < 2) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
        const auto previous_id = static_cast<std::size_t>(cities[i - 1].id - 1);
        const auto current_id = static_cast<std::size_t>(cities[i].id - 1);
        total += distance_matrix[matrix_index(previous_id, current_id, n)];
    }

    const auto last_id = static_cast<std::size_t>(cities[n - 1].id - 1);
    const auto first_id = static_cast<std::size_t>(cities[0].id - 1);

    total += distance_matrix[matrix_index(last_id, first_id, n)];

    return total;
}

std::vector<std::vector<std::size_t>> build_neighbor_lists(const std::vector<double>& distance_matrix, std::size_t n, std::size_t k) {
    if (distance_matrix.size() != n * n) {
        throw std::invalid_argument("build_neighbor_lists: distance matrix size does not match city count.");
    }

    std::vector<std::vector<std::size_t>> neighbors(n);
    if (n < 2) {
        return neighbors;
    }

    const std::size_t limit = std::min(k, n - 1);

    for (std::size_t city = 0; city < n; ++city) {
        std::vector<std::size_t> others;

        others.reserve(n - 1);

        for (std::size_t other = 0; other < n; ++other) {
            if (other != city) {
                others.push_back(other);
            }
        }

        std::partial_sort(others.begin(), others.begin() + static_cast<std::ptrdiff_t>(limit), others.end(),
                          [&](std::size_t lhs, std::size_t rhs) {
            return distance_matrix[matrix_index(city, lhs, n)] < distance_matrix[matrix_index(city, rhs, n)];
        });

        others.resize(limit);
        neighbors[city] = std::move(others);
    }

    return neighbors;
}

std::size_t two_opt_neighbors(std::vector<City>& path, const std::vector<double>& distance_matrix,
                              const std::vector<std::vector<std::size_t>>& neighbors, std::size_t max_moves,
                              const RunController* controller) {
    validate_tour_input(path, "Two-opt");

    return two_opt_neighbors_unchecked(path, distance_matrix, neighbors, max_moves, controller);
}

std::size_t two_opt_neighbors_unchecked(std::vector<City>& path, const std::vector<double>& distance_matrix,
                                        const std::vector<std::vector<std::size_t>>& neighbors,
                                        std::size_t max_moves, const RunController* controller) {
    const std::size_t n = path.size();

    if (distance_matrix.size() != n * n || neighbors.size() != n) {
        throw std::invalid_argument("two_opt_neighbors: matrix or neighbor-list size does not match tour size.");
    }
    if (n < 4 || max_moves == 0) {
        return 0;
    }

    auto dist = [&](std::size_t a, std::size_t b) -> double {
        return distance_matrix[matrix_index(a, b, n)];
    };

    std::vector<std::size_t> pos(n);
    for (std::size_t i = 0; i < n; ++i) {
        pos[static_cast<std::size_t>(path[i].id - 1)] = i;
    }

    auto reverse_arc = [&](std::size_t i, std::size_t j) {
        std::size_t len = (j >= i) ? (j - i + 1) : (n - i + j + 1);
        if (len > n - len) {
            const std::size_t ni = (j + 1) % n;
            const std::size_t nj = (i + n - 1) % n;
            i = ni;
            j = nj;
            len = n - len;
        }
        std::size_t a = i;
        std::size_t b = j;
        for (std::size_t s = 0; s < len / 2; ++s) {
            std::swap(path[a], path[b]);
            pos[static_cast<std::size_t>(path[a].id - 1)] = a;
            pos[static_cast<std::size_t>(path[b].id - 1)] = b;
            a = (a + 1) % n;
            b = (b + n - 1) % n;
        }
    };

    std::queue<std::size_t> active;

    std::vector<char> queued(n, 1);
    for (std::size_t i = 0; i < n; ++i) {
        active.push(static_cast<std::size_t>(path[i].id - 1));
    }

    std::size_t moves = 0;
    constexpr double eps = 1e-9;

    while (!active.empty() && moves < max_moves && !(controller && controller->time_expired())) {
        const std::size_t c1 = active.front();
        active.pop();
        queued[c1] = 0;

        bool improved = false;
        for (int dir = 0; dir < 2 && !improved && !(controller && controller->time_expired()); ++dir) {
            const std::size_t p1 = pos[c1];
            const std::size_t p2 = (dir == 0) ? (p1 + 1) % n : (p1 + n - 1) % n;
            const std::size_t c2 = static_cast<std::size_t>(path[p2].id - 1);
            const double d_c1c2 = dist(c1, c2);

            for (const std::size_t c3: neighbors[c1]) {
                if (controller && controller->time_expired()) {
                    break;
                }

                const double d_c1c3 = dist(c1, c3);
                if (d_c1c3 >= d_c1c2) {
                    break;
                }

                const std::size_t p3 = pos[c3];
                const std::size_t p4 = (dir == 0) ? (p3 + 1) % n : (p3 + n - 1) % n;
                const std::size_t c4 = static_cast<std::size_t>(path[p4].id - 1);

                if (c4 == c1) {
                    continue;
                }

                const double gain = d_c1c2 + dist(c3, c4) - d_c1c3 - dist(c2, c4);

                if (gain > eps) {
                    if (dir == 0) {
                        reverse_arc(p2, p3);
                    } else {
                        reverse_arc(p3, p2);
                    }
                    for (const std::size_t c: {c1, c2, c3, c4}) {
                        if (!queued[c]) {
                            queued[c] = 1;
                            active.push(c);
                        }
                    }
                    ++moves;
                    improved = true;
                    break;
                }
            }
        }
    }

    return moves;
}

void readfile(std::vector<City>& cities, const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open TSPLIB file: " + filename);
    }

    std::string line;
    bool reading_coords = false;
    bool found_coord_section = false;
    int dimension = -1;
    std::string edge_weight_type;
    std::vector<City> parsed_cities;
    std::unordered_set<int> used_ids;

    while (std::getline(file, line)) {
        const std::string stripped = trim_line(line);
        if (stripped.empty()) {
            continue;
        }
        if (stripped == "NODE_COORD_SECTION") {
            reading_coords = true;
            found_coord_section = true;
            continue;
        }
        if (stripped == "EOF") {
            break;
        }

        if (reading_coords) {
            std::istringstream iss(stripped);
            City c{};
            if (!(iss >> c.id >> c.point.first >> c.point.second)) {
                throw std::runtime_error("Invalid coordinate line in " + filename + ": " + line);
            }
            std::string trailing;
            if (iss >> trailing) {
                throw std::runtime_error("Unexpected trailing coordinate data in " + filename + ": " + line);
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
                std::size_t used = 0;
                dimension = std::stoi(value, &used);
                if (used != value.size() || dimension <= 0) {
                    throw std::invalid_argument("invalid dimension");
                }
            }
            catch (const std::exception&) {
                throw std::runtime_error("Invalid DIMENSION in " + filename + ": " + value);
            }
        }
        else if (parse_tsplib_field(line, "EDGE_WEIGHT_TYPE", value)) {
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
        throw std::runtime_error("DIMENSION mismatch in " + filename + ": expected " + std::to_string(dimension) +
                                 " cities, parsed " + std::to_string(parsed_cities.size()));
    }

    for (const auto& city: parsed_cities) {
        if (city.id < 1 || city.id > dimension) {
            throw std::runtime_error("City id out of supported range 1..DIMENSION in " + filename + ": " +
                                     std::to_string(city.id));
        }
    }

    std::sort(parsed_cities.begin(), parsed_cities.end());
    cities = std::move(parsed_cities);
}

StopCondition time_limit(double seconds) {
    StopCondition stop;
    stop.max_seconds = seconds;
    return stop;
}

StopCondition iteration_limit(std::size_t iters) {
    StopCondition stop;
    stop.max_iters = iters;
    return stop;
}

StopCondition until_stable(std::size_t min_iters, std::size_t stable_window, double improvement_eps,
                           double plateau_seconds, std::size_t safety_iters) {
    StopCondition stop;

    stop.min_iters = min_iters;
    stop.stable_window = stable_window;
    stop.improvement_eps = improvement_eps;
    stop.max_seconds = std::numeric_limits<double>::infinity();
    stop.plateau_seconds = plateau_seconds;
    stop.max_iters = safety_iters;

    return stop;
}

RunController::RunController(const StopCondition& stop) : stop_(stop) {}

void RunController::start() {
    t0_ = std::chrono::steady_clock::now();
    timer_started_ = true;
}

double RunController::elapsed() const {
    if (!timer_started_) {
        return 0.0;
    }

    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
}

bool RunController::time_expired() const {
    return elapsed() >= stop_.max_seconds;
}

bool RunController::next(double best_cost, bool stable_ready) {
    bool improved = false;

    if (!started_) {
        started_ = true;
        best_so_far_ = best_cost;
    }
    else if (best_cost + 1e-9 < best_so_far_) {
        best_so_far_ = best_cost;
        improved = true;
    }

    if (improved && plateau_timer_started_) {
        plateau_timer_started_ = false;
        stable_started_ = false;
    }

    if (time_expired()) {
        stop_reason_ = StopReason::TimeLimit;
        return false;
    }
    if (iters_ >= stop_.max_iters) {
        stop_reason_ = StopReason::IterationLimit;
        return false;
    }

    if (stable_ready && stop_.stable_window > 0 && iters_ >= stop_.min_iters) {
        if (!stable_started_) {
            stable_started_ = true;
            check_iter_ = iters_;
            check_cost_ = best_so_far_;
        }
        else if (iters_ >= check_iter_ + stop_.stable_window) {
            const double relative_improvement = (check_cost_ - best_so_far_) / std::max(1e-12, std::fabs(check_cost_));

            if (relative_improvement < stop_.improvement_eps) {
                if (!plateau_timer_started_) {
                    plateau_timer_started_ = true;
                    plateau_t0_ = std::chrono::steady_clock::now();

                    if (stop_.plateau_seconds <= 0.0) {
                        stop_reason_ = StopReason::Stable;
                        converged_ = true;
                        return false;
                    }
                }
                else {
                    const double plateau_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - plateau_t0_).count();

                    if (plateau_elapsed >= stop_.plateau_seconds) {
                        stop_reason_ = StopReason::Stable;
                        converged_ = true;
                        return false;
                    }
                }
            }
            else {
                plateau_timer_started_ = false;
                check_iter_ = iters_;
                check_cost_ = best_so_far_;
            }
        }
    }
    ++iters_;

    return true;
}

SolveResult RunController::result(double best_cost) const {
    return {best_cost, iters_, converged_, stop_reason_};
}
