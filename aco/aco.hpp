#ifndef ant_colony_optimization
#define ant_colony_optimization

#include <cstddef>
#include <vector>

#include "../Cities/city.hpp"

void validate_aco_parameters(const std::vector<City>& cities, std::size_t m, double alpha, double beta, double po, std::size_t epochs);
void aco(std::vector<City>& cities, std::size_t m, double alpha, double beta, double po, std::size_t epochs, bool use_two_opt = false);

#endif
