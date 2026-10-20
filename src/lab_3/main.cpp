/** @file main.cpp
 *  @brief Main file for running all algorithms implemented in this lab.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include "algorithms/local_search_greedy.hpp"
#include "algorithms/local_search_steepest.hpp"
#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"

/// One of the 8 configurations: search type x intra neighbourhood x initial solution.
struct local_search_method {
    std::string name;
    std::unique_ptr<LocalSearchBase> algorithm;
    std::string intra_method;
    std::string initial_solution;
};

// Run from the repository root
int main() {
    const unsigned seed = 42;
    const std::vector<std::string> instances = {"TSPA", "TSPB"};
    const std::string output_dir = "data/lab_3/output";
    const std::string results_dir = "data/lab_3/results";
    const std::string inter_method = "inter_node_exchange";
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    const std::vector<std::string> search_types = {"greedy", "steepest"};
    // {short name used in method name, name passed to the algorithm}
    const std::vector<std::pair<std::string, std::string>> intra_methods = {
        {"node", "intra_node_exchange"}, {"edge", "intra_edge_exchange"}};
    const std::vector<std::pair<std::string, std::string>> initial_solutions = {
        {"random", "random"}, {"greedy_cycle", "greedy_cycle"}};

    std::vector<local_search_method> methods;
    for (const std::string& search_type : search_types) {
        for (const auto& [intra_name, intra_method] : intra_methods) {
            for (const auto& [initial_name, initial_solution] : initial_solutions) {
                std::unique_ptr<LocalSearchBase> algorithm;
                if (search_type == "greedy") {
                    algorithm = std::make_unique<LocalSearchGreedy>(seed);
                } else {
                    algorithm = std::make_unique<LocalSearchSteepest>(seed);
                }
                methods.push_back({search_type + "_" + intra_name + "_" + initial_name,
                                   std::move(algorithm), intra_method, initial_solution});
            }
        }
    }

    const std::string statistics_file = results_dir + "/statistics.csv";
    std::ofstream statistics(statistics_file);
    if (!statistics.is_open()) {
        throw std::runtime_error("Could not open file: " + statistics_file);
    }
    statistics << "instance;method;min;max;average;avg_time(microsec);min_time(microsec);max_time(microsec)\n";
    statistics << std::fixed << std::setprecision(2);
    std::cout << std::fixed << std::setprecision(2);

    for (const std::string& instance : instances) {
        const std::string input_file = "data/input/" + instance + ".csv";
        const algorithm_data data = parse_csv(input_file);

        for (const local_search_method& method : methods) {
            long long average_time(0);
            long long min_time = std::numeric_limits<long long>::max();
            long long max_time = 0;

            algorithm_result best;
            best.total_weight = std::numeric_limits<int>::max();
            int max_weight = std::numeric_limits<int>::min();
            long long sum_weight = 0;

            // greedy cycle start: once from each node; random start: same number of runs
            for (int i = 0; i < data.nodes_num; ++i) {
                const auto start_time = std::chrono::high_resolution_clock::now();
                algorithm_result result = method.algorithm->run(
                    data, method.intra_method, inter_method, method.initial_solution, i);
                const auto end_time = std::chrono::high_resolution_clock::now();
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                average_time += elapsed.count();
                min_time = std::min(min_time, static_cast<long long>(elapsed.count()));
                max_time = std::max(max_time, static_cast<long long>(elapsed.count()));
                const std::string error = check_solution(data, result);
                if (!error.empty()) {
                    std::cerr << "Invalid solution (" << instance << ", " << method.name << "): " << error << '\n';
                    return 1;
                }
                sum_weight += result.total_weight;
                max_weight = std::max(max_weight, result.total_weight);
                if (result.total_weight < best.total_weight) {
                    best = std::move(result);
                }
            }
            const double average_weight = static_cast<double>(sum_weight) / data.nodes_num;

            method.algorithm->save_result_to_file(best, output_dir + "/" + instance + "_" + method.name + ".txt", input_file);
            statistics << instance << ';' << method.name << ';' << best.total_weight << ';'
                       << max_weight << ';' << average_weight << ';' << average_time / data.nodes_num << ';'
                       << min_time << ';' << max_time << '\n';
            std::cout << instance << " " << method.name << ": min " << best.total_weight
                      << ", max " << max_weight << ", average " << average_weight
                      << ", average time " << average_time / data.nodes_num << " μs\n";
        }
    }

    std::cout << "All solutions passed the solution checker\n";
    return 0;
}
