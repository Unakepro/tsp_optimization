#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "../algorithms/aco.hpp"
#include "../algorithms/genetic.hpp"
#include "../algorithms/sa.hpp"
#include "../core/config.hpp"
#include "../core/tsp.hpp"

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool rejects_invalid_argument(const std::function<void()>& action) {
    try {
        action();
    }
    catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

void run_full_two_opt(std::vector<City>& path, const std::vector<double>& distance_matrix) {
    if (path.size() < 4) {
        return;
    }

    const auto neighbors = build_neighbor_lists(distance_matrix, path.size(), path.size() - 1);

    two_opt_neighbors(path, distance_matrix, neighbors, std::numeric_limits<std::size_t>::max());
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

void test_tsplib_parser_rejects_trailing_coordinate_data() {
    const auto path = temp_file("tsp_parser_trailing.tsp");

    write_text_file(path,
                    "NAME: parser_trailing\n"
                    "TYPE: TSP\n"
                    "DIMENSION: 3\n"
                    "EDGE_WEIGHT_TYPE: EUC_2D\n"
                    "NODE_COORD_SECTION\n"
                    "1 0 0 extra\n"
                    "2 1 1\n"
                    "3 2 2\n"
                    "EOF\n");

    std::vector<City> cities;
    bool parser_rejected = false;

    try {
        readfile(cities, path.string());
    }
    catch (const std::runtime_error&) {
        parser_rejected = true;
    }
    std::filesystem::remove(path);

    expect(parser_rejected, "parser should reject trailing columns in coordinate lines");
}

void test_tsplib_parser_requires_exact_section_tokens() {
    const auto path = temp_file("tsp_parser_bad_section.tsp");

    write_text_file(path,
                    "NAME: parser_bad_section\n"
                    "TYPE: TSP\n"
                    "DIMENSION: 3\n"
                    "EDGE_WEIGHT_TYPE: EUC_2D\n"
                    "NODE_COORD_SECTION_EXTRA\n"
                    "1 0 0\n"
                    "2 1 1\n"
                    "3 2 2\n"
                    "EOF\n");

    std::vector<City> cities;
    bool parser_rejected = false;

    try {
        readfile(cities, path.string());
    }
    catch (const std::runtime_error&) {
        parser_rejected = true;
    }
    std::filesystem::remove(path);

    expect(parser_rejected, "parser should require an exact NODE_COORD_SECTION token");
}

void test_distance_and_total_cost() {
    const auto cities = rectangle_tour();

    expect(tsplib_distance(cities[0], cities[2]) == 5, "EUC_2D distance should round using TSPLIB rules");
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
    const auto neighbors = build_neighbor_lists(distance_matrix, invalid.size(), 3);
    expect(rejects_invalid_argument([&]() { two_opt_neighbors(invalid, distance_matrix, neighbors, 1); }), "two-opt should reject out-of-range city ids");
}

void test_two_opt_non_regression() {
    auto cities = crossing_tour();
    const auto distance_matrix = build_distance_matrix(cities);
    const double before = total_cost(cities, distance_matrix);

    run_full_two_opt(cities, distance_matrix);

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
    ga_solve(tour, GaParams{9, 0.35, false}, iteration_limit(1));

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
    aco_solve(cities, AcoParams{4, 1.0, 2.0, 0.2, false}, iteration_limit(5));

    expect(is_valid_tour(cities), "ACO should preserve tour validity when all distances are zero");
    expect(std::isfinite(total_cost(cities)), "ACO should return a finite cost when all distances are zero");
}

void test_algorithms_preserve_valid_tours() {
    const auto base = sample_cities();

    auto sa_tour = base;
    set_random_seed(101);
    sa_solve(sa_tour, SaParams{100.0, 0.01, 0.95, true}, iteration_limit(2));
    expect(is_valid_tour(sa_tour), "simulated annealing should preserve tour validity");
    expect(std::isfinite(total_cost(sa_tour)), "simulated annealing should return a finite tour cost");

    auto ga_tour = base;
    set_random_seed(202);
    ga_solve(ga_tour, GaParams{9, 0.35, false}, iteration_limit(25));
    expect(is_valid_tour(ga_tour), "genetic algorithm should preserve tour validity for odd population sizes");
    expect(std::isfinite(total_cost(ga_tour)), "genetic algorithm should return a finite tour cost");

    auto aco_tour = base;
    std::rotate(aco_tour.begin(), aco_tour.begin() + 3, aco_tour.end());
    set_random_seed(303);
    aco_solve(aco_tour, AcoParams{1, 1.0, 2.0, 0.2, false}, iteration_limit(8));
    expect(is_valid_tour(aco_tour), "ACO should preserve tour validity for unsorted input and one ant");
    expect(std::isfinite(total_cost(aco_tour)), "ACO should return a finite tour cost");
}

void test_sa_config_reads_two_opt() {
    const ConfigMap values = {
        {"start_temp", "100"},
        {"end_temp", "0.1"},
        {"cooling", "0.5"},
        {"two_opt", "true"}
    };

    const SaParams params = sa_params_from(values);

    expect(params.two_opt, "SA config should read the two_opt option");
    expect(describe(params).find("two_opt=true") != std::string::npos,
           "SA parameter description should include two_opt");
}

std::vector<City> random_instance(std::size_t n, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> coord(0.0, 1000.0);
    std::vector<City> cities;

    cities.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        cities.push_back({static_cast<int>(i + 1), {coord(rng), coord(rng)}});
    }

    return cities;
}


bool has_improving_two_opt(const std::vector<City>& path, const std::vector<double>& dist) {
    const std::size_t n = path.size();

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t a = static_cast<std::size_t>(path[i].id - 1);
        const std::size_t b = static_cast<std::size_t>(path[(i + 1) % n].id - 1);

        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) {
                continue;
            }

            const std::size_t c = static_cast<std::size_t>(path[j].id - 1);
            const std::size_t d = static_cast<std::size_t>(path[(j + 1) % n].id - 1);
            const double before = dist[a * n + b] + dist[c * n + d];
            const double after = dist[a * n + c] + dist[b * n + d];

            if (after + 1e-7 < before) {
                return true;
            }
        }
    }

    return false;
}

void test_neighbor_two_opt_reaches_local_optimum() {
    for (std::uint32_t seed = 1; seed <= 6; ++seed) {
        auto cities = random_instance(35, seed);
        const auto dist = build_distance_matrix(cities);

        set_random_seed(seed);
        std::shuffle(cities.begin(), cities.end(), gen);
        const double start = total_cost(cities, dist);

        run_full_two_opt(cities, dist);
        const double end = total_cost(cities, dist);

        expect(is_valid_tour(cities), "neighbor 2-opt must preserve a valid permutation");
        expect(end <= start + 1e-9, "neighbor 2-opt must never worsen the tour");
        expect(!has_improving_two_opt(cities, dist),
               "neighbor 2-opt with full lists must reach a true 2-opt local optimum");
    }
}

void test_bounded_neighbor_two_opt_is_safe() {
    auto cities = random_instance(40, 123);
    const auto dist = build_distance_matrix(cities);
    const auto neighbors = build_neighbor_lists(dist, cities.size(), 8);

    set_random_seed(123);
    std::shuffle(cities.begin(), cities.end(), gen);
    const double start = total_cost(cities, dist);

    const std::size_t moves = two_opt_neighbors(cities, dist, neighbors, 5);
    expect(moves <= 5, "bounded 2-opt must respect the move cap");
    expect(is_valid_tour(cities), "bounded neighbor 2-opt must preserve a valid permutation");
    expect(total_cost(cities, dist) <= start + 1e-9, "bounded neighbor 2-opt must never worsen the tour");
}

void test_run_controller_detects_stability() {
    RunController controller(until_stable(/*min_iters=*/3, /*window=*/2, /*epsilon=*/0.001,
                                         /*plateau_seconds=*/0.0, 1000000));
    controller.start();

    expect(controller.next(100.0), "first iteration sets the baseline and continues");
    expect(controller.next(90.0), "an improvement before min-iters should continue");
    expect(controller.next(90.0), "stable mode should still respect min-iters");
    expect(controller.next(90.0), "stable mode should anchor its first check after min-iters");
    expect(controller.next(90.0), "stable mode should observe a full window before stopping");
    expect(!controller.next(90.0), "a flat stable window should stop the run");
    expect(controller.converged(), "a stable stop must report stability");
    expect(controller.stop_reason() == StopReason::Stable, "stable stop should report the stable reason");
}

void test_run_controller_iteration_limit_is_not_stable() {
    RunController controller(iteration_limit(3));
    controller.start();

    std::size_t loops = 0;
    while (controller.next(100.0 - static_cast<double>(loops))) {
        ++loops;
    }

    expect(loops == 3, "iteration_limit(3) should permit exactly three iterations");
    expect(controller.iterations() == 3, "iterations() should report the number of iterations run");
    expect(!controller.converged(), "an iteration-limited stop must not report stability");
    expect(controller.stop_reason() == StopReason::IterationLimit, "iteration limit should report its stop reason");
}

void test_run_controller_stable_window() {
    RunController controller(until_stable(/*min_iters=*/20, /*window=*/10, /*epsilon=*/0.01,
                                         /*plateau_seconds=*/0.0, 1000000));
    controller.start();

    double best = 1000.0;
    std::size_t guard = 0;
    while (controller.next(best)) {
        if (controller.iterations() < 50) {
            best *= 0.99;
        }
        if (++guard > 1000000) {
            break;
        }
    }

    expect(controller.converged(), "stable mode should report stability once improvement stalls");
    expect(controller.stop_reason() == StopReason::Stable, "stable mode should report the stable stop reason");
    expect(controller.iterations() >= 50, "stable mode must not converge during active improvement");
    expect(guard < 1000000, "stable mode must terminate without hitting the safety cap");
}

void test_run_controller_plateau_grace_period() {
    RunController controller(until_stable(/*min_iters=*/3, /*window=*/2, /*epsilon=*/0.001,
                                         /*plateau_seconds=*/0.03, 1000000));
    controller.start();

    expect(controller.next(100.0), "first iteration should continue");
    expect(controller.next(90.0), "improvement before min-iters should continue");
    expect(controller.next(90.0), "min-iters boundary should continue");
    expect(controller.next(90.0), "stable window should anchor");
    expect(controller.next(90.0), "stable window should keep running");
    expect(controller.next(90.0), "plateau detection should start the grace period, not stop immediately");
    expect(!controller.converged(), "plateau grace period should delay stable stop");

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    expect(!controller.next(90.0), "stable mode should stop after the plateau grace period");
    expect(controller.stop_reason() == StopReason::Stable, "plateau grace period should end with stable reason");
}

void test_timed_mode_runs_and_stops() {
    auto cities = random_instance(60, 7);
    const double budget_seconds = 0.05;

    set_random_seed(7);
    const auto t0 = std::chrono::steady_clock::now();
    const SolveResult result = sa_solve(cities, SaParams{100.0, 0.1, 0.5}, time_limit(budget_seconds));
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    expect(is_valid_tour(cities), "a timed run should return a valid tour");
    expect(std::isfinite(result.cost) && result.cost > 0.0, "a timed run should return a finite positive cost");
    expect(result.restarts > 1, "timed SA should use the budget for repeated annealing restarts");
    expect(result.iterations >= result.restarts, "SA iterations should include attempted restart chains");
    expect(!result.converged, "a time-limited stop must not report stability");
    expect(result.stop_reason == StopReason::TimeLimit, "a timed run should stop by time limit");
    expect(elapsed < budget_seconds + 1.0, "a timed run should stop close to its wall-clock budget");
}

void test_sa_berlin52_reaches_known_optimum() {
    std::vector<City> cities;
    readfile(cities, (project_root() / "tsplib" / "tests" / "berlin52.tsp").string());

    set_random_seed(derive_run_seed(42, 0x005Au, 0, 0));
    const SolveResult result = sa_solve(cities, SaParams{10000.0, 0.001, 0.99999, false}, iteration_limit(3));

    expect(result.cost == 7542.0, "SA berlin52 end-to-end run should reach the known optimum");
    expect(total_cost(cities) == 7542.0, "SA berlin52 returned tour should have zero gap");
    expect(is_valid_tour(cities), "SA berlin52 result should be a valid tour");
}

void test_sa_stable_mode_uses_restart_stagnation() {
    auto cities = random_instance(30, 5);

    set_random_seed(5);
    const SolveResult result = sa_solve(cities, SaParams{100.0, 0.1, 0.5},
                                        until_stable(/*min_iters=*/2, /*window=*/2, /*epsilon=*/1000.0,
                                                     /*plateau_seconds=*/0.0, 1000000));

    expect(result.converged, "SA stable mode should report stability");
    expect(result.stop_reason == StopReason::Stable, "SA stable mode should stop by restart-level stability");
    expect(result.restarts == 4, "SA stable mode should check stagnation only after completed restarts");
    expect(result.iterations == result.restarts, "SA iterations should report completed restarts");
    expect(is_valid_tour(cities), "SA stable result should be a valid tour");
}

void test_derive_run_seed_is_deterministic_and_distinct() {
    const std::uint32_t base = derive_run_seed(42, 0x005Au, 0, 0);
    expect(derive_run_seed(42, 0x005Au, 0, 0) == base, "derive_run_seed must be deterministic for identical inputs");
    expect(derive_run_seed(43, 0x005Au, 0, 0) != base, "a different base seed must change the run seed");
    expect(derive_run_seed(42, 0x006Au, 0, 0) != base, "a different algorithm id must change the run seed");
    expect(derive_run_seed(42, 0x005Au, 1, 0) != base, "a different dataset index must change the run seed");
    expect(derive_run_seed(42, 0x005Au, 0, 1) != base, "a different repeat index must change the run seed");
}

}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"TSPLIB parser", test_tsplib_parser},
        {"TSPLIB parser rejects duplicates", test_tsplib_parser_rejects_duplicates},
        {"TSPLIB parser rejects trailing coordinate data", test_tsplib_parser_rejects_trailing_coordinate_data},
        {"TSPLIB parser requires exact section tokens", test_tsplib_parser_requires_exact_section_tokens},
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
        {"algorithm validity", test_algorithms_preserve_valid_tours},
        {"SA config reads two-opt", test_sa_config_reads_two_opt},
        {"neighbor 2-opt reaches local optimum", test_neighbor_two_opt_reaches_local_optimum},
        {"bounded neighbor 2-opt is safe", test_bounded_neighbor_two_opt_is_safe},
        {"RunController detects stability", test_run_controller_detects_stability},
        {"RunController iteration limit is not stable", test_run_controller_iteration_limit_is_not_stable},
        {"RunController stable window", test_run_controller_stable_window},
        {"RunController plateau grace period", test_run_controller_plateau_grace_period},
        {"timed stop mode runs and stops", test_timed_mode_runs_and_stops},
        {"SA berlin52 reaches known optimum", test_sa_berlin52_reaches_known_optimum},
        {"SA stable mode uses restart stagnation", test_sa_stable_mode_uses_restart_stagnation},
        {"derive_run_seed deterministic and distinct", test_derive_run_seed_is_deterministic_and_distinct}
    };

    for (const auto& [name, test]: tests) {
        try {
            test();
            std::cout << "[PASS] " << name << "\n";
        }
        catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << name << ": " << ex.what() << "\n";
            return 1;
        }
    }

    return 0;
}
