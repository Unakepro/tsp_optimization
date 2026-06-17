#include "genetic.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {

struct ScoredTour {
    std::vector<City> tour;
    double cost = 0.0;
};

std::size_t tournament_select(const std::vector<ScoredTour>& population) {
    const std::size_t candidate_count = std::max<std::size_t>(1, population.size() / 2);
    std::uniform_int_distribution<std::size_t> candidate_dist(0, candidate_count - 1);

    const std::size_t cand1 = candidate_dist(gen);
    const std::size_t cand2 = candidate_dist(gen);

    return population[cand1].cost <= population[cand2].cost ? cand1 : cand2;
}

void update_best(const ScoredTour& candidate, std::vector<City>& best_tour, double& best_cost) {
    if (candidate.cost < best_cost) {
        best_tour = candidate.tour;
        best_cost = candidate.cost;
    }
}

}

std::vector<City> genetic_order_crossover(const std::vector<City>& parent1, const std::vector<City>& parent2) {
    if (parent1.empty() || parent1.size() != parent2.size()) {
        throw std::invalid_argument("Genetic crossover requires non-empty parents with equal sizes.");
    }

    std::uniform_int_distribution<std::size_t> path_dist(0, parent1.size() - 1);

    std::size_t start = path_dist(gen);
    std::size_t end = path_dist(gen);

    if (start > end) {
        std::swap(start, end);
    }

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

void validate_genetic_parameters(const std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps) {
    validate_tsp_input(cities, "Genetic algorithm");

    if (!std::isfinite(mutation_rate) || mutation_rate < 0.0 || mutation_rate > 1.0) {
        throw std::invalid_argument("Genetic algorithm parameter mutation_rate must be finite and between 0 and 1.");
    }
    if (size == 0) {
        throw std::invalid_argument("Genetic algorithm parameter population_size must be greater than zero.");
    }
    if (steps == 0) {
        throw std::invalid_argument("Genetic algorithm parameter steps must be greater than zero.");
    }
}

void genetic_optimization(std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps, bool use_two_opt, bool verbose) {
    validate_genetic_parameters(cities, mutation_rate, size, steps);

    const std::vector<double> distance_matrix = build_distance_matrix(cities);

    std::shuffle(cities.begin(), cities.end(), gen);

    std::vector<ScoredTour> population;
    population.reserve(size);

    std::vector<City> tmp;
    for (std::size_t i = 0; i < size; ++i) {
        tmp = cities;
        std::shuffle(tmp.begin(), tmp.end(), gen);
        const double cost = total_cost(tmp, distance_matrix);
        population.push_back({std::move(tmp), cost});
    }

    auto currBest = population.front().tour;
    double currDistance = population.front().cost;

    for (std::size_t i = 0; i < steps; ++i) {
        std::shuffle(population.begin(), population.end(), gen);

        std::sort(population.begin(), population.end(), [](const ScoredTour& lhs, const ScoredTour& rhs) {
            return lhs.cost < rhs.cost;
        });

        if (use_two_opt) {
            for (std::size_t j = 0; j < population.size() / 4; ++j) {
                apply_two_opt(population[j].tour, distance_matrix);
                population[j].cost = total_cost(population[j].tour, distance_matrix);
            }

            std::sort(population.begin(), population.end(), [](const ScoredTour& lhs, const ScoredTour& rhs) {
                return lhs.cost < rhs.cost;
            });
        }

        update_best(population[0], currBest, currDistance);

        std::vector<ScoredTour> next_population;
        next_population.reserve(size);
        const std::size_t elite_count = std::min<std::size_t>(size, std::max<std::size_t>(1, size / 10));
        for (std::size_t j = 0; j < elite_count; ++j) {
            next_population.push_back(population[j]);
        }

        std::bernoulli_distribution should_mutate(mutation_rate);
        while (next_population.size() < size) {
            const std::size_t parent1 = tournament_select(population);
            const std::size_t parent2 = tournament_select(population);

            auto result = genetic_order_crossover(population[parent1].tour, population[parent2].tour);
            if (should_mutate(gen)) {
                mutate_tour(result);
            }

            const double result_cost = total_cost(result, distance_matrix);
            ScoredTour child{std::move(result), result_cost};
            update_best(child, currBest, currDistance);
            next_population.emplace_back(std::move(child));
        }

        population = std::move(next_population);

        if (verbose && i % 100 == 0) {
            std::cout << "Generation " << i << " - Best: " << currDistance << " - Mutation Rate: " << mutation_rate << std::endl;
        }
    }

    cities = currBest;
}
