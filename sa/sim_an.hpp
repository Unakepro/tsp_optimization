#ifndef SA
#define SA

#include <cstddef>
#include <vector>

#include "../Cities/city.hpp"

void validate_sa_parameters(const std::vector<City>& cities, double start_temp, double end_temp, double alpha, std::size_t steps);
double tour_reversal_delta(const std::vector<City>& cities, const std::vector<double>& distance_matrix, std::size_t start, std::size_t end);
void sa_optimization(std::vector<City>& cities, double start_temp, double end_temp, double alpha, std::size_t steps);

#endif
