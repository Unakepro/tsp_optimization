#include "sim_an.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace {

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

std::pair<std::size_t, std::size_t> random_segment(std::size_t city_count) {
    std::uniform_int_distribution<std::size_t> dist(0, city_count - 1);

    std::size_t pos1 = dist(gen);
    std::size_t pos2 = dist(gen);

    while (pos1 == pos2) {
        pos2 = dist(gen);
    }

    if (pos1 > pos2) {
        std::swap(pos1, pos2);
    }

    return {pos1, pos2};
}

bool make_transition(long double P) {
    std::uniform_real_distribution<> dis(0.0, 1.0);
    if (P >= dis(gen)) {
        return true;
    }

    return false;
}

}

double tour_reversal_delta(const std::vector<City>& cities, const std::vector<double>& distance_matrix, std::size_t start, std::size_t end) {
    const std::size_t n = cities.size();
    if (n < 2) {
        throw std::invalid_argument("Reversal delta requires at least two cities.");
    }
    if (distance_matrix.size() != n * n) {
        throw std::invalid_argument("Distance matrix size does not match tour size.");
    }
    if (start >= n || end >= n || start > end) {
        throw std::invalid_argument("Reversal delta segment is out of range.");
    }
    if (start == 0 && end + 1 == n) {
        return 0.0;
    }

    const std::size_t before_start = start == 0 ? n - 1 : start - 1;
    const std::size_t after_end = (end + 1) % n;

    const auto a = static_cast<std::size_t>(cities[before_start].id - 1);
    const auto b = static_cast<std::size_t>(cities[start].id - 1);
    const auto c = static_cast<std::size_t>(cities[end].id - 1);
    const auto d = static_cast<std::size_t>(cities[after_end].id - 1);

    const double old_cost = distance_matrix[matrix_index(a, b, n)] + distance_matrix[matrix_index(c, d, n)];
    const double new_cost = distance_matrix[matrix_index(a, c, n)] + distance_matrix[matrix_index(b, d, n)];

    return new_cost - old_cost;
}

void validate_sa_parameters(const std::vector<City>& cities, double start_temp, double end_temp, double alpha, std::size_t steps) {
    validate_tsp_input(cities, "Simulated annealing");

    if (!std::isfinite(start_temp) || start_temp <= 0.0) {
        throw std::invalid_argument("Simulated annealing parameter start_temp must be finite and positive.");
    }
    if (!std::isfinite(end_temp) || end_temp <= 0.0) {
        throw std::invalid_argument("Simulated annealing parameter end_temp must be finite and positive.");
    }
    if (end_temp >= start_temp) {
        throw std::invalid_argument("Simulated annealing parameter end_temp must be smaller than start_temp.");
    }
    if (!std::isfinite(alpha) || alpha <= 0.0 || alpha >= 1.0) {
        throw std::invalid_argument("Simulated annealing parameter alpha must be finite and between 0 and 1.");
    }
    if (steps == 0) {
        throw std::invalid_argument("Simulated annealing parameter steps must be greater than zero.");
    }
}

void sa_optimization(std::vector<City>& cities, double start_temp, double end_temp, double alpha, std::size_t steps) {
    validate_sa_parameters(cities, start_temp, end_temp, alpha, steps);

    const std::vector<double> distance_matrix = build_distance_matrix(cities);
    std::shuffle(cities.begin(), cities.end(), gen);

    double currEnergy = total_cost(cities, distance_matrix);
    auto bestState = cities;
    double bestEnergy = currEnergy;

    double T = start_temp;

    for (std::size_t i = 0; i < steps; ++i) {
        const auto [start, end] = random_segment(cities.size());
        const double delta = tour_reversal_delta(cities, distance_matrix, start, end);
        const double newEnergy = currEnergy + delta;

        bool accepted = false;

        if (delta < 0.0) {
            accepted = true;
        } else {
            accepted = make_transition(std::exp(-delta / T));
        }

        if (accepted) {
            std::reverse(cities.begin() + static_cast<std::ptrdiff_t>(start), cities.begin() + static_cast<std::ptrdiff_t>(end + 1));
            currEnergy = newEnergy;

            if (currEnergy < bestEnergy) {
                bestEnergy = currEnergy;
                bestState = cities;
            }
        }

        T = std::max(T * alpha, end_temp);
    }

    cities = bestState;
}
