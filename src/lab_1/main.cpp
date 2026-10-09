/** @file main.cpp
 *  @brief Main file for running all algorithms implemented in this lab.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include "algorithms/random.hpp"
#include "algorithms/nearest_endpoint.hpp"
#include "algorithms/nearest_anypoint.hpp"
#include "algorithms/greedy_cycle.hpp"
#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"

// Run from the repository root
int main() {
    const unsigned seed = 42;
    const std::vector<std::string> instances = {"TSPA", "TSPB"};
    const std::string output_dir = "data/lab_1/output";
    const std::string results_dir = "data/lab_1/results";
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    RandomAlgorithm random_algorithm(seed);
    NearestEndpointAlgorithm nearest_endpoint(seed);
    NearestAnypointAlgorithm nearest_anypoint(seed);
    GreedyCycleAlgorithm greedy_cycle(seed);
    const std::vector<std::pair<std::string, AlgorithmBase*>> methods = {
        {"random", &random_algorithm},
        {"nearest_endpoint", &nearest_endpoint},
        {"nearest_anypoint", &nearest_anypoint},
        {"greedy_cycle", &greedy_cycle},
    };

    const std::string statistics_file = results_dir + "/statistics.csv";
    std::ofstream statistics(statistics_file);
    if (!statistics.is_open()) {
        throw std::runtime_error("Could not open file: " + statistics_file);
    }
    statistics << "instance;method;min;max;average;avg_time(microsec)\n";
    statistics << std::fixed << std::setprecision(2);
    std::cout << std::fixed << std::setprecision(2);

    for (const std::string& instance : instances) {
        const std::string input_file = "data/input/" + instance + ".csv";
        const algorithm_data data = parse_csv(input_file);
        long long average_time(0);

        for (const auto& [method_name, algorithm] : methods) {
            // greedy methods start once from each node, random is just run the same number of times
            auto* start_node_algorithm = dynamic_cast<StartNodeAlgorithm*>(algorithm);

            algorithm_result best;
            best.total_weight = std::numeric_limits<int>::max();
            int max_weight = std::numeric_limits<int>::min();
            long long sum_weight = 0;

            for (int i = 0; i < data.nodes_num; ++i) {
                const auto start_time = std::chrono::high_resolution_clock::now();
                algorithm_result result = start_node_algorithm ? start_node_algorithm->run(data, i)
                                                               : algorithm->run(data);
                const auto end_time = std::chrono::high_resolution_clock::now();
                const auto elapsed = 
                    std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                average_time += elapsed.count();
                const std::string error = check_solution(data, result);
                if (!error.empty()) {
                    std::cerr << "Invalid solution (" << instance << ", " << method_name << "): " << error << '\n';
                    return 1;
                }
                sum_weight += result.total_weight;
                max_weight = std::max(max_weight, result.total_weight);
                if (result.total_weight < best.total_weight) {
                    best = std::move(result);
                }
            }
            const double average_weight = static_cast<double>(sum_weight) / data.nodes_num;

            algorithm->save_result_to_file(best, output_dir + "/" + instance + "_" + method_name + ".txt", input_file);
            statistics << instance << ';' << method_name << ';' << best.total_weight << ';'
                       << max_weight << ';' << average_weight << ';' << average_time / data.nodes_num << '\n';
            std::cout << instance << " " << method_name << ": min " << best.total_weight
                      << ", max " << max_weight << ", average " << average_weight 
                      << ", average time " << average_time / data.nodes_num << " μs\n";
        }
    }

    std::cout << "All solutions passed the solution checker\n";
    return 0;
}
