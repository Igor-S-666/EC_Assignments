
/** @file main.cpp
 *  @brief Compare baseline and candidate-based steepest local search,
 *         and sweep the candidate list size K.
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
#include <utility>
#include <vector>

#include "algorithms/local_search_steepest.hpp"
#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"

using clock_type = std::chrono::steady_clock;

struct run_statistics {
    long long total_time = 0;
    long long min_time = std::numeric_limits<long long>::max();
    long long max_time = 0;

    long long sum_weight = 0;
    int max_weight = std::numeric_limits<int>::min();

    algorithm_result best{{}, std::numeric_limits<int>::max()};

    void add(algorithm_result result, long long elapsed) {
        total_time += elapsed;
        min_time = std::min(min_time, elapsed);
        max_time = std::max(max_time, elapsed);

        sum_weight += result.total_weight;
        max_weight = std::max(max_weight, result.total_weight);
        if (result.total_weight < best.total_weight) {
            best = std::move(result);
        }
    }

    void write(std::ostream& out, int runs) const {
        out << best.total_weight << ';' << max_weight << ';'
            << static_cast<double>(sum_weight) / runs << ';'
            << static_cast<double>(total_time) / runs << ';'
            << min_time << ';' << max_time << '\n';
    }
};

struct method {
    std::string name;
    int k;  // candidate list size, 0 = full neighbourhood (baseline)
    bool in_sweep;  // reported in k_sweep.csv instead of statistics.csv
    // One object per method, with the same seed, kept across instances (as in
    // Lab 3): every method gets the same sequence of random starting solutions,
    // and the baseline reproduces the Lab 3 results.
    std::unique_ptr<LocalSearchSteepest> algorithm;
};

int main() {
    constexpr unsigned seed = 42;
    constexpr int number_of_runs = 200;
    constexpr int candidate_list_size = 10;
    constexpr int sweep_min_k = 1;
    constexpr int sweep_max_k = 20;

    const std::vector<std::string> instances = {"TSPA", "TSPB"};

    const std::string output_dir = "data/lab_4/output";
    const std::string results_dir = "data/lab_4/results";

    constexpr const char* intra_method = "intra_edge_exchange";
    constexpr const char* inter_method = "inter_node_exchange";
    constexpr const char* initial_solution = "random";

    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    std::vector<method> methods;
    methods.push_back({"steepest_edge_random_baseline", 0, false,
                       std::make_unique<LocalSearchSteepest>(seed)});
    methods.push_back({"steepest_edge_random_candidates", candidate_list_size, false,
                       std::make_unique<LocalSearchSteepest>(seed)});
    for (int k = sweep_min_k; k <= sweep_max_k; ++k) {
        methods.push_back({"candidates_k" + std::to_string(k), k, true,
                           std::make_unique<LocalSearchSteepest>(seed)});
    }

    const std::string statistics_file = results_dir + "/statistics.csv";
    const std::string sweep_file = results_dir + "/k_sweep.csv";

    std::ofstream statistics(statistics_file);
    std::ofstream sweep(sweep_file);
    if (!statistics) {
        throw std::runtime_error("Could not open file: " + statistics_file);
    }
    if (!sweep) {
        throw std::runtime_error("Could not open file: " + sweep_file);
    }

    statistics
        << "instance;method;min;max;average;"
        << "avg_time(microsec);min_time(microsec);max_time(microsec)\n";
    sweep
        << "instance;k;min;max;average;"
        << "avg_time(microsec);min_time(microsec);max_time(microsec)\n";
    statistics << std::fixed << std::setprecision(2);
    sweep << std::fixed << std::setprecision(2);

    for (const std::string& instance : instances) {
        const std::string input_file = "data/input/" + instance + ".csv";
        const algorithm_data data = parse_csv(input_file);

        // Candidate lists are built once per instance and K (not timed per run).
        std::vector<std::unique_ptr<CandidateMoveGenerator>> generators;
        for (const method& m : methods) {
            if (m.k == 0) {
                generators.push_back(nullptr);
                continue;
            }
            const auto start = clock_type::now();
            generators.push_back(std::make_unique<CandidateMoveGenerator>(data, m.k));
            if (m.k == candidate_list_size && !m.in_sweep) {
                std::cout << instance << ": candidate preprocessing (K=" << m.k << ") = "
                          << std::chrono::duration_cast<std::chrono::microseconds>(
                                 clock_type::now() - start).count()
                          << " microseconds\n";
            }
        }

        // Methods are interleaved run by run, so fluctuations of the machine
        // load affect all of them equally instead of distorting single methods.
        std::vector<run_statistics> stats(methods.size());
        for (int run = 0; run < number_of_runs; ++run) {
            for (std::size_t i = 0; i < methods.size(); ++i) {
                LocalSearchSteepest& algorithm = *methods[i].algorithm;

                const auto start = clock_type::now();
                algorithm_result result = generators[i]
                    ? algorithm.run_with_candidates(data, intra_method, inter_method,
                                                    initial_solution, 0, *generators[i])
                    : algorithm.run(data, intra_method, inter_method,
                                    initial_solution, 0);
                const long long elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        clock_type::now() - start).count();

                // Validation is deliberately outside the timed section.
                const std::string error = check_solution(data, result);
                if (!error.empty()) {
                    std::cerr << "Invalid solution: " << instance << ", "
                              << methods[i].name << ", run " << run + 1 << ": "
                              << error << '\n';
                    return 1;
                }

                stats[i].add(std::move(result), elapsed);
            }
        }

        for (std::size_t i = 0; i < methods.size(); ++i) {
            const method& m = methods[i];
            const run_statistics& s = stats[i];

            if (m.in_sweep) {
                sweep << instance << ';' << m.k << ';';
                s.write(sweep, number_of_runs);
            } else {
                m.algorithm->save_result_to_file(
                    s.best, output_dir + "/" + instance + "_" + m.name + ".txt", input_file);
                statistics << instance << ';' << m.name << ';';
                s.write(statistics, number_of_runs);
            }

            std::cout << instance << " " << m.name
                      << ": best = " << s.best.total_weight
                      << ", average = " << static_cast<double>(s.sum_weight) / number_of_runs
                      << ", average time = " << static_cast<double>(s.total_time) / number_of_runs
                      << " microseconds\n";
        }
    }

    std::cout << "All solutions passed the solution checker.\n";
    return 0;
}
