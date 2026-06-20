#ifndef TSP_ALGORITHMS_SA
#define TSP_ALGORITHMS_SA

#include <cstddef>
#include <vector>

#include "../core/config.hpp"
#include "../core/tsp.hpp"

double tour_reversal_delta(const std::vector<City>& cities, const std::vector<double>& distance_matrix,
                           std::size_t start, std::size_t end);

SolveResult sa_solve(std::vector<City>& cities, const SaParams& params, const StopCondition& stop);

#endif
