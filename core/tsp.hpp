#ifndef TSP_CORE_TSP
#define TSP_CORE_TSP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

inline constexpr std::uint32_t DEFAULT_RANDOM_SEED = 42;

extern std::mt19937 gen;
void set_random_seed(std::uint32_t seed);

std::uint32_t derive_run_seed(std::uint32_t base_seed, std::uint32_t algorithm_id,
                              std::size_t dataset_index, std::size_t repeat_index);

struct City {
    int id;
    std::pair<double, double> point;

    bool operator<(const City& other) const { return id < other.id; }
    bool operator==(const City& other) const { return id == other.id && point == other.point; }
};

int tsplib_distance(const City& a, const City& b);
void validate_tour_input(const std::vector<City>& cities, const std::string& algorithm_name);
bool is_valid_tour(const std::vector<City>& cities);

std::vector<double> build_distance_matrix(const std::vector<City>& cities);

double total_cost(const std::vector<City>& cities);
double total_cost(const std::vector<City>& cities, const std::vector<double>& distance_matrix);
double total_cost_unchecked(const std::vector<City>& cities, const std::vector<double>& distance_matrix);

void readfile(std::vector<City>& cities, const std::string& filename);

class RunController;

std::vector<std::vector<std::size_t>> build_neighbor_lists(const std::vector<double>& distance_matrix,
                                                           std::size_t n, std::size_t k);

std::size_t two_opt_neighbors(std::vector<City>& tour, const std::vector<double>& distance_matrix,
                              const std::vector<std::vector<std::size_t>>& neighbors, std::size_t max_moves,
                              const RunController* controller = nullptr);

std::size_t two_opt_neighbors_unchecked(std::vector<City>& tour, const std::vector<double>& distance_matrix,
                                        const std::vector<std::vector<std::size_t>>& neighbors,
                                        std::size_t max_moves, const RunController* controller = nullptr);

enum class StopReason {
    None,
    Stable,
    TimeLimit,
    IterationLimit
};

struct StopCondition {
    double max_seconds = std::numeric_limits<double>::infinity();
    std::size_t max_iters = std::numeric_limits<std::size_t>::max();
    std::size_t min_iters = 0;
    std::size_t stable_window = 0;

    double improvement_eps = 0.0;
    double plateau_seconds = 0.0;

    std::size_t progress_interval = 0;
    std::function<void(std::size_t, double)> progress_callback;
};

StopCondition time_limit(double seconds);
StopCondition iteration_limit(std::size_t iters);
StopCondition until_stable(std::size_t min_iters, std::size_t stable_window, double improvement_eps,
                           double plateau_seconds, std::size_t safety_iters);

struct SolveResult {
    double cost = 0.0;
    std::size_t iterations = 0;
    bool converged = false;
    StopReason stop_reason = StopReason::None;

    std::size_t restarts = 0;
};

class RunController {
public:
    explicit RunController(const StopCondition& stop);

    void start();

    bool next(double best_cost, bool stable_ready = true);
    bool time_expired() const;
    std::size_t iterations() const { return iters_; }

    bool converged() const { return converged_; }
    StopReason stop_reason() const { return stop_reason_; }
    double elapsed() const;

    SolveResult result(double best_cost) const;

private:
    StopCondition stop_;

    std::chrono::steady_clock::time_point t0_{};

    double best_so_far_ = 0.0;
    std::size_t iters_ = 0;
    std::size_t check_iter_ = 0;
    double check_cost_ = 0.0;

    bool stable_started_ = false;
    std::chrono::steady_clock::time_point plateau_t0_{};
    bool plateau_timer_started_ = false;

    bool timer_started_ = false;
    bool started_ = false;
    bool converged_ = false;

    StopReason stop_reason_ = StopReason::None;
};

#endif
