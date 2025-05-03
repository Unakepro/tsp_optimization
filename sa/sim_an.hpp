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

void sa_optimization(std::vector<City>& cities, double start_temp, double end_temp, double alpha, size_t steps) {
    std::shuffle(cities.begin(), cities.end(), gen);

    double currEnergy = total_cost(cities);
    auto bestState = cities;
    double bestEnergy = currEnergy;

    double T = start_temp;
    
    for(size_t i = 0; i < steps; ++i) {
        double newEnergy = 0;
        auto tmp_state = new_state(cities);
        
        newEnergy = tmp_state.second;

        if(newEnergy < currEnergy) {
            currEnergy = newEnergy;
            cities = tmp_state.first;
        }
        else {
            if(make_transition(exp(-((newEnergy-currEnergy)/T)))) {
                cities = tmp_state.first;
                currEnergy = newEnergy;

                if(currEnergy < bestEnergy) {
                    bestEnergy = currEnergy;
                    bestState = cities;
                }
            }
        }

        T *= alpha; 

        if(T <= end_temp) break; 
    }

    cities = bestState;
}


#endif