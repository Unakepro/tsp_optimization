#ifndef hyperparameter_search
#define hyperparameter_search

#include <iostream>
#include <fstream>
#include <sstream>
#include "../Cities/city.hpp"
#include "../sa/sim_an.hpp"
#include "../genetic/genetic.hpp"
#include "../aco/aco.hpp"

const int repeats = 1;


void log_result(std::ofstream& out, const std::string& method, const std::string& dataset, double param1, double param2, double param3, double avg_cost, double avg_time) {
    out << method << "," << dataset << "," << param1 << "," << param2 << "," << param3 << "," << avg_cost << "," << avg_time << std::endl;
}


void random_search_sa(std::vector<std::string>& datasets) {
    std::ofstream out("sa_random_search.csv");
    out << "algorithm,dataset,temperature,cooling,iterations,avg_cost,avg_time_sec\n";

    const int fixed_iterations = 500;
    const int random_trials = 20;

    std::uniform_int_distribution<> temp_dist(1000, 20000);
    std::uniform_real_distribution<> cooling_dist(0.9999, 0.99999);

    const int generations = 1000000;
    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < random_trials; ++t) {
            int temp = temp_dist(gen);
            double cooling = cooling_dist(gen);
            
            double cost_sum = 0.0;
            double time_sum = 0.0;

            for (int r = 0; r < repeats; ++r) {
                std::vector<City> cities;
                readfile(cities, file);

                auto start = std::chrono::high_resolution_clock::now();
                sa_optimization(cities, temp, 1e-3, cooling, generations);
                auto end = std::chrono::high_resolution_clock::now();

                cost_sum += total_cost(cities);
                time_sum += std::chrono::duration<double>(end - start).count();
            }

            double avg_cost = cost_sum / repeats;
            double avg_time = time_sum / repeats;

            log_result(out, "SA", name, temp, cooling, generations, avg_cost, avg_time);

            std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cooling
            << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
        }
    }

    out.close();
}

void grid_search_sa(std::vector<std::string>& datasets) {
    std::ofstream out("sa_grid_search.csv");
    
    out << "algorithm,dataset,temperature,cooling,iterations,avg_cost,avg_time_sec\n";

    std::vector<int> start_temperatures = {1000, 5000, 10000};
    std::vector<double> cooling_rates = {0.99, 0.999, 0.9999, 0.99999};
    
    const int generations = 1000000;
    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        for (auto temp : start_temperatures) {
            for (double cool_rate : cooling_rates) {
                double cost_sum = 0.0;
                double time_sum = 0.0;

                for (int r = 0; r < repeats; ++r) {
                    std::vector<City> cities;
                    readfile(cities, file);

                    auto start = std::chrono::high_resolution_clock::now();
                    sa_optimization(cities, temp, 1e-3, cool_rate, generations);
                    auto end = std::chrono::high_resolution_clock::now();

                    cost_sum += total_cost(cities);
                    time_sum += std::chrono::duration<double>(end - start).count();
                }

                double avg_cost = cost_sum / repeats;
                double avg_time = time_sum / repeats;

                log_result(out, "SA", name, temp, cool_rate, generations, avg_cost, avg_time);

                std::cout << "[SA] " << name << " temp=" << temp << " cool=" << cool_rate
                << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
            }
        }
    }

    out.close();
}


void random_search_genetic(std::vector<std::string>& datasets) {
    std::ofstream out("ga_random_search.csv");
    out << "algorithm,dataset,population,mutation,generations,avg_cost,avg_time_sec\n";

    const int fixed_iterations = 500;
    const int random_trials = 20;

    std::uniform_int_distribution<> pop_dist(10, 100);
    std::uniform_real_distribution<> mutation_dist(0.01, 0.1);

    const int generations = 1000;
    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < random_trials; ++t) {
            int pop = pop_dist(gen);
            double mut = mutation_dist(gen);
            
            double cost_sum = 0.0, time_sum = 0.0;
            for (int r = 0; r < repeats; ++r) {
                std::vector<City> cities;
                readfile(cities, file);

                auto start = std::chrono::high_resolution_clock::now();
                genetic_optimization(cities, mut, pop, generations);
                auto end = std::chrono::high_resolution_clock::now();

                cost_sum += total_cost(cities);
                time_sum += std::chrono::duration<double>(end - start).count();
            }

            double avg_cost = cost_sum / repeats;
            double avg_time = time_sum / repeats;

            log_result(out, "GA", name, pop, mut, generations, avg_cost, avg_time);

            std::cout << "[GA] " << name << " pop=" << pop << " mut=" << mut
                          << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
        }
    }

    out.close();
}

void grid_search_genetic(std::vector<std::string>& datasets) {
    std::ofstream out("ga_grid_search.csv");
    
    out << "algorithm,dataset,population,mutation,generations,avg_cost,avg_time_sec\n";

    std::vector<int> populations = {10, 50, 100};
    std::vector<double> mutation_rates = {0.01, 0.05, 0.1};
    
    const int generations = 1000;
    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);
        
        for (auto pop : populations) {
            for (double mut : mutation_rates) {
                double cost_sum = 0.0;
                double time_sum = 0.0;

                for (int r = 0; r < repeats; ++r) {
                    std::vector<City> cities;
                    readfile(cities, file);

                    auto start = std::chrono::high_resolution_clock::now();
                    genetic_optimization(cities, mut, pop, generations);
                    auto end = std::chrono::high_resolution_clock::now();

                    cost_sum += total_cost(cities);
                    time_sum += std::chrono::duration<double>(end - start).count();
                }

                double avg_cost = cost_sum / repeats;
                double avg_time = time_sum / repeats;

                log_result(out, "GA", name, pop, mut, generations, avg_cost, avg_time);

                std::cout << "[GA] " << name << " pop=" << pop << " mut=" << mut
                          << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
            }
        }
    }

    out.close();
}


void grid_search_aco(const std::vector<std::string>& datasets) {
    std::ofstream out("aco_grid_search.csv");
    out << "algorithm,dataset,ants,alpha,beta,evaporation,avg_cost,avg_time_sec\n";

    std::vector<int> ants_list = {10, 20, 40};
    std::vector<double> alpha_list = {1.0, 1.5};
    std::vector<double> beta_list = {3.0, 5.0};
    std::vector<double> evap_list = {0.1, 0.3, 0.5};

    const int iterations = 100;

    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);
        for (int ants : ants_list) {
            for (double alpha : alpha_list) {
                for (double beta : beta_list) {
                    for (double evap : evap_list) {
                        double cost_sum = 0.0, time_sum = 0.0;

                        for (int r = 0; r < repeats; ++r) {
                            std::vector<City> cities;
                            readfile(cities, file);

                            auto start = std::chrono::high_resolution_clock::now();

                            if(name == "rl5934.tsp" && r < 2) {
                                aco(cities, ants, alpha, beta, evap, 20);
                            }
                            else if(name == "rl5934.tsp") {
                                continue;
                            }
                            else {
                                aco(cities, ants, alpha, beta, evap, iterations);
                            }

                            auto end = std::chrono::high_resolution_clock::now();

                            cost_sum += total_cost(cities);
                            time_sum += std::chrono::duration<double>(end - start).count();
                        }

                        double avg_cost = cost_sum;
                        double avg_time = time_sum;

                        if(name == "rl5934.tsp") {
                            avg_cost /= 2;
                            avg_time /= 2;
                        }
                        else {
                            avg_cost /= repeats;
                            avg_time /= repeats;
                        }

                        out << "ACO," << name << "," << ants << "," << alpha << "," << beta << "," << evap
                            << "," << avg_cost << "," << avg_time << "\n";

                        std::cout << "[ACO] " << name << " ants=" << ants
                                  << " alpha=" << alpha << " beta=" << beta << " evap=" << evap
                                  << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
                    }
                }
            }
        }
    }

    out.close();
}

void random_search_aco(const std::vector<std::string>& datasets) {
    std::ofstream out("aco_random_search.csv");
    out << "algorithm,dataset,ants,alpha,beta,evaporation,avg_cost,avg_time_sec\n";

    const int iterations = 100;
    const int trials = 10;

    std::uniform_int_distribution<> ants_dist(10, 40);
    std::uniform_real_distribution<> alpha_dist(1.0, 2);
    std::uniform_real_distribution<> beta_dist(3.0, 7.0);
    std::uniform_real_distribution<> evap_dist(0.1, 0.5);

    for (const auto& file : datasets) {
        std::string name = file.substr(file.find_last_of('/') + 1);

        for (int t = 0; t < trials; ++t) {
            int ants = ants_dist(gen);
            double alpha = alpha_dist(gen);
            double beta = beta_dist(gen);
            double evap = evap_dist(gen);

            double cost_sum = 0.0, time_sum = 0.0;

            for (int r = 0; r < repeats; ++r) {
                std::vector<City> cities;
                readfile(cities, file);

                auto start = std::chrono::high_resolution_clock::now();

                if(name == "rl5934.tsp" && r < 2) {
                    aco(cities, ants, alpha, beta, evap, 20);
                }
                else if(name == "rl5934.tsp") {
                    continue;
                }
                else {
                    aco(cities, ants, alpha, beta, evap, iterations);
                }

                auto end = std::chrono::high_resolution_clock::now();

                cost_sum += total_cost(cities);
                time_sum += std::chrono::duration<double>(end - start).count();
            }

            double avg_cost = cost_sum;
            double avg_time = time_sum;

            if(name == "rl5934.tsp") {
                avg_cost /= 2;
                avg_time /= 2;
            }
            else {
                avg_cost /= repeats;
                avg_time /= repeats;
            }

            out << "ACO," << name << "," << ants << "," << alpha << "," << beta << "," << evap
                << "," << avg_cost << "," << avg_time << "\n";

            std::cout << "[ACO] " << name << " ants=" << ants
                      << " alpha=" << alpha << " beta=" << beta << " evap=" << evap
                      << " -> cost=" << avg_cost << ", time=" << avg_time << "s\n";
        }
    }

    out.close();
}



#endif