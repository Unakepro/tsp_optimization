#ifndef ant_colony_optimization
#define ant_colony_optimization

#include <unordered_set>
#include <random>

#include "../Cities/city.hpp"

const double Q = 100;


inline int idx(int i, int j, int n) {
    return i * n + j;
}


void generate_path(const std::vector<City>& cities, std::vector<City>& path, const std::vector<double>& old_pheromons, const std::vector<double>& eta_matrix, int alpha, int beta) {
    size_t n = cities.size();

    std::vector<bool> used(n, false);
    used[path[0].id - 1] = true;
    
    while (path.size() < n) {
        std::vector<std::pair<double, int>> city_weight;

        auto current_city = path.back();
        int current_id = current_city.id - 1;

        for(size_t i = 0; i < n; ++i) {
            if (used[i]) continue;

            double tau = old_pheromons[idx(current_id, i, n)];
            double eta = eta_matrix[idx(current_id, i, n)];
            double weight = std::pow(tau, alpha) * std::pow(eta, beta);

            city_weight.push_back({weight, i});
        }

        std::vector<double> weights;
        for(const auto& [weight, _]: city_weight) {
            weights.push_back(weight);
        }

        std::discrete_distribution<> weighted_dist(weights.begin(), weights.end());   
        int selected_index = weighted_dist(gen);
        
        int next_city_index = city_weight[selected_index].second;
        path.push_back(cities[next_city_index]);
        used[next_city_index] = true;
    }
}

void precompute_matrix(const std::vector<City>& cities, std::vector<double>& distance_matrix, std::vector<double>& eta_matrix) {
    size_t n = cities.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i != j) {
                double dist = euclideanDistance(cities[i], cities[j]);
                distance_matrix[idx(i, j, n)] = dist;
                eta_matrix[idx(i, j, n)] = 1.0 / (dist + 1e-6);
            }
            else {
                distance_matrix[idx(i, j, n)] = 0;
                eta_matrix[idx(i, j, n)] = 0;
            }
        }
    }
}


void apply_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix) {
    bool improved = true;
    size_t n = path.size();

    while (improved) {
        improved = false;
        for (size_t i = 1; i < n - 2; ++i) {
            for (size_t j = i + 1; j < n - 1; ++j) {                
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


void aco(std::vector<City>& cities, size_t m, double alpha, double beta, double po, int epochs) {
    size_t n = cities.size();

    std::vector<double> pheromons(n * n, 1);
    std::vector<double> distance_matrix(n * n);
    std::vector<double> eta_matrix(n * n);
    
    precompute_matrix(cities, distance_matrix, eta_matrix);

    std::vector<City> curr_best = cities;
    for(size_t i = 0; i < epochs; ++i) {
        std::vector<std::vector<City>> ant_pathes;

        auto tmp_pheromons = pheromons;
        for(size_t j = 0; j < m; ++j) {
            std::uniform_int_distribution<> dist(0, n-1);

            ant_pathes.push_back({cities[dist(gen)]});

            generate_path(cities, ant_pathes[j], pheromons, eta_matrix, alpha, beta);            
        }


        pheromons = tmp_pheromons;
        for(size_t j = 0; j < pheromons.size(); ++j) {
            pheromons[j] *= (1.0 - po);
        }


        std::sort(ant_pathes.begin(), ant_pathes.end(), [](const std::vector<City>& lhs, const std::vector<City>& rhs){
            return total_cost(lhs) < total_cost(rhs);
        });

        for(size_t j = 0; j < ant_pathes.size() / 2; ++j) {
            double delta_pheromone = Q / total_cost(ant_pathes[j]);

            for(size_t k = 1; k < ant_pathes[j].size(); ++k) {
                int from = ant_pathes[j][k - 1].id - 1;
                int to = ant_pathes[j][k].id - 1;
    
                pheromons[idx(from, to, n)] += delta_pheromone;
                pheromons[idx(to, from, n)] += delta_pheromone;
            }
    
        }

        apply_two_opt(ant_pathes[0], distance_matrix);

        if(total_cost(curr_best) > total_cost(ant_pathes[0])) {
            curr_best = ant_pathes[0];
        }

        std::cout << "Best so far " << total_cost(curr_best) << std::endl;
    }

    cities = curr_best;
}   


#endif