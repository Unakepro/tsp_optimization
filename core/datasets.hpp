#ifndef TSP_CORE_DATASETS_HPP
#define TSP_CORE_DATASETS_HPP

#include <string>
#include <vector>

struct Dataset {
    std::string name;
    std::string path;
    std::string size_class;
};


std::vector<Dataset> load_dataset_group(const std::string& group);
bool is_dataset_group(const std::string& group);

// Returns best known distance, 0.0 when the optimum is unknown.
double best_known_for(const std::string& instance_name);

#endif
