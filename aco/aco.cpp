#include "aco.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
constexpr double Q = 1.0;
constexpr std::size_t DEFAULT_CANDIDATE_LIST_SIZE = 20;

struct AntPath {
    std::vector<City> path;
    double cost = std::numeric_limits<double>::infinity();
};

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

double transition_weight(double pheromone, double eta_beta, double alpha) {
    if (pheromone <= 0.0 || eta_beta <= 0.0 || !std::isfinite(pheromone) || !std::isfinite(eta_beta)) {
        return 0.0;
    }

    const double pheromone_weight = alpha == 1.0 ? pheromone : std::pow(pheromone, alpha);
    const double weight = pheromone_weight * eta_beta;
    return std::isfinite(weight) && weight > 0.0 ? weight : 0.0;
}

std::size_t uniform_unvisited_city(const std::vector<bool>& used) {
    std::size_t available = 0;
    for (bool is_used: used) {
        if (!is_used) {
            ++available;
        }
    }

    std::uniform_int_distribution<std::size_t> dist(0, available - 1);
    std::size_t selected_offset = dist(gen);
    for (std::size_t city = 0; city < used.size(); ++city) {
        if (used[city]) {
            continue;
        }
        if (selected_offset == 0) {
            return city;
        }
        --selected_offset;
    }

    return used.size();
}

std::size_t select_weighted_city(const std::vector<std::size_t>& candidates,
                                 const std::vector<bool>& used,
                                 const std::vector<double>& pheromons,
                                 const std::vector<double>& eta_beta_matrix,
                                 std::size_t current_id,
                                 double alpha,
                                 std::size_t n) {
    double total_weight = 0.0;
    for (std::size_t city: candidates) {
        if (used[city]) {
            continue;
        }

        total_weight += transition_weight(pheromons[matrix_index(current_id, city, n)], eta_beta_matrix[matrix_index(current_id, city, n)], alpha);
    }

    if (total_weight <= 0.0 || !std::isfinite(total_weight)) {
        return n;
    }

    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double selected_weight = dist(gen);
    for (std::size_t city: candidates) {
        if (used[city]) {
            continue;
        }

        selected_weight -= transition_weight(pheromons[matrix_index(current_id, city, n)], eta_beta_matrix[matrix_index(current_id, city, n)], alpha);
        if (selected_weight <= 0.0) {
            return city;
        }
    }

    return n;
}

std::size_t select_weighted_city_from_all(const std::vector<bool>& used,
                                          const std::vector<double>& pheromons,
                                          const std::vector<double>& eta_beta_matrix,
                                          std::size_t current_id,
                                          double alpha,
                                          std::size_t n) {
    double total_weight = 0.0;
    for (std::size_t city = 0; city < n; ++city) {
        if (used[city]) {
            continue;
        }

        total_weight += transition_weight(pheromons[matrix_index(current_id, city, n)], eta_beta_matrix[matrix_index(current_id, city, n)], alpha);
    }

    if (total_weight <= 0.0 || !std::isfinite(total_weight)) {
        return uniform_unvisited_city(used);
    }

    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double selected_weight = dist(gen);
    for (std::size_t city = 0; city < n; ++city) {
        if (used[city]) {
            continue;
        }

        selected_weight -= transition_weight(pheromons[matrix_index(current_id, city, n)], eta_beta_matrix[matrix_index(current_id, city, n)], alpha);
        if (selected_weight <= 0.0) {
            return city;
        }
    }

    return uniform_unvisited_city(used);
}

void generate_path(const std::vector<City>& cities,
                   std::vector<City>& path,
                   const std::vector<double>& old_pheromons,
                   const std::vector<double>& eta_beta_matrix,
                   const std::vector<std::vector<std::size_t>>& candidate_lists,
                   double alpha) {
    const std::size_t n = cities.size();

    std::vector<bool> used(n, false);
    used[static_cast<std::size_t>(path[0].id - 1)] = true;

    while (path.size() < n) {
        auto current_city = path.back();
        const auto current_id = static_cast<std::size_t>(current_city.id - 1);

        std::size_t next_city_index = select_weighted_city(candidate_lists[current_id], used, old_pheromons, eta_beta_matrix, current_id, alpha, n);
        if (next_city_index == n) {
            next_city_index = select_weighted_city_from_all(used, old_pheromons, eta_beta_matrix, current_id, alpha, n);
        }

        path.push_back(cities[next_city_index]);
        used[next_city_index] = true;
    }
}

void precompute_eta_beta_matrix(const std::vector<double>& distance_matrix, std::vector<double>& eta_beta_matrix, double beta, std::size_t n) {
    if (distance_matrix.size() != n * n || eta_beta_matrix.size() != n * n) {
        throw std::invalid_argument("Matrix size does not match city count.");
    }

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j) {
                const double dist = distance_matrix[matrix_index(i, j, n)];
                eta_beta_matrix[matrix_index(i, j, n)] = std::pow(1.0 / (dist + 1e-6), beta);
            } else {
                eta_beta_matrix[matrix_index(i, j, n)] = 0;
            }
        }
    }
}

std::vector<std::vector<std::size_t>> build_candidate_lists(const std::vector<double>& distance_matrix, std::size_t n, std::size_t candidate_count) {
    std::vector<std::vector<std::size_t>> candidate_lists(n);
    const std::size_t limit = std::min(candidate_count, n - 1);

    for (std::size_t city = 0; city < n; ++city) {
        std::vector<std::size_t> neighbors;
        neighbors.reserve(n - 1);
        for (std::size_t other = 0; other < n; ++other) {
            if (other != city) {
                neighbors.push_back(other);
            }
        }

        std::partial_sort(neighbors.begin(), neighbors.begin() + static_cast<std::ptrdiff_t>(limit), neighbors.end(), [&](std::size_t lhs, std::size_t rhs) {
            return distance_matrix[matrix_index(city, lhs, n)] < distance_matrix[matrix_index(city, rhs, n)];
        });

        neighbors.resize(limit);
        candidate_lists[city] = std::move(neighbors);
    }

    return candidate_lists;
}

double nearest_neighbor_cost(const std::vector<double>& distance_matrix, std::size_t n) {
    std::vector<bool> used(n, false);
    std::size_t current = 0;
    used[current] = true;
    double cost = 0.0;

    for (std::size_t step = 1; step < n; ++step) {
        std::size_t best_city = n;
        double best_distance = std::numeric_limits<double>::infinity();
        for (std::size_t city = 0; city < n; ++city) {
            if (!used[city] && distance_matrix[matrix_index(current, city, n)] < best_distance) {
                best_city = city;
                best_distance = distance_matrix[matrix_index(current, city, n)];
            }
        }

        used[best_city] = true;
        cost += best_distance;
        current = best_city;
    }

    cost += distance_matrix[matrix_index(current, 0, n)];
    return cost;
}

void deposit_pheromone(std::vector<double>& pheromons, const std::vector<City>& path, std::size_t n, double amount) {
    for (std::size_t k = 1; k < path.size(); ++k) {
        const auto from = static_cast<std::size_t>(path[k - 1].id - 1);
        const auto to = static_cast<std::size_t>(path[k].id - 1);

        pheromons[matrix_index(from, to, n)] += amount;
        pheromons[matrix_index(to, from, n)] += amount;
    }

    const auto from = static_cast<std::size_t>(path.back().id - 1);
    const auto to = static_cast<std::size_t>(path.front().id - 1);

    pheromons[matrix_index(from, to, n)] += amount;
    pheromons[matrix_index(to, from, n)] += amount;
}

void clamp_pheromones(std::vector<double>& pheromons, double min_pheromone, double max_pheromone) {
    if (!std::isfinite(max_pheromone)) {
        return;
    }

    for (double& pheromone: pheromons) {
        pheromone = std::clamp(pheromone, min_pheromone, max_pheromone);
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

    const std::vector<double> distance_matrix = build_distance_matrix(city_by_id);
    std::vector<double> eta_beta_matrix(n * n);
    const auto candidate_lists = build_candidate_lists(distance_matrix, n, DEFAULT_CANDIDATE_LIST_SIZE);

    precompute_eta_beta_matrix(distance_matrix, eta_beta_matrix, beta, n);

    std::vector<City> curr_best = city_by_id;
    const double initial_cost = nearest_neighbor_cost(distance_matrix, n);
    double best_cost = total_cost(curr_best, distance_matrix);

    const double initial_pheromone = po > 0.0 && std::isfinite(initial_cost) && initial_cost > 0.0 ? 1.0 / (po * initial_cost) : 1.0;
    std::vector<double> pheromons(n * n, initial_pheromone);
    for (std::size_t i = 0; i < epochs; ++i) {
        std::vector<AntPath> ant_paths;
        ant_paths.reserve(m);

        for (std::size_t j = 0; j < m; ++j) {
            std::uniform_int_distribution<> dist(0, n - 1);

            AntPath ant;
            ant.path.reserve(n);
            ant.path.push_back(city_by_id[static_cast<std::size_t>(dist(gen))]);

            generate_path(city_by_id, ant.path, pheromons, eta_beta_matrix, candidate_lists, alpha);
            ant_paths.emplace_back(std::move(ant));
        }

        for (auto& ant: ant_paths) {
            ant.cost = total_cost(ant.path, distance_matrix);
        }

        std::sort(ant_paths.begin(), ant_paths.end(), [](const AntPath& lhs, const AntPath& rhs) {
            return lhs.cost < rhs.cost;
        });

        if (use_two_opt) {
            const std::size_t polished_ants = std::min<std::size_t>(3, ant_paths.size());
            for (std::size_t j = 0; j < polished_ants; ++j) {
                apply_two_opt(ant_paths[j].path, distance_matrix);
                ant_paths[j].cost = total_cost(ant_paths[j].path, distance_matrix);
            }

            std::sort(ant_paths.begin(), ant_paths.end(), [](const AntPath& lhs, const AntPath& rhs) {
                return lhs.cost < rhs.cost;
            });
        }

        const double candidate_cost = ant_paths[0].cost;
        if (best_cost > candidate_cost) {
            curr_best = ant_paths[0].path;
            best_cost = candidate_cost;
        }

        for (std::size_t j = 0; j < pheromons.size(); ++j) {
            pheromons[j] *= (1.0 - po);
        }

        if (ant_paths[0].cost > 0.0 && std::isfinite(ant_paths[0].cost)) {
            deposit_pheromone(pheromons, ant_paths[0].path, n, Q / ant_paths[0].cost);
        }
        if (best_cost > 0.0 && std::isfinite(best_cost)) {
            deposit_pheromone(pheromons, curr_best, n, Q / best_cost);
        }

        const double max_pheromone = po > 0.0 && best_cost > 0.0 && std::isfinite(best_cost) ? 1.0 / (po * best_cost) : std::numeric_limits<double>::infinity();
        const double min_pheromone = std::isfinite(max_pheromone) ? max_pheromone / static_cast<double>(2 * n) : 0.0;
        clamp_pheromones(pheromons, min_pheromone, max_pheromone);

        if (verbose && i % 50 == 0) {
            std::cout << "Iteration: " << i << "  Best so far " << best_cost << std::endl;
        }
    }

    cities = curr_best;
}
