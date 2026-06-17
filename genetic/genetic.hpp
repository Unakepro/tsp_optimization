#ifndef GENETIC
#define GENETIC

#include <cstddef>
#include <vector>

#include "../Cities/city.hpp"

void validate_genetic_parameters(const std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps);
void genetic_optimization(std::vector<City>& cities, double mutation_rate, std::size_t size, std::size_t steps, bool use_two_opt = false);

#endif
