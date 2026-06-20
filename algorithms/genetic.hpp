#ifndef TSP_ALGORITHMS_GENETIC
#define TSP_ALGORITHMS_GENETIC

#include <vector>

#include "../core/config.hpp"
#include "../core/tsp.hpp"

std::vector<City> genetic_order_crossover(const std::vector<City>& parent1, const std::vector<City>& parent2);
void mutate_tour(std::vector<City>& order);

SolveResult ga_solve(std::vector<City>& cities, const GaParams& params, const StopCondition& stop);

#endif
