
/** @file main.cpp
 *  @brief Compare baseline and candidate-based steepest local search.
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "algorithms/local_search_steepest.hpp"
#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"

using clock_type = std::chrono::steady_clock;

struct local_search_method {
    std::string name;
    std::unique_ptr<LocalSearchSteepest> algorithm;
    bool use_candidates;
};

struct run_statistics {
    long long total_time = 0;
    long long min_time = std::numeric_limits<long long>::max();
    long long max_time = 0;

    long long sum_weight = 0;
    int max_weight = std::numeric_limits<int>::min();

    algorithm_result best;
};

int main() {
    constexpr unsigned seed = 42;
    constexpr int number_of_runs = 200;
    constexpr int candidate_list_size = 10;

    const std::vector<std::string> instances = {"TSPA", "TSPB"};

    const std::string output_dir = "data/lab_4/output";
    const std::string results_dir = "data/lab_4/results";

    constexpr const char* intra_method = "intra_edge_exchange";
    constexpr const char* inter_method = "inter_node_exchange";
    constexpr const char* initial_solution = "random";

    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    std::vector<local_search_method> methods;
    methods.reserve(2);

    methods.push_back({
        "steepest_edge_random_baseline",
        std::make_unique<LocalSearchSteepest>(seed),
        false
    });

    methods.push_back({
        "steepest_edge_random_candidates",
        std::make_unique<LocalSearchSteepest>(seed),
        true
    });

    const std::string statistics_file =
        results_dir + "/statistics.csv";

    std::ofstream statistics(statistics_file);

    if (!statistics) {
        throw std::runtime_error(
            "Could not open file: " + statistics_file);
    }

    statistics
        << "instance;method;min;max;average;"
        << "avg_time(microsec);min_time(microsec);max_time(microsec)\n";

    statistics << std::fixed << std::setprecision(2);

    for (const std::string& instance : instances) {
        const std::string input_file =
            "data/input/" + instance + ".csv";

        const algorithm_data data = parse_csv(input_file);

        // Candidate preprocessing is performed once per instance.
        const auto preprocessing_start = clock_type::now();

        const CandidateMoveGenerator candidates(
            data, candidate_list_size);

        const auto preprocessing_end = clock_type::now();

        const auto preprocessing_time =
            std::chrono::duration_cast<std::chrono::microseconds>(
                preprocessing_end - preprocessing_start).count();

        std::cout << instance
                  << ": candidate preprocessing = "
                  << preprocessing_time << " microseconds\n";

        for (auto& method : methods) {
            run_statistics stats;
            stats.best.total_weight =
                std::numeric_limits<int>::max();

            for (int run = 0; run < number_of_runs; ++run) {
                const auto start = clock_type::now();

                algorithm_result result;

                if (method.use_candidates) {
                    result = method.algorithm->run_with_candidates(
                        data,
                        intra_method,
                        inter_method,
                        initial_solution,
                        0,
                        candidates
                    );
                } else {
                    result = method.algorithm->run(
                        data,
                        intra_method,
                        inter_method,
                        initial_solution,
                        0
                    );
                }

                const auto end = clock_type::now();

                const long long elapsed =
                    std::chrono::duration_cast<
                        std::chrono::microseconds>(end - start).count();

                stats.total_time += elapsed;
                stats.min_time = std::min(stats.min_time, elapsed);
                stats.max_time = std::max(stats.max_time, elapsed);

                // Validation is deliberately outside the timed section.
                const std::string error = check_solution(data, result);

                if (!error.empty()) {
                    std::cerr
                        << "Invalid solution: "
                        << instance << ", "
                        << method.name << ", run "
                        << run + 1 << ": "
                        << error << '\n';

                    return 1;
                }

                stats.sum_weight += result.total_weight;
                stats.max_weight = std::max(
                    stats.max_weight, result.total_weight);

                if (result.total_weight < stats.best.total_weight) {
                    stats.best = std::move(result);
                }
            }

            const double average_weight =
                static_cast<double>(stats.sum_weight) /
                number_of_runs;

            const long long average_time =
                stats.total_time / number_of_runs;

            const std::string output_file =
                output_dir + "/" + instance + "_" +
                method.name + ".txt";

            method.algorithm->save_result_to_file(
                stats.best, output_file, input_file);

            statistics
                << instance << ';'
                << method.name << ';'
                << stats.best.total_weight << ';'
                << stats.max_weight << ';'
                << average_weight << ';'
                << average_time << ';'
                << stats.min_time << ';'
                << stats.max_time << '\n';

            statistics.flush();

            std::cout
                << instance << " " << method.name
                << ": best = " << stats.best.total_weight
                << ", average = " << average_weight
                << ", average time = " << average_time
                << " microseconds\n";
        }
    }

    std::cout << "All solutions passed the solution checker.\n";
    return 0;
}
