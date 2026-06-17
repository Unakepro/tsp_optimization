#ifndef SA
#define SA

#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <stdexcept>
#include "../Cities/city.hpp"


std::pair<std::vector<City>, double> new_state(std::vector<City>& cities) {    
    std::uniform_int_distribution<> dist(0, cities.size()-1);

    int pos1 = dist(gen);
    int pos2 = dist(gen);

    while (pos1 == pos2) {
        pos2 = dist(gen);
    } 
    
    if (pos1 > pos2) {
        std::swap(pos1, pos2);
    }

    auto newCities = cities;
    std::reverse(newCities.begin() + pos1, newCities.begin() + pos2 + 1);
    
    return std::pair{newCities, total_cost(newCities)};

}


bool make_transition(long double P) {
    std::uniform_real_distribution<> dis(0.0, 1.0);
    if(P >= dis(gen)) {
        return true;
    }

    return false;
}

inline void validate_sa_parameters(const std::vector<City>& cities, double start_temp, double end_temp, double alpha, size_t steps) {
    validate_tsp_input(cities, "Simulated annealing");

    if (!std::isfinite(start_temp) || start_temp <= 0.0) {
        throw std::invalid_argument("Simulated annealing parameter start_temp must be finite and positive.");
    }
    if (!std::isfinite(end_temp) || end_temp <= 0.0) {
        throw std::invalid_argument("Simulated annealing parameter end_temp must be finite and positive.");
    }
    if (end_temp >= start_temp) {
        throw std::invalid_argument("Simulated annealing parameter end_temp must be smaller than start_temp.");
    }
    if (!std::isfinite(alpha) || alpha <= 0.0 || alpha >= 1.0) {
        throw std::invalid_argument("Simulated annealing parameter alpha must be finite and between 0 and 1.");
    }
    if (steps == 0) {
        throw std::invalid_argument("Simulated annealing parameter steps must be greater than zero.");
    }
}

void sa_optimization(std::vector<City>& cities, double start_temp, double end_temp, double alpha, size_t steps) {
    validate_sa_parameters(cities, start_temp, end_temp, alpha, steps);

    std::shuffle(cities.begin(), cities.end(), gen);

    double currEnergy = total_cost(cities);
    auto bestState = cities;
    double bestEnergy = currEnergy;

    double T = start_temp;
    
    for(size_t i = 0; i < steps; ++i) {
        double newEnergy = 0;
        auto tmp_state = new_state(cities);
        
        newEnergy = tmp_state.second;

        bool accepted = false;

        if(newEnergy < currEnergy) {
            accepted = true;
        }
        else {
            accepted = make_transition(std::exp(-(newEnergy - currEnergy) / T));
        }

        if(accepted) {
            cities = tmp_state.first;
            currEnergy = newEnergy;

            if(currEnergy < bestEnergy) {
                bestEnergy = currEnergy;
                bestState = cities;
            }
        }
         
        T *= alpha; 

        if(T <= end_temp) break; 
    }

    cities = bestState;
}


#endif
