#ifndef CITY
#define CITY

#include <iostream>

std::random_device rd;
std::mt19937 gen(rd());

struct City {
    int id;
    std::pair<double, double> point;

    
    bool operator<(const City& other) const {
        return id < other.id;
    }
};


double euclideanDistance(const City& a, const City& b) {
    return std::sqrt(std::pow((a.point.first - b.point.first), 2) + std::pow((a.point.second - b.point.second), 2));
}

double total_cost(std::vector<City>& cities) {
    double total_distance = 0;
    
    for(size_t i = 1; i < cities.size(); ++i) {
        total_distance += euclideanDistance(cities[i-1], cities[i]);
    }

    total_distance += euclideanDistance(cities[cities.size()-1], cities[0]);

    return total_distance;
}

#endif