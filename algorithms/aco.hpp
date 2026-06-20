#ifndef TSP_ALGORITHMS_ACO
#define TSP_ALGORITHMS_ACO

#include <vector>

#include "../core/config.hpp"
#include "../core/tsp.hpp"

SolveResult aco_solve(std::vector<City>& cities, const AcoParams& params, const StopCondition& stop);

#endif
