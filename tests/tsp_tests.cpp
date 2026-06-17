#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Cities/city.hpp"
#include "../aco/aco.hpp"
#include "../genetic/genetic.hpp"
#include "../sa/sim_an.hpp"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool rejects_invalid_argument(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

std::vector<City> rectangle_tour() {
    return {
        {1, {0.0, 0.0}},
        {2, {3.0, 0.0}},
        {3, {3.0, 4.0}},
        {4, {0.0, 4.0}}
    };
}

std::vector<City> crossing_tour() {
    return {
        {1, {0.0, 0.0}},
        {2, {10.0, 10.0}},
        {3, {0.0, 10.0}},
        {4, {10.0, 0.0}}
    };
}

std::vector<City> sample_cities() {
    return {
        {1, {0.0, 0.0}},
        {2, {2.0, 1.0}},
        {3, {5.0, 0.0}},
        {4, {6.0, 4.0}},
        {5, {3.0, 7.0}},
        {6, {1.0, 5.0}},
        {7, {8.0, 1.0}},
        {8, {7.0, 7.0}}
    };
}

std::filesystem::path temp_file(const std::string& filename) {
    return std::filesystem::temp_directory_path() / (std::to_string(std::hash<std::string>{}(std::filesystem::current_path().string())) + "_" + filename);
}

void write_text_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to create temporary test file");
    }
    out << contents;
}

void test_tsplib_parser() {
    const auto path = temp_file("tsp_parser_valid.tsp");
    write_text_file(path,
                    "NAME: parser_valid\n"
                    "TYPE: TSP\n"
                    "DIMENSION : 4\n"
                    "EDGE_WEIGHT_TYPE EUC_2D\n"
                    "NODE_COORD_SECTION\n"
                    "3 3 4\n"
                    "1 0 0\n"
                    "4 0 4\n"
                    "2 3 0\n"
                    "EOF\n");

    std::vector<City> cities;
    readfile(cities, path.string());
    std::filesystem::remove(path);

    expect(cities.size() == 4, "parser should read all cities");
    expect(cities[0].id == 1 && cities[3].id == 4, "parser should sort cities by id");
    expect(total_cost(cities) == 14.0, "parser output should have expected rectangle tour cost");
}

void test_tsplib_parser_rejects_duplicates() {
    const auto path = temp_file("tsp_parser_duplicate.tsp");
    write_text_file(path,
                    "NAME: parser_duplicate\n"
                    "TYPE: TSP\n"
                    "DIMENSION: 3\n"
                    "EDGE_WEIGHT_TYPE: EUC_2D\n"
                    "NODE_COORD_SECTION\n"
                    "1 0 0\n"
                    "1 1 1\n"
                    "3 2 2\n"
                    "EOF\n");

    std::vector<City> cities;
    bool rejected = false;
    try {
        readfile(cities, path.string());
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    std::filesystem::remove(path);

    expect(rejected, "parser should reject duplicate city ids");
}

void test_distance_and_total_cost() {
    const auto cities = rectangle_tour();
    expect(tsplibEuc2dDistance(cities[0], cities[2]) == 5, "EUC_2D distance should round using TSPLIB rules");
    expect(total_cost(cities) == 14.0, "total_cost should include the return edge to the start city");

    const auto distance_matrix = build_distance_matrix(cities);
    expect(total_cost(cities, distance_matrix) == 14.0, "matrix total cost should match direct total cost");

    const std::vector<City> shuffled = {cities[2], cities[0], cities[3], cities[1]};
    expect(total_cost(shuffled, distance_matrix) == total_cost(shuffled), "matrix total cost should be indexed by city id, not vector position");
}

void test_matrix_cost_rejects_invalid_ids() {
    const auto cities = rectangle_tour();
    const auto distance_matrix = build_distance_matrix(cities);

    auto out_of_range = cities;
    out_of_range[3].id = 9;
    expect(rejects_invalid_argument([&]() { total_cost(out_of_range, distance_matrix); }), "matrix total cost should reject out-of-range city ids");

    auto duplicate = cities;
    duplicate[2] = duplicate[1];
    expect(rejects_invalid_argument([&]() { total_cost(duplicate, distance_matrix); }), "matrix total cost should reject duplicate city ids");
}

void test_tour_validity() {
    const auto cities = rectangle_tour();
    expect(is_valid_tour(cities), "valid tour should pass");

    auto duplicate = cities;
    duplicate[2] = duplicate[1];
    expect(!is_valid_tour(duplicate), "tour validity should reject duplicate city ids");

    auto missing = cities;
    missing[3].id = 8;
    expect(!is_valid_tour(missing), "tour validity should reject missing/out-of-range city ids");
}

void test_two_opt_rejects_invalid_ids() {
    const auto cities = rectangle_tour();
    const auto distance_matrix = build_distance_matrix(cities);

    auto invalid = cities;
    invalid[0].id = 6;
    expect(rejects_invalid_argument([&]() { apply_bounded_two_opt(invalid, distance_matrix, 1); }), "two-opt should reject out-of-range city ids");
}

void test_two_opt_non_regression() {
    auto cities = crossing_tour();
    const auto distance_matrix = build_distance_matrix(cities);
    const double before = total_cost(cities, distance_matrix);

    apply_two_opt(cities, distance_matrix);

    const double after = total_cost(cities, distance_matrix);
    expect(is_valid_tour(cities), "two-opt should preserve tour validity");
    expect(after <= before, "two-opt should not make a tour worse");
    expect(after == 40.0, "two-opt should uncross the simple four-city tour");
}

void test_sa_full_reversal_delta() {
    const auto cities = rectangle_tour();
    const auto distance_matrix = build_distance_matrix(cities);

    expect(tour_reversal_delta(cities, distance_matrix, 0, cities.size() - 1) == 0.0, "reversing the full cycle should not change tour cost");
}

void test_genetic_crossover_validity() {
    const auto parent1 = sample_cities();
    auto parent2 = parent1;
    std::reverse(parent2.begin(), parent2.end());

    set_random_seed(7);
    for (int i = 0; i < 50; ++i) {
        const auto child = genetic_order_crossover(parent1, parent2);
        expect(is_valid_tour(child), "GA crossover should produce a valid permutation");
    }
}

void test_genetic_mutation_validity() {
    auto tour = sample_cities();

    set_random_seed(11);
    for (int i = 0; i < 100; ++i) {
        mutate_tour(tour);
        expect(is_valid_tour(tour), "GA mutation should preserve a valid permutation");
    }
}

void test_genetic_does_not_worsen_starting_tour() {
    auto tour = sample_cities();
    const double starting_cost = total_cost(tour);

    set_random_seed(19);
    genetic_optimization(tour, 0.35, 9, 1);

    expect(is_valid_tour(tour), "genetic algorithm should return a valid tour after one generation");
    expect(total_cost(tour) <= starting_cost, "genetic algorithm should not return a tour worse than the starting tour");
}

void test_aco_handles_zero_cost_tours() {
    std::vector<City> cities = {
        {1, {0.0, 0.0}},
        {2, {0.0, 0.0}},
        {3, {0.0, 0.0}},
        {4, {0.0, 0.0}}
    };

    set_random_seed(404);
    aco(cities, 4, 1.0, 2.0, 0.2, 5);

    expect(is_valid_tour(cities), "ACO should preserve tour validity when all distances are zero");
    expect(std::isfinite(total_cost(cities)), "ACO should return a finite cost when all distances are zero");
}

void test_algorithms_preserve_valid_tours() {
    const auto base = sample_cities();

    auto sa_tour = base;
    set_random_seed(101);
    sa_optimization(sa_tour, 100.0, 0.01, 0.95, 100);
    expect(is_valid_tour(sa_tour), "simulated annealing should preserve tour validity");
    expect(std::isfinite(total_cost(sa_tour)), "simulated annealing should return a finite tour cost");

    auto ga_tour = base;
    set_random_seed(202);
    genetic_optimization(ga_tour, 0.35, 9, 25);
    expect(is_valid_tour(ga_tour), "genetic algorithm should preserve tour validity for odd population sizes");
    expect(std::isfinite(total_cost(ga_tour)), "genetic algorithm should return a finite tour cost");

    auto aco_tour = base;
    std::rotate(aco_tour.begin(), aco_tour.begin() + 3, aco_tour.end());
    set_random_seed(303);
    aco(aco_tour, 1, 1.0, 2.0, 0.2, 8);
    expect(is_valid_tour(aco_tour), "ACO should preserve tour validity for unsorted input and one ant");
    expect(std::isfinite(total_cost(aco_tour)), "ACO should return a finite tour cost");
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"TSPLIB parser", test_tsplib_parser},
        {"TSPLIB parser rejects duplicates", test_tsplib_parser_rejects_duplicates},
        {"distance and total cost", test_distance_and_total_cost},
        {"matrix cost rejects invalid ids", test_matrix_cost_rejects_invalid_ids},
        {"tour validity", test_tour_validity},
        {"two-opt rejects invalid ids", test_two_opt_rejects_invalid_ids},
        {"two-opt non-regression", test_two_opt_non_regression},
        {"SA full reversal delta", test_sa_full_reversal_delta},
        {"GA crossover validity", test_genetic_crossover_validity},
        {"GA mutation validity", test_genetic_mutation_validity},
        {"GA keeps starting tour baseline", test_genetic_does_not_worsen_starting_tour},
        {"ACO zero-cost tours", test_aco_handles_zero_cost_tours},
        {"algorithm validity", test_algorithms_preserve_valid_tours}
    };

    for (const auto& [name, test]: tests) {
        try {
            test();
            std::cout << "[PASS] " << name << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << name << ": " << ex.what() << "\n";
            return 1;
        }
    }

    return 0;
}
