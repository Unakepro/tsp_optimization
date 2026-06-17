#ifndef TSP_CITY_HPP
#define TSP_CITY_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

inline constexpr std::uint32_t DEFAULT_RANDOM_SEED = 42;
extern std::mt19937 gen;

void set_random_seed(std::uint32_t seed);
std::uint32_t derive_run_seed(std::uint32_t base_seed, std::uint32_t algorithm_id, std::size_t dataset_index, std::size_t repeat_index);

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

int tsplibEuc2dDistance(const City& a, const City& b);
double euclideanDistance(const City& a, const City& b);
void validate_tsp_input(const std::vector<City>& cities, const std::string& algorithm_name);
bool is_valid_tour(const std::vector<City>& cities);
std::vector<double> build_distance_matrix(const std::vector<City>& cities);
double total_cost(const std::vector<City>& cities);
double total_cost(const std::vector<City>& cities, const std::vector<double>& distance_matrix);
std::size_t apply_bounded_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix, std::size_t max_improvements);
void apply_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix);
void readfile(std::vector<City>& cities, const std::string& filename);

#endif
