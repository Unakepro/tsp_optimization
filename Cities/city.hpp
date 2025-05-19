#ifndef CITY
#define CITY

#include <iostream>

inline std::random_device rd;
inline std::mt19937 gen(rd());


struct City {
    int id;
    std::pair<double, double> point;

    
    bool operator<(const City& other) const {
        return id < other.id;
    }

    bool operator==(const City& other) const {
        return id == other.id && point == other.point;
    }

};

inline double euclideanDistance(const City& a, const City& b) {
    return std::sqrt(std::pow((a.point.first - b.point.first), 2) +
                     std::pow((a.point.second - b.point.second), 2));
}


double total_cost(const std::vector<City>& cities) {
    double total_distance = 0;
    
    for(size_t i = 1; i < cities.size(); ++i) {
        total_distance += euclideanDistance(cities[i-1], cities[i]);
    }

    total_distance += euclideanDistance(cities[cities.size()-1], cities[0]);

    return total_distance;
}

inline int idx(int i, int j, int n) {
    return i * n + j;
}

void apply_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix) {
    bool improved = true;
    size_t n = path.size();

    while (improved) {
        improved = false;
        for (size_t i = 1; i < n - 1; ++i) {
            for (size_t j = i + 1; j < n - 1; ++j) {   
                if (j - i == 1) continue;

                double d1 = distance_matrix[idx(path[i - 1].id-1, path[i].id-1, n)] + distance_matrix[idx(path[j].id-1, path[j+1].id-1, n)];
                double d2 = distance_matrix[idx(path[i - 1].id-1, path[j].id-1, n)] + distance_matrix[idx(path[i].id-1, path[j+1].id-1, n)];
                
                if (d2 < d1) {
                    std::reverse(path.begin() + i, path.begin() + j + 1);
                    improved = true;
                }
            }
        }
    }
}


#endif