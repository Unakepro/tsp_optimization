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

    assert(not_used.empty());

    return output;
}

void mutation(std::vector<City>& order) {
    std::uniform_int_distribution<> city_swap(0, order.size()-1);
 
    std::swap(order[city_swap(gen)], order[city_swap(gen)]);
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


void genetic_optimization(std::vector<City>& cities, double mutation_rate, size_t size, size_t steps, bool use_two_opt=false) {    
    if(size % 2 != 0) {
        throw std::logic_error("population should be divisible by 2");
    }

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
    int currDistance = total_cost(currBest);

    for(size_t i = 0; i < steps; ++i) {
        std::shuffle(population.begin(), population.end(), gen);

        if(use_two_opt) {
            for(size_t j = 0; j < population.size() / 2; ++j) {
                apply_two_opt(population[j], distance_matrix);
            }
        }

        std::sort(population.begin(), population.end(), [](std::vector<City> lhs, std::vector<City> rhs){
            return total_cost(lhs) < total_cost(rhs);
        });
         
        if(currDistance > total_cost(population[0])) {
            currBest = population[0];
            currDistance = total_cost(currBest);
        }
        
        std::vector<std::vector<City>> selection;
        std::uniform_int_distribution<> dist(0, population.size()-1);

        for(size_t j = 0; j < size / 2; ++j) {
            int cand1 = dist(gen);
            int cand2 = dist(gen);

            int parent1 = total_cost(population[cand1]) <= total_cost(population[cand2]) ? cand1 : cand2;

            cand1 = dist(gen);
            cand2 = dist(gen);
            
            int parent2 = total_cost(population[cand1]) <= total_cost(population[cand2]) ? cand1 : cand2;
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

        std::cout << "Best so far " << currDistance << std::endl;
    }


    cities = currBest;
}

#endif