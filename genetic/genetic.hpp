#ifndef GENETIC
#define GENETIC

#include <cstddef>
#include <vector>

#include "../Cities/city.hpp"

void validate_genetic_parameters(const std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps);
std::vector<City> genetic_order_crossover(const std::vector<City>& parent1, const std::vector<City>& parent2);
void mutate_tour(std::vector<City>& order);
void genetic_optimization(std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps, bool use_two_opt = false, bool verbose = false);

#endif
