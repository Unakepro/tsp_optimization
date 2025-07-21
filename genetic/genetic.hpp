#ifndef GENETIC
#define GENETIC

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <set>
#include <queue>
#include "../Cities/city.hpp"


std::vector<City> crossover(std::vector<std::vector<City>>& population, int index1, int index2) {
    std::uniform_int_distribution<> path_dist(0, population[index1].size()-1);

    std::vector<City> output = population[index1];
    int start = path_dist(gen);
    int end = path_dist(gen);

    if (start > end) std::swap(start, end);

    std::set<City> used(population[index1].begin()+start, population[index1].begin() + end + 1);

    std::queue<City> not_used;
    for(size_t i = 0; i < population[index2].size(); ++i) {
        if(used.find(population[index2][i]) == used.end()) {
            not_used.push(population[index2][i]);
        }
    }

    for(size_t i = 0; i < output.size(); ++i) {
        if(used.find(output[i]) == used.end()) {
            output[i] = not_used.front();
            not_used.pop();
        }
    }


    return output;
}

void mutation(std::vector<City>& order) {
    std::uniform_int_distribution<> city_swap(0, order.size()-1);
    std::uniform_real_distribution<> mutation_type(0.0, 1.0);
    
    if(mutation_type(gen) < 0.7) {
        std::swap(order[city_swap(gen)], order[city_swap(gen)]);
    } else {
        int i = city_swap(gen);
        int j = city_swap(gen);
        if (i > j) std::swap(i, j);
        if (j > i) {
            std::reverse(order.begin() + i, order.begin() + j + 1);
        }
    }
}

void precompute_distance(const std::vector<City>& cities, std::vector<double>& distance_matrix) {
    size_t n = cities.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i != j) {
                double dist = euclideanDistance(cities[i], cities[j]);
                distance_matrix[idx(i, j, n)] = dist;
            }
            else {
                distance_matrix[idx(i, j, n)] = 0;
            }
        }
    }
}

double total_cost_fast(const std::vector<City>& cities, const std::vector<double>& distance_matrix) {
    double total_distance = 0;
    size_t n = cities.size();
    
    for(size_t i = 1; i < n; ++i) {
        int id1 = cities[i-1].id - 1; 
        int id2 = cities[i].id - 1;
        total_distance += distance_matrix[idx(id1, id2, n)];
    }
    
    int last_id = cities[n-1].id - 1;
    int first_id = cities[0].id - 1;
    total_distance += distance_matrix[idx(last_id, first_id, n)];
    
    return total_distance;
}

void genetic_optimization(std::vector<City>& cities, double mutation_rate, size_t size, size_t steps, bool use_two_opt=false) {    

    std::vector<double> distance_matrix(cities.size() * cities.size());
    precompute_distance(cities, distance_matrix);

    std::shuffle(cities.begin(), cities.end(), gen);
    
    std::vector<std::vector<City>> population;
    
    std::vector<City> tmp;
    for(size_t i = 0; i < size; ++i) {
        tmp = cities;
        std::shuffle(tmp.begin(), tmp.end(), gen);
        population.emplace_back(std::move(tmp));
    }

    auto currBest = cities;
    double currDistance = total_cost_fast(currBest, distance_matrix);

    double original_mutation_rate = mutation_rate;
    int  stagnation_counter = 0;
    double prev_best = currDistance;

    for(size_t i = 0; i < steps; ++i) {
        std::shuffle(population.begin(), population.end(), gen);

        if(use_two_opt) {
            for(size_t j = 0; j < population.size() / 4; ++j) {  
                apply_two_opt(population[j], distance_matrix);
            }
        }

        std::sort(population.begin(), population.end(), [&distance_matrix](const std::vector<City>& lhs, const std::vector<City>& rhs){
            return total_cost_fast(lhs, distance_matrix) < total_cost_fast(rhs, distance_matrix);
        });
         
        double current_best_cost = total_cost_fast(population[0], distance_matrix);
        if(currDistance > current_best_cost) {
            currBest = population[0];
            currDistance = current_best_cost;
            stagnation_counter = 0;
        } else {
            stagnation_counter++;
        }
        
        if (stagnation_counter > 20) {
            mutation_rate = std::min(0.4, original_mutation_rate * 2.0);
        } else if (current_best_cost < prev_best) {
            mutation_rate = std::max(0.05, mutation_rate * 0.95);
        }
        prev_best = current_best_cost;
        
        std::vector<std::vector<City>> selection;
        std::uniform_int_distribution<> dist(0, population.size()-1);

        size_t elite_count = std::max(1, static_cast<int>(size * 0.1));
        for(size_t j = 0; j < elite_count; ++j) {
            selection.push_back(population[j]);
        }

        for(size_t j = elite_count; j < size / 2; ++j) {
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

        if(i % 100 == 0) { 
            std::cout << "Generation " << i << " - Best: " << currDistance << " - Mutation Rate: " << mutation_rate << std::endl;
        }
    }

    cities = currBest;
}

#endif