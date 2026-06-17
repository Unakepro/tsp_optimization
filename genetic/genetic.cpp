#include "genetic.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>

namespace {

std::size_t matrix_index(std::size_t i, std::size_t j, std::size_t n) {
    return i * n + j;
}

std::vector<City> crossover(std::vector<std::vector<City>>& population, int index1, int index2) {
    std::uniform_int_distribution<> path_dist(0, population[index1].size() - 1);

    std::vector<City> output = population[index1];
    int start = path_dist(gen);
    int end = path_dist(gen);

    if (start > end) {
        std::swap(start, end);
    }

    std::set<City> used(population[index1].begin() + start, population[index1].begin() + end + 1);

    std::queue<City> not_used;
    for (std::size_t i = 0; i < population[index2].size(); ++i) {
        if (used.find(population[index2][i]) == used.end()) {
            not_used.push(population[index2][i]);
        }
    }

    for (std::size_t i = 0; i < output.size(); ++i) {
        if (used.find(output[i]) == used.end()) {
            output[i] = not_used.front();
            not_used.pop();
        }
    }

    return output;
}

void mutation(std::vector<City>& order) {
    std::uniform_int_distribution<> city_swap(0, order.size() - 1);
    std::uniform_real_distribution<> mutation_type(0.0, 1.0);

    if (mutation_type(gen) < 0.7) {
        std::swap(order[city_swap(gen)], order[city_swap(gen)]);
    } else {
        int i = city_swap(gen);
        int j = city_swap(gen);
        if (i > j) {
            std::swap(i, j);
        }
        if (j > i) {
            std::reverse(order.begin() + i, order.begin() + j + 1);
        }
    }
}

void precompute_distance(const std::vector<City>& cities, std::vector<double>& distance_matrix) {
    std::size_t n = cities.size();

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j) {
                double dist = euclideanDistance(cities[i], cities[j]);
                distance_matrix[matrix_index(i, j, n)] = dist;
            } else {
                distance_matrix[matrix_index(i, j, n)] = 0;
            }
        }
    }
}

double total_cost_fast(const std::vector<City>& cities, const std::vector<double>& distance_matrix) {
    double total_distance = 0;
    std::size_t n = cities.size();

    for (std::size_t i = 1; i < n; ++i) {
        const auto id1 = static_cast<std::size_t>(cities[i - 1].id - 1);
        const auto id2 = static_cast<std::size_t>(cities[i].id - 1);
        total_distance += distance_matrix[matrix_index(id1, id2, n)];
    }

    const auto last_id = static_cast<std::size_t>(cities[n - 1].id - 1);
    const auto first_id = static_cast<std::size_t>(cities[0].id - 1);
    total_distance += distance_matrix[matrix_index(last_id, first_id, n)];

    return total_distance;
}

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

void genetic_optimization(std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps, bool use_two_opt) {
    validate_genetic_parameters(cities, mutation_rate, size, steps);

    std::vector<double> distance_matrix(cities.size() * cities.size());
    precompute_distance(cities, distance_matrix);

    std::shuffle(cities.begin(), cities.end(), gen);

    std::vector<std::vector<City>> population;

    std::vector<City> tmp;
    for (std::size_t i = 0; i < size; ++i) {
        tmp = cities;
        std::shuffle(tmp.begin(), tmp.end(), gen);
        population.emplace_back(std::move(tmp));
    }

    auto currBest = cities;
    double currDistance = total_cost_fast(currBest, distance_matrix);

    for (std::size_t i = 0; i < steps; ++i) {
        std::shuffle(population.begin(), population.end(), gen);

        if (use_two_opt) {
            for (std::size_t j = 0; j < population.size() / 4; ++j) {
                apply_two_opt(population[j], distance_matrix);
            }
        }

        std::sort(population.begin(), population.end(), [&distance_matrix](const std::vector<City>& lhs, const std::vector<City>& rhs) {
            return total_cost_fast(lhs, distance_matrix) < total_cost_fast(rhs, distance_matrix);
        });

        double current_best_cost = total_cost_fast(population[0], distance_matrix);
        if (currDistance > current_best_cost) {
            currBest = population[0];
            currDistance = current_best_cost;
        }

        std::vector<std::vector<City>> selection;
        std::uniform_int_distribution<> dist(0, population.size() - 1);

        std::size_t elite_count = std::max(1, static_cast<int>(size * 0.1));
        for (std::size_t j = 0; j < elite_count; ++j) {
            selection.push_back(population[j]);
        }

        for (std::size_t j = elite_count; j < size / 2; ++j) {
            int cand1 = dist(gen) % (population.size() / 2);
            int cand2 = dist(gen) % (population.size() / 2);
            int parent1 = total_cost_fast(population[cand1], distance_matrix) <= total_cost_fast(population[cand2], distance_matrix) ? cand1 : cand2;

            cand1 = dist(gen) % (population.size() / 2);
            cand2 = dist(gen) % (population.size() / 2);
            int parent2 = total_cost_fast(population[cand1], distance_matrix) <= total_cost_fast(population[cand2], distance_matrix) ? cand1 : cand2;

            auto result = crossover(population, parent1, parent2);

            int chanceThreshold = static_cast<int>(100 * mutation_rate);
            std::uniform_int_distribution<> dis(1, 100);

            int chance = dis(gen);
            if (chance <= chanceThreshold) {
                mutation(result);
            }

            selection.emplace_back(std::move(result));
        }

        int half_size = population.size() / 2;

        population.erase(population.begin() + half_size, population.end());
        population.resize(half_size);

        population.insert(population.end(), selection.begin(), selection.end());

        if (i % 100 == 0) {
            std::cout << "Generation " << i << " - Best: " << currDistance << " - Mutation Rate: " << mutation_rate << std::endl;
        }
    }

    cities = currBest;
}
