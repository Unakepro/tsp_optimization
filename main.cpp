#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "sa/sim_an.hpp"
#include "genetic/genetic.hpp"


void readfile(std::vector<City>& cities, std::string filename) {
    std::string line;
    bool reading_coords = false;

    std::ifstream file(filename);
    while (std::getline(file, line)) {
        if (line.find("NODE_COORD_SECTION") != std::string::npos) {
            reading_coords = true;
            continue;
        }
        if (line.find("EOF") != std::string::npos) break;
        if (reading_coords) {
            std::istringstream iss(line);
            City c;
            iss >> c.id >> c.point.first >> c.point.second;
            cities.push_back(c);
        }
    }
}


int main() {
    std::vector<City> cities;
    readfile(cities, "tsplib/a280.tsp");

    std::shuffle(cities.begin(), cities.end(), gen);
    std::cout << "Before optimization: " << total_cost(cities) << std::endl;

    // sa_optimization(cities, 100000, 0.01, 0.9, 100000);
    genetic_optimization(cities, 0.02, 10, 100, true);

    std::cout << total_cost(cities);
}