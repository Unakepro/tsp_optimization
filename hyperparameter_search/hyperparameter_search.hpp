#ifndef HYPERPARAMETER_SEARCH
#define HYPERPARAMETER_SEARCH

#include <cstdint>
#include <string>
#include <vector>

enum class DatasetSet {
    Tuning,
    FinalBenchmark
};

struct HyperparameterSearchConfig {
    std::uint32_t base_seed = 42;
    int repeats = 30;
    DatasetSet dataset_set = DatasetSet::Tuning;
    int evaluation_budget = 10000;
};

std::vector<std::string> tuning_datasets();
std::vector<std::string> final_benchmark_datasets();

void run_all_hyperparameter_searches(const HyperparameterSearchConfig& config);

#endif
