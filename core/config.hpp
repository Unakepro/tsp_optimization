#ifndef TSP_CORE_CONFIG
#define TSP_CORE_CONFIG

#include <filesystem>
#include <fstream>
#include <map>
#include <string>


struct SaParams {
    double start_temp = 10000.0;
    double end_temp = 1e-3;
    double cooling = 0.9999;
    bool two_opt = false;
};

struct GaParams {
    int population = 100;
    double mutation = 0.1;
    bool two_opt = true;
};

struct AcoParams {
    int ants = 20;
    double alpha = 1.0;
    double beta = 5.0;
    double evaporation = 0.3;
    bool two_opt = true;
};

std::filesystem::path project_root();
std::string trim(const std::string& text);
std::string bool_text(bool value);
std::ofstream open_output_file(const std::filesystem::path& directory, const std::string& filename);

using ConfigMap = std::map<std::string, std::string>;

ConfigMap read_config(const std::filesystem::path& path);
bool config_exists(const std::filesystem::path& path);

double parse_double(const std::string& text);
int parse_int(const std::string& text);

double config_double(const ConfigMap& values, const std::string& key);
int config_int(const ConfigMap& values, const std::string& key);
bool config_bool(const ConfigMap& values, const std::string& key);

SaParams sa_params_from(const ConfigMap& values);
GaParams ga_params_from(const ConfigMap& values);
AcoParams aco_params_from(const ConfigMap& values);

std::string describe(const SaParams& params);
std::string describe(const GaParams& params);
std::string describe(const AcoParams& params);

std::filesystem::path default_config_path(const std::string& algorithm);

#endif
