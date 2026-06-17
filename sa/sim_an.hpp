#ifndef SA
#define SA

#include <cstddef>
#include <vector>

#include "../Cities/city.hpp"

void validate_sa_parameters(const std::vector<City>& cities, double start_temp, double end_temp, double alpha, std::size_t steps);
void sa_optimization(std::vector<City>& cities, double start_temp, double end_temp, double alpha, std::size_t steps);

#endif
