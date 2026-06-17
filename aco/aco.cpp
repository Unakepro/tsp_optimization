#include "aco.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
constexpr double Q = 100.0;

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

void generate_path(const std::vector<City>& cities, std::vector<City>& path, const std::vector<double>& old_pheromons, const std::vector<double>& eta_matrix, double alpha, double beta) {
    std::size_t n = cities.size();

    std::vector<bool> used(n, false);
    used[path[0].id - 1] = true;

    while (path.size() < n) {
        std::vector<std::pair<double, int>> city_weight;

        auto current_city = path.back();
        int current_id = current_city.id - 1;

        for (std::size_t i = 0; i < n; ++i) {
            if (used[i]) {
                continue;
            }

            double tau = old_pheromons[matrix_index(current_id, i, n)];
            double eta = eta_matrix[matrix_index(current_id, i, n)];
            double weight = std::pow(tau, alpha) * std::pow(eta, beta);

            city_weight.push_back({weight, i});
        }

        std::vector<double> weights;
        for (const auto& [weight, _]: city_weight) {
            weights.push_back(weight);
        }

        std::discrete_distribution<> weighted_dist(weights.begin(), weights.end());
        int selected_index = weighted_dist(gen);

        int next_city_index = city_weight[selected_index].second;
        path.push_back(cities[next_city_index]);
        used[next_city_index] = true;
    }
}

void precompute_matrix(const std::vector<City>& cities, std::vector<double>& distance_matrix, std::vector<double>& eta_matrix) {
    std::size_t n = cities.size();

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j) {
                double dist = euclideanDistance(cities[i], cities[j]);
                distance_matrix[matrix_index(i, j, n)] = dist;
                eta_matrix[matrix_index(i, j, n)] = 1.0 / (dist + 1e-6);
            } else {
                distance_matrix[matrix_index(i, j, n)] = 0;
                eta_matrix[matrix_index(i, j, n)] = 0;
            }
        }
    }
}

}

void validate_aco_parameters(const std::vector<City>& cities, std::size_t m, double alpha, double beta, double po, std::size_t epochs) {
    validate_tsp_input(cities, "Ant colony optimization");

    if (m == 0) {
        throw std::invalid_argument("Ant colony optimization parameter ants must be greater than zero.");
    }
    if (!std::isfinite(alpha) || alpha < 0.0) {
        throw std::invalid_argument("Ant colony optimization parameter alpha must be finite and non-negative.");
    }
    if (!std::isfinite(beta) || beta < 0.0) {
        throw std::invalid_argument("Ant colony optimization parameter beta must be finite and non-negative.");
    }
    if (!std::isfinite(po) || po < 0.0 || po >= 1.0) {
        throw std::invalid_argument("Ant colony optimization parameter evaporation must be finite and between 0 and 1.");
    }
    if (epochs == 0) {
        throw std::invalid_argument("Ant colony optimization parameter epochs must be greater than zero.");
    }
}

void aco(std::vector<City>& cities, std::size_t m, double alpha, double beta, double po, std::size_t epochs, bool use_two_opt) {
    validate_aco_parameters(cities, m, alpha, beta, po, epochs);

    std::size_t n = cities.size();

    std::vector<double> pheromons(n * n, 1);
    std::vector<double> distance_matrix(n * n);
    std::vector<double> eta_matrix(n * n);

    precompute_matrix(cities, distance_matrix, eta_matrix);

    std::vector<City> curr_best = cities;
    for (std::size_t i = 0; i < epochs; ++i) {
        std::vector<std::vector<City>> ant_pathes;

        auto tmp_pheromons = pheromons;
        for (std::size_t j = 0; j < m; ++j) {
            std::uniform_int_distribution<> dist(0, n - 1);

            ant_pathes.push_back({cities[dist(gen)]});

            generate_path(cities, ant_pathes[j], pheromons, eta_matrix, alpha, beta);
        }

        pheromons = tmp_pheromons;
        for (std::size_t j = 0; j < pheromons.size(); ++j) {
            pheromons[j] *= (1.0 - po);
        }

        std::sort(ant_pathes.begin(), ant_pathes.end(), [](const std::vector<City>& lhs, const std::vector<City>& rhs) {
            return total_cost(lhs) < total_cost(rhs);
        });

        for (std::size_t j = 0; j < ant_pathes.size() / 2; ++j) {
            double delta_pheromone = Q / total_cost(ant_pathes[j]);

            for (std::size_t k = 1; k < ant_pathes[j].size(); ++k) {
                const auto from = static_cast<std::size_t>(ant_pathes[j][k - 1].id - 1);
                const auto to = static_cast<std::size_t>(ant_pathes[j][k].id - 1);

                pheromons[matrix_index(from, to, n)] += delta_pheromone;
                pheromons[matrix_index(to, from, n)] += delta_pheromone;
            }

            const auto from = static_cast<std::size_t>(ant_pathes[j].back().id - 1);
            const auto to = static_cast<std::size_t>(ant_pathes[j].front().id - 1);

            pheromons[matrix_index(from, to, n)] += delta_pheromone;
            pheromons[matrix_index(to, from, n)] += delta_pheromone;
        }

        if (use_two_opt) {
            apply_two_opt(ant_pathes[0], distance_matrix);
        }

        if (total_cost(curr_best) > total_cost(ant_pathes[0])) {
            curr_best = ant_pathes[0];
        }

        if (i % 50 == 0) {
            std::cout << "Iteration: " << i << "  Best so far " << total_cost(curr_best) << std::endl;
        }
    }

    cities = curr_best;
}
