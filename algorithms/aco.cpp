#include "aco.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr double Q = 1.0;
constexpr std::size_t CANDIDATE_LIST_SIZE = 20;
constexpr std::size_t POLISHED_ANTS = 3;

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
    std::size_t offset = dist(gen);
    for (std::size_t city = 0; city < used.size(); ++city) {
        if (used[city]) {
            continue;
        }
        if (offset == 0) {
            return city;
        }
        --offset;
    }

    return used.size();
}

std::size_t select_from(const std::vector<std::size_t>& candidates, const std::vector<bool>& used,
                        const std::vector<double>& pheromones, const std::vector<double>& eta_beta,
                        std::size_t current, double alpha, std::size_t n) {

    double total_weight = 0.0;

    std::size_t last_eligible = n;
    for (std::size_t city: candidates) {
        if (!used[city]) {
            last_eligible = city;
            total_weight += transition_weight(pheromones[matrix_index(current, city, n)],
                                               eta_beta[matrix_index(current, city, n)], alpha);
        }
    }
    if (total_weight <= 0.0 || !std::isfinite(total_weight)) {
        return n;
    }

    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double threshold = dist(gen);

    for (std::size_t city: candidates) {
        if (used[city]) {
            continue;
        }
        threshold -= transition_weight(pheromones[matrix_index(current, city, n)],
                                       eta_beta[matrix_index(current, city, n)], alpha);
        if (threshold <= 0.0) {
            return city;
        }
    }

    return last_eligible;
}

std::size_t select_from_all(const std::vector<bool>& used, const std::vector<double>& pheromones,
                            const std::vector<double>& eta_beta, std::size_t current, double alpha, std::size_t n) {

    double total_weight = 0.0;
    std::size_t last_eligible = n;

    for (std::size_t city = 0; city < n; ++city) {
        if (!used[city]) {
            last_eligible = city;
            total_weight += transition_weight(pheromones[matrix_index(current, city, n)],
                                               eta_beta[matrix_index(current, city, n)], alpha);
        }
    }
    if (total_weight <= 0.0 || !std::isfinite(total_weight)) {
        return uniform_unvisited_city(used);
    }

    std::uniform_real_distribution<double> dist(0.0, total_weight);
    double threshold = dist(gen);
    for (std::size_t city = 0; city < n; ++city) {
        if (used[city]) {
            continue;
        }
        threshold -= transition_weight(pheromones[matrix_index(current, city, n)],
                                       eta_beta[matrix_index(current, city, n)], alpha);
        if (threshold <= 0.0) {
            return city;
        }
    }

    return last_eligible == n ? uniform_unvisited_city(used) : last_eligible;
}

bool build_path(const std::vector<City>& cities, std::vector<City>& path,
                const std::vector<double>& pheromones, const std::vector<double>& eta_beta,
                const std::vector<std::vector<std::size_t>>& candidate_lists, double alpha,
                const RunController& controller) {
    const std::size_t n = cities.size();
    std::vector<bool> used(n, false);
    used[static_cast<std::size_t>(path[0].id - 1)] = true;

    while (path.size() < n) {
        if (controller.time_expired()) {
            return false;
        }

        const auto current = static_cast<std::size_t>(path.back().id - 1);
        std::size_t next = select_from(candidate_lists[current], used, pheromones, eta_beta, current, alpha, n);

        if (next == n) {
            next = select_from_all(used, pheromones, eta_beta, current, alpha, n);
        }
        path.push_back(cities[next]);
        used[next] = true;
    }

    return true;
}

void precompute_eta_beta(const std::vector<double>& distance_matrix, std::vector<double>& eta_beta,
                         double beta, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            eta_beta[matrix_index(i, j, n)] =
                (i == j) ? 0.0 : std::pow(1.0 / (distance_matrix[matrix_index(i, j, n)] + 1e-6), beta);
        }
    }
}

std::vector<City> nearest_neighbor_tour(const std::vector<City>& cities,
                                        const std::vector<double>& distance_matrix, std::size_t n) {
    std::vector<bool> used(n, false);
    std::size_t current = 0;

    used[current] = true;
    std::vector<City> tour;

    tour.reserve(n);
    tour.push_back(cities[current]);

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
        current = best_city;
        tour.push_back(cities[current]);
    }

    return tour;
}

void deposit_pheromone(std::vector<double>& pheromones, const std::vector<City>& path, std::size_t n, double amount) {
    for (std::size_t k = 1; k < path.size(); ++k) {
        const auto from = static_cast<std::size_t>(path[k - 1].id - 1);
        const auto to = static_cast<std::size_t>(path[k].id - 1);
        pheromones[matrix_index(from, to, n)] += amount;
        pheromones[matrix_index(to, from, n)] += amount;
    }

    const auto from = static_cast<std::size_t>(path.back().id - 1);
    const auto to = static_cast<std::size_t>(path.front().id - 1);
    pheromones[matrix_index(from, to, n)] += amount;
    pheromones[matrix_index(to, from, n)] += amount;
}

void clamp_pheromones(std::vector<double>& pheromones, double low, double high) {
    if (!std::isfinite(high)) {
        return;
    }
    for (double& pheromone: pheromones) {
        pheromone = std::clamp(pheromone, low, high);
    }
}

void run_one_epoch(const std::vector<City>& cities, const std::vector<double>& distance_matrix,
                   const std::vector<double>& eta_beta, const std::vector<std::vector<std::size_t>>& candidate_lists,
                   std::vector<double>& pheromones, std::vector<City>& best_tour, double& best_cost,
                   bool& has_ant_tour,
                   std::size_t m, double alpha, double evaporation, bool use_two_opt, std::size_t n,
                   const RunController& controller) {

    std::vector<AntPath> ants;
    ants.reserve(m);

    std::uniform_int_distribution<std::size_t> start_dist(0, n - 1);

    for (std::size_t j = 0; j < m && !controller.time_expired(); ++j) {
        AntPath ant;
        ant.path.reserve(n);
        ant.path.push_back(cities[start_dist(gen)]);
        if (!build_path(cities, ant.path, pheromones, eta_beta, candidate_lists, alpha, controller)) {
            break;
        }
        ants.emplace_back(std::move(ant));
    }

    if (ants.empty()) {
        return;
    }

    for (auto& ant: ants) {
        ant.cost = total_cost_unchecked(ant.path, distance_matrix);
    }

    std::sort(ants.begin(), ants.end(), [](const AntPath& a, const AntPath& b) { return a.cost < b.cost; });
    has_ant_tour = true;

    if (use_two_opt) {
        const std::size_t polished = std::min<std::size_t>(POLISHED_ANTS, ants.size());
        for (std::size_t j = 0; j < polished && !controller.time_expired(); ++j) {
            two_opt_neighbors_unchecked(ants[j].path, distance_matrix, candidate_lists,
                                        std::numeric_limits<std::size_t>::max(), &controller);
            ants[j].cost = total_cost_unchecked(ants[j].path, distance_matrix);
        }
        std::sort(ants.begin(), ants.end(), [](const AntPath& a, const AntPath& b) { return a.cost < b.cost; });
    }

    if (ants[0].cost < best_cost) {
        best_tour = ants[0].path;
        best_cost = ants[0].cost;
    }

    if (controller.time_expired()) {
        return;
    }

    for (double& pheromone: pheromones) {
        pheromone *= (1.0 - evaporation);
    }
    if (ants[0].cost > 0.0 && std::isfinite(ants[0].cost)) {
        deposit_pheromone(pheromones, ants[0].path, n, Q / ants[0].cost);
    }
    if (has_ant_tour && best_cost > 0.0 && std::isfinite(best_cost)) {
        deposit_pheromone(pheromones, best_tour, n, Q / best_cost);
    }

    const double high = evaporation > 0.0 && best_cost > 0.0 && std::isfinite(best_cost)
                            ? 1.0 / (evaporation * best_cost)
                            : std::numeric_limits<double>::infinity();
    const double low = std::isfinite(high) ? high / static_cast<double>(2 * n) : 0.0;

    clamp_pheromones(pheromones, low, high);
}

void validate(const AcoParams& p) {
    if (p.ants <= 0) {
        throw std::invalid_argument("ACO ants must be greater than zero.");
    }
    if (!std::isfinite(p.alpha) || p.alpha < 0.0) {
        throw std::invalid_argument("ACO alpha must be finite and non-negative.");
    }
    if (!std::isfinite(p.beta) || p.beta < 0.0) {
        throw std::invalid_argument("ACO beta must be finite and non-negative.");
    }
    if (!std::isfinite(p.evaporation) || p.evaporation < 0.0 || p.evaporation >= 1.0) {
        throw std::invalid_argument("ACO evaporation must be finite and between 0 and 1.");
    }
}

}

SolveResult aco_solve(std::vector<City>& cities, const AcoParams& params, const StopCondition& stop) {
    validate_tour_input(cities, "Ant colony optimization");
    validate(params);

    RunController controller(stop);
    controller.start();

    const std::size_t n = cities.size();
    std::vector<City> city_by_id = cities;
    std::sort(city_by_id.begin(), city_by_id.end());

    const std::vector<double> distance_matrix = build_distance_matrix(city_by_id);
    std::vector<double> eta_beta(n * n);
    precompute_eta_beta(distance_matrix, eta_beta, params.beta, n);
    const auto candidate_lists = build_neighbor_lists(distance_matrix, n, CANDIDATE_LIST_SIZE);

    std::vector<City> best_tour = nearest_neighbor_tour(city_by_id, distance_matrix, n);
    double best_cost = total_cost_unchecked(best_tour, distance_matrix);
    const double initial_cost = best_cost;
    bool has_ant_tour = false;

    const double initial_pheromone =
        params.evaporation > 0.0 && std::isfinite(initial_cost) && initial_cost > 0.0
            ? 1.0 / (params.evaporation * initial_cost)
            : 1.0;
    std::vector<double> pheromones(n * n, initial_pheromone);

    const auto m = static_cast<std::size_t>(params.ants);
    while (controller.next(best_cost)) {
        run_one_epoch(city_by_id, distance_matrix, eta_beta, candidate_lists, pheromones, best_tour, best_cost,
                      has_ant_tour, m, params.alpha, params.evaporation, params.two_opt, n, controller);
    }

    cities = best_tour;

    return controller.result(best_cost);
}
