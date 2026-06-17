#ifndef CITY
#define CITY

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
double total_cost(const std::vector<City>& cities);
void apply_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix);
void readfile(std::vector<City>& cities, const std::string& filename);

#endif
