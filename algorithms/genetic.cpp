#include "genetic.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr std::size_t TOURNAMENT_SIZE = 3;
constexpr std::size_t MEMETIC_TWO_OPT_MOVES = 25;
constexpr std::size_t TWO_OPT_NEIGHBORS = 10;

struct ScoredTour {
    std::vector<City> tour;
    double cost = 0.0;
};

std::size_t tournament_select(const std::vector<ScoredTour>& population) {
    std::uniform_int_distribution<std::size_t> dist(0, population.size() - 1);
    std::size_t best = dist(gen);

    for (std::size_t i = 1; i < TOURNAMENT_SIZE; ++i) {
        const std::size_t candidate = dist(gen);
        if (population[candidate].cost < population[best].cost) {
            best = candidate;
        }
    }

    return best;
}

void update_best(const ScoredTour& candidate, std::vector<City>& best_tour, double& best_cost) {
    if (candidate.cost < best_cost) {
        best_tour = candidate.tour;
        best_cost = candidate.cost;
    }
}

void polish(ScoredTour& candidate, const std::vector<double>& distance_matrix,
            const std::vector<std::vector<std::size_t>>& neighbors, const RunController& controller) {

    if (two_opt_neighbors_unchecked(candidate.tour, distance_matrix, neighbors, MEMETIC_TWO_OPT_MOVES, &controller) > 0) {
        candidate.cost = total_cost_unchecked(candidate.tour, distance_matrix);
    }
}

void run_one_generation(std::vector<ScoredTour>& population, std::size_t size, double mutation_rate,
                        bool use_two_opt, const std::vector<double>& distance_matrix,
                        const std::vector<std::vector<std::size_t>>& neighbors,
                        std::vector<City>& best_tour, double& best_cost,
                        const RunController& controller) {

    std::sort(population.begin(), population.end(), [](const ScoredTour& a, const ScoredTour& b) { return a.cost < b.cost; });

    if (use_two_opt) {
        const std::size_t elites = std::min<std::size_t>(std::max<std::size_t>(1, population.size() / 10), population.size());

        for (std::size_t j = 0; j < elites; ++j) {
            if (controller.time_expired()) {
                break;
            }
            polish(population[j], distance_matrix, neighbors, controller);
        }

        std::sort(population.begin(), population.end(), [](const ScoredTour& a, const ScoredTour& b) { return a.cost < b.cost; });
    }

    update_best(population[0], best_tour, best_cost);

    std::vector<ScoredTour> next_population;
    next_population.reserve(size);

    const std::size_t elite_count = std::min<std::size_t>(size, std::max<std::size_t>(1, size / 10));
    for (std::size_t j = 0; j < elite_count; ++j) {
        next_population.push_back(population[j]);
    }

    std::bernoulli_distribution should_mutate(mutation_rate);
    std::size_t polished_children = 0;
    const std::size_t max_polished_children = use_two_opt ? std::max<std::size_t>(1, size / 20) : 0;

    while (next_population.size() < size && !controller.time_expired()) {
        const std::size_t parent1 = tournament_select(population);
        const std::size_t parent2 = tournament_select(population);

        auto child_tour = genetic_order_crossover(population[parent1].tour, population[parent2].tour);
        if (should_mutate(gen)) {
            mutate_tour(child_tour);
        }

        ScoredTour child{std::move(child_tour), 0.0};
        child.cost = total_cost_unchecked(child.tour, distance_matrix);

        if (use_two_opt && polished_children < max_polished_children) {
            polish(child, distance_matrix, neighbors, controller);
            ++polished_children;
        }

        update_best(child, best_tour, best_cost);

        next_population.emplace_back(std::move(child));
    }

    if (next_population.size() == size) {
        population = std::move(next_population);
    }
}

void validate(const GaParams& p) {
    if (!std::isfinite(p.mutation) || p.mutation < 0.0 || p.mutation > 1.0) {
        throw std::invalid_argument("GA mutation must be finite and between 0 and 1.");
    }
    if (p.population <= 0) {
        throw std::invalid_argument("GA population must be greater than zero.");
    }
}

}

std::vector<City> genetic_order_crossover(const std::vector<City>& parent1, const std::vector<City>& parent2) {
    if (parent1.empty() || parent1.size() != parent2.size()) {
        throw std::invalid_argument("Genetic crossover requires non-empty parents with equal sizes.");
    }

    // Keep at least two slots for parent2 to avoid parent clones.
    const std::size_t n = parent1.size();
    std::uniform_int_distribution<std::size_t> length_dist(1, n >= 3 ? n - 2 : 1);

    const std::size_t length = length_dist(gen);
    std::uniform_int_distribution<std::size_t> start_dist(0, n - length);

    const std::size_t start = start_dist(gen);
    const std::size_t end = start + length - 1;

    std::vector<City> output(parent1.size());
    std::vector<bool> copied_position(parent1.size(), false);
    std::vector<bool> used_city(parent1.size(), false);

    for (std::size_t i = start; i <= end; ++i) {
        output[i] = parent1[i];
        copied_position[i] = true;
        used_city[static_cast<std::size_t>(parent1[i].id - 1)] = true;
    }

    auto parent2_it = parent2.begin();
    for (std::size_t i = 0; i < output.size(); ++i) {
        if (copied_position[i]) {
            continue;
        }
        while (parent2_it != parent2.end() && used_city[static_cast<std::size_t>(parent2_it->id - 1)]) {
            ++parent2_it;
        }
        if (parent2_it == parent2.end()) {
            throw std::runtime_error("Genetic crossover failed to construct a complete child tour.");
        }
        output[i] = *parent2_it;
        used_city[static_cast<std::size_t>(parent2_it->id - 1)] = true;
    }

    return output;
}

void mutate_tour(std::vector<City>& order) {
    if (order.size() < 2) {
        return;
    }

    std::uniform_int_distribution<std::size_t> city_dist(0, order.size() - 1);
    std::uniform_real_distribution<> mutation_type(0.0, 1.0);

    std::size_t i = city_dist(gen);
    std::size_t j = city_dist(gen);

    while (i == j) {
        j = city_dist(gen);
    }

    if (mutation_type(gen) < 0.7) {
        std::swap(order[i], order[j]);
        return;
    }
    if (i > j) {
        std::swap(i, j);
    }

    std::reverse(order.begin() + static_cast<std::ptrdiff_t>(i), order.begin() + static_cast<std::ptrdiff_t>(j + 1));
}

SolveResult ga_solve(std::vector<City>& cities, const GaParams& params, const StopCondition& stop) {
    validate_tour_input(cities, "Genetic algorithm");
    validate(params);

    RunController controller(stop);
    controller.start();

    const std::size_t size = static_cast<std::size_t>(params.population);
    const std::vector<double> distance_matrix = build_distance_matrix(cities);
    const std::vector<City> original_tour = cities;

    const std::vector<std::vector<std::size_t>> neighbors =
        params.two_opt ? build_neighbor_lists(distance_matrix, cities.size(), TWO_OPT_NEIGHBORS)
                       : std::vector<std::vector<std::size_t>>{};

    std::shuffle(cities.begin(), cities.end(), gen);

    std::vector<ScoredTour> population;
    population.reserve(size);
    population.push_back({original_tour, total_cost_unchecked(original_tour, distance_matrix)});

    std::vector<City> tmp;
    for (std::size_t i = 1; i < size; ++i) {
        tmp = cities;
        std::shuffle(tmp.begin(), tmp.end(), gen);
        population.push_back({tmp, total_cost_unchecked(tmp, distance_matrix)});
    }

    std::vector<City> best_tour = population.front().tour;
    double best_cost = population.front().cost;

    while (controller.next(best_cost)) {
        run_one_generation(population, size, params.mutation, params.two_opt, distance_matrix, neighbors,
                           best_tour, best_cost, controller);
    }

    if (params.two_opt && !controller.time_expired()) {
        two_opt_neighbors_unchecked(best_tour, distance_matrix, neighbors, MEMETIC_TWO_OPT_MOVES, &controller);
        best_cost = total_cost_unchecked(best_tour, distance_matrix);
    }
    cities = best_tour;

    return controller.result(best_cost);
}
