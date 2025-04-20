
#ifndef SA
#define SA

#include <iostream>
#include <algorithm>
#include <random>
#include "../Cities/city.hpp"


std::pair<std::vector<City>, double> new_state(std::vector<City>& cities) {
    std::uniform_int_distribution<> dist(0, cities.size()-1);

    int pos1 = dist(gen);
    int pos2 = dist(gen);

    if(pos1 == pos2) {
        return std::pair{cities, total_cost(cities)};
    }
    else if (pos1 > pos2) {
        std::swap(pos1, pos2);
    }

    auto newCities = cities;
    std::reverse(newCities.begin()+pos1, newCities.begin() + pos2);
    
    return std::pair{newCities, total_cost(newCities)};

}


bool make_transition(long double P) {
    std::uniform_real_distribution<> dis(0.0, 1.0);
    if(P >= dis(gen)) {
        return true;
    }

    return false;
}

void sa_optimization(std::vector<City>& xs, double start_temp, double end_temp, double alpha, size_t steps) {
    double currEnergy = total_cost(xs);
    auto bestState = xs;
    double bestEnergy = currEnergy;

    double T = start_temp;
    
    for(size_t i = 0; i < steps; ++i) {
        double newEnergy = 0;
        auto tmp_state = new_state(xs);
        
        newEnergy = tmp_state.second;

        if(newEnergy < currEnergy) {
            currEnergy = newEnergy;
            xs = tmp_state.first;
        }
        else {
            if(make_transition(exp(-((newEnergy-currEnergy)/T)))) {
                xs = tmp_state.first;
                currEnergy = newEnergy;

                if(currEnergy < bestEnergy) {
                    bestEnergy = currEnergy;
                    bestState = xs;
                }
            }
        }

        T = start_temp / (1 + alpha * i); 

        if(T <= end_temp) break; 
    }

    xs = bestState;
}


#endif