#include "sa.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr std::size_t TIME_CHECK_INTERVAL = 64;
constexpr std::size_t TWO_OPT_NEIGHBORS = 10;
constexpr std::size_t TWO_OPT_MOVES = 25;

std::pair<std::size_t, std::size_t> random_segment(std::size_t city_count) {
    std::uniform_int_distribution<std::size_t> dist(0, city_count - 1);
    std::size_t a = dist(gen);
    std::size_t b = dist(gen);
    while (a == b) {
        b = dist(gen);
    }
    if (a > b) {
        std::swap(a, b);
    }
    return {a, b};
}

bool accept_worse(double probability) {
    std::uniform_real_distribution<> dist(0.0, 1.0);
    return probability >= dist(gen);
}

void validate(const SaParams& p) {
    if (!std::isfinite(p.start_temp) || p.start_temp <= 0.0) {
        throw std::invalid_argument("SA start_temp must be finite and positive.");
    }
    if (!std::isfinite(p.end_temp) || p.end_temp <= 0.0 || p.end_temp >= p.start_temp) {
        throw std::invalid_argument("SA end_temp must be finite, positive and smaller than start_temp.");
    }
    if (!std::isfinite(p.cooling) || p.cooling <= 0.0 || p.cooling >= 1.0) {
        throw std::invalid_argument("SA cooling must be finite and between 0 and 1.");
    }
}

struct ChainResult {
    std::vector<City> best_tour;
    double best_cost = 0.0;
    bool completed = false;
};

ChainResult run_chain(const std::vector<City>& base_tour, const std::vector<double>& distance_matrix,
                      const std::vector<std::vector<std::size_t>>& neighbors,
                      const SaParams& params, RunController& controller) {
    std::vector<City> current = base_tour;
    std::shuffle(current.begin(), current.end(), gen);

    double current_cost = total_cost_unchecked(current, distance_matrix);
    std::vector<City> best = current;
    double best_cost = current_cost;
    double temperature = params.start_temp;
    std::size_t steps_since_time_check = 0;

    while (temperature > params.end_temp) {
        if (steps_since_time_check == 0 && controller.time_expired()) {
            return {best, total_cost_unchecked(best, distance_matrix), false};
        }

        const auto [start, end] = random_segment(current.size());
        const double delta = tour_reversal_delta(current, distance_matrix, start, end);
        const bool accepted = delta < 0.0 || accept_worse(std::exp(-delta / temperature));
        if (accepted) {
            std::reverse(current.begin() + static_cast<std::ptrdiff_t>(start),
                         current.begin() + static_cast<std::ptrdiff_t>(end + 1));
            current_cost += delta;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best = current;
            }
        }

        temperature = std::max(temperature * params.cooling, params.end_temp);
        steps_since_time_check = (steps_since_time_check + 1) % TIME_CHECK_INTERVAL;
    }

    if (params.two_opt && !controller.time_expired()) {
        two_opt_neighbors_unchecked(best, distance_matrix, neighbors, TWO_OPT_MOVES, &controller);
    }

    return {best, total_cost_unchecked(best, distance_matrix), true};
}

}

double tour_reversal_delta(const std::vector<City>& cities, const std::vector<double>& distance_matrix, std::size_t start, std::size_t end) {
    const std::size_t n = cities.size();

    if (n < 2) {
        throw std::invalid_argument("Reversal delta requires at least two cities.");
    }
    if (distance_matrix.size() != n * n) {
        throw std::invalid_argument("Distance matrix size does not match tour size.");
    }
    if (start >= n || end >= n || start > end) {
        throw std::invalid_argument("Reversal delta segment is out of range.");
    }
    if (start == 0 && end + 1 == n) {
        return 0.0;
    }

    const std::size_t before_start = start == 0 ? n - 1 : start - 1;
    const std::size_t after_end = (end + 1) % n;

    const auto a = static_cast<std::size_t>(cities[before_start].id - 1);
    const auto b = static_cast<std::size_t>(cities[start].id - 1);
    const auto c = static_cast<std::size_t>(cities[end].id - 1);
    const auto d = static_cast<std::size_t>(cities[after_end].id - 1);

    const double old_cost = distance_matrix[a * n + b] + distance_matrix[c * n + d];
    const double new_cost = distance_matrix[a * n + c] + distance_matrix[b * n + d];

    return new_cost - old_cost;
}

SolveResult sa_solve(std::vector<City>& cities, const SaParams& params, const StopCondition& stop) {
    validate_tour_input(cities, "Simulated annealing");
    validate(params);

    RunController controller(stop);
    controller.start();

    const std::vector<double> distance_matrix = build_distance_matrix(cities);
    const std::vector<City> base_tour = cities;
    const std::vector<std::vector<std::size_t>> neighbors =
        params.two_opt ? build_neighbor_lists(distance_matrix, cities.size(), TWO_OPT_NEIGHBORS)
                       : std::vector<std::vector<std::size_t>>{};

    std::vector<City> global_best = cities;

    double global_best_cost = total_cost_unchecked(global_best, distance_matrix);

    std::size_t attempted_restarts = 0;
    std::size_t completed_restarts = 0;
    StopReason stop_reason = StopReason::None;

    const bool timed_mode = std::isfinite(stop.max_seconds);

    while ((timed_mode && attempted_restarts == 0) || controller.next(global_best_cost)) {
        ++attempted_restarts;

        ChainResult chain = run_chain(base_tour, distance_matrix, neighbors, params, controller);

        if (!chain.completed) {
            stop_reason = StopReason::TimeLimit;
            break;
        }
        if (chain.best_cost < global_best_cost) {
            global_best_cost = chain.best_cost;
            global_best = std::move(chain.best_tour);
        }

        ++completed_restarts;

        if (stop.progress_interval > 0 && stop.progress_callback &&
            completed_restarts % stop.progress_interval == 0) {
            stop.progress_callback(completed_restarts, global_best_cost);
        }
    }

    if (stop_reason == StopReason::None) {
        stop_reason = controller.stop_reason();
    }

    global_best_cost = total_cost_unchecked(global_best, distance_matrix);
    cities = global_best;

    return {global_best_cost, attempted_restarts, controller.converged(), stop_reason, completed_restarts};
}
