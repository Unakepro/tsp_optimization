#include "aco.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
constexpr double Q = 100.0;

struct AntPath {
    std::vector<City> path;
    double cost = std::numeric_limits<double>::infinity();
};

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

void generate_path(const std::vector<City>& cities, std::vector<City>& path, const std::vector<double>& old_pheromons, const std::vector<double>& eta_matrix, double alpha, double beta) {
    std::size_t n = cities.size();

    std::vector<bool> used(n, false);
    used[static_cast<std::size_t>(path[0].id - 1)] = true;

    while (path.size() < n) {
        std::vector<std::pair<double, int>> city_weight;
        double weight_sum = 0.0;

        auto current_city = path.back();
        const auto current_id = static_cast<std::size_t>(current_city.id - 1);

        for (std::size_t i = 0; i < n; ++i) {
            if (used[i]) {
                continue;
            }

            double tau = old_pheromons[matrix_index(current_id, i, n)];
            double eta = eta_matrix[matrix_index(current_id, i, n)];
            double weight = std::pow(tau, alpha) * std::pow(eta, beta);

            city_weight.push_back({weight, static_cast<int>(i)});
            weight_sum += weight;
        }

        int selected_index = 0;
        if (weight_sum <= 0.0 || !std::isfinite(weight_sum)) {
            std::uniform_int_distribution<> uniform_dist(0, static_cast<int>(city_weight.size() - 1));
            selected_index = uniform_dist(gen);
        } else {
            std::vector<double> weights;
            weights.reserve(city_weight.size());
            for (const auto& [weight, _]: city_weight) {
                weights.push_back(weight);
            }

            std::discrete_distribution<> weighted_dist(weights.begin(), weights.end());
            selected_index = weighted_dist(gen);
        }

        int next_city_index = city_weight[selected_index].second;
        path.push_back(cities[static_cast<std::size_t>(next_city_index)]);
        used[static_cast<std::size_t>(next_city_index)] = true;
    }
}

void precompute_eta_matrix(const std::vector<double>& distance_matrix, std::vector<double>& eta_matrix, std::size_t n) {
    if (distance_matrix.size() != n * n || eta_matrix.size() != n * n) {
        throw std::invalid_argument("Matrix size does not match city count.");
    }

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j) {
                const double dist = distance_matrix[matrix_index(i, j, n)];
                eta_matrix[matrix_index(i, j, n)] = 1.0 / (dist + 1e-6);
            } else {
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

void aco(std::vector<City>& cities, std::size_t m, double alpha, double beta, double po, std::size_t epochs, bool use_two_opt, bool verbose) {
    validate_aco_parameters(cities, m, alpha, beta, po, epochs);

    std::size_t n = cities.size();
    std::vector<City> city_by_id = cities;
    std::sort(city_by_id.begin(), city_by_id.end());

    std::vector<double> pheromons(n * n, 1);
    const std::vector<double> distance_matrix = build_distance_matrix(city_by_id);
    std::vector<double> eta_matrix(n * n);

    precompute_eta_matrix(distance_matrix, eta_matrix, n);

    std::vector<City> curr_best = city_by_id;
    double best_cost = total_cost(curr_best, distance_matrix);
    for (std::size_t i = 0; i < epochs; ++i) {
        std::vector<AntPath> ant_paths;
        ant_paths.reserve(m);

        for (std::size_t j = 0; j < m; ++j) {
            std::uniform_int_distribution<> dist(0, n - 1);

            AntPath ant;
            ant.path.reserve(n);
            ant.path.push_back(city_by_id[static_cast<std::size_t>(dist(gen))]);

            generate_path(city_by_id, ant.path, pheromons, eta_matrix, alpha, beta);
            ant_paths.emplace_back(std::move(ant));
        }

        if (use_two_opt) {
            for (auto& ant: ant_paths) {
                apply_two_opt(ant.path, distance_matrix);
            }
        }

        for (auto& ant: ant_paths) {
            ant.cost = total_cost(ant.path, distance_matrix);
        }

        for (std::size_t j = 0; j < pheromons.size(); ++j) {
            pheromons[j] *= (1.0 - po);
        }

        std::sort(ant_paths.begin(), ant_paths.end(), [](const AntPath& lhs, const AntPath& rhs) {
            return lhs.cost < rhs.cost;
        });

        const std::size_t reinforced_ants = std::max<std::size_t>(1, ant_paths.size() / 2);
        for (std::size_t j = 0; j < reinforced_ants; ++j) {
            if (ant_paths[j].cost <= 0.0 || !std::isfinite(ant_paths[j].cost)) {
                continue;
            }

            double delta_pheromone = Q / ant_paths[j].cost;
            const auto& path = ant_paths[j].path;

            for (std::size_t k = 1; k < path.size(); ++k) {
                const auto from = static_cast<std::size_t>(path[k - 1].id - 1);
                const auto to = static_cast<std::size_t>(path[k].id - 1);

                pheromons[matrix_index(from, to, n)] += delta_pheromone;
                pheromons[matrix_index(to, from, n)] += delta_pheromone;
            }

            const auto from = static_cast<std::size_t>(path.back().id - 1);
            const auto to = static_cast<std::size_t>(path.front().id - 1);

            pheromons[matrix_index(from, to, n)] += delta_pheromone;
            pheromons[matrix_index(to, from, n)] += delta_pheromone;
        }

        const double candidate_cost = ant_paths[0].cost;
        if (best_cost > candidate_cost) {
            curr_best = ant_paths[0].path;
            best_cost = candidate_cost;
        }

        if (verbose && i % 50 == 0) {
            std::cout << "Iteration: " << i << "  Best so far " << best_cost << std::endl;
        }
    }

    cities = curr_best;
}
