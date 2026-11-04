/** @file main.cpp
 *  @brief Compare the steepest local search of lab 3 (baseline) with its version using move evaluations from
 *         previous iterations (list of improving moves, LM), candidate moves (lab 4 and an optimized version)
 *         and LM combined with candidate moves. All start from random solutions.
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../lab_4/algorithms/local_search_steepest.hpp"
#include "algorithms/candidate_lists.hpp"
#include "algorithms/local_search_candidates.hpp"
#include "algorithms/local_steepest_fast.hpp"
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

    double average_time(int runs) const { return static_cast<double>(total_time) / runs; }

    void write(std::ostream& out, int runs) const {
        out << best.total_weight << ';' << max_weight << ';'
            << static_cast<double>(sum_weight) / runs << ';'
            << average_time(runs) << ';' << min_time << ';' << max_time << '\n';
    }
};

struct method {
    std::string name;
    // One object per method, with the same seed, kept across instances (as in labs 3 and 4):
    // every method gets the same sequence of random starting solutions.
    std::unique_ptr<AlgorithmBase> algorithm;
    std::function<algorithm_result(const algorithm_data&)> run;
};

int main() {
    constexpr unsigned seed = 42;
    constexpr int number_of_runs = 200;
    constexpr int candidate_list_size = 10;  // the K used in lab 4

    const std::vector<std::string> instances = {"TSPA", "TSPB"};
    const std::string output_dir = "data/lab_5/output";
    const std::string results_dir = "data/lab_5/results";
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    // candidate lists of the current instance, used by the candidate methods
    std::unique_ptr<CandidateMoveGenerator> generator;
    std::unique_ptr<CandidateLists> candidate_lists;

    auto baseline = std::make_unique<LocalSearchSteepest>(seed);
    auto lab4_candidates = std::make_unique<LocalSearchSteepest>(seed);
    auto lm = std::make_unique<LocalSearchSteepestFast>(seed);
    auto candidates = std::make_unique<LocalSearchCandidates>(seed);
    auto lm_candidates = std::make_unique<LocalSearchSteepestFast>(seed);

    std::vector<method> methods;
    // lab 3 steepest search (lab 4's LocalSearchSteepest::run is the same code)
    methods.push_back({"steepest_baseline", nullptr, [p = baseline.get()](const algorithm_data& data) {
        return p->run(data, "intra_edge_exchange", "inter_node_exchange", "random", 0);
    }});
    methods.push_back({"steepest_lm", nullptr, [p = lm.get()](const algorithm_data& data) {
        return p->run(data, "random", 0);
    }});
    methods.push_back({"candidates_lab4", nullptr, [p = lab4_candidates.get(), &generator](const algorithm_data& data) {
        return p->run_with_candidates(data, "intra_edge_exchange", "inter_node_exchange", "random", 0, *generator);
    }});
    methods.push_back({"candidates", nullptr, [p = candidates.get()](const algorithm_data& data) {
        return p->run(data, "random", 0);
    }});
    methods.push_back({"lm_candidates", nullptr, [p = lm_candidates.get()](const algorithm_data& data) {
        return p->run(data, "random", 0);
    }});
    methods[0].algorithm = std::move(baseline);
    methods[1].algorithm = std::move(lm);
    methods[2].algorithm = std::move(lab4_candidates);
    methods[3].algorithm = std::move(candidates);
    methods[4].algorithm = std::move(lm_candidates);

    const std::string statistics_file = results_dir + "/statistics.csv";
    std::ofstream statistics(statistics_file);
    if (!statistics) {
        throw std::runtime_error("Could not open file: " + statistics_file);
    }
    statistics << "instance;method;min;max;average;"
               << "avg_time(microsec);min_time(microsec);max_time(microsec)\n";
    statistics << std::fixed << std::setprecision(2);
    std::cout << std::fixed << std::setprecision(2);

    for (const std::string& instance : instances) {
        const std::string input_file = "data/input/" + instance + ".csv";
        const algorithm_data data = parse_csv(input_file);

        // Candidate lists are built once per instance (not timed per run).
        generator = std::make_unique<CandidateMoveGenerator>(data, candidate_list_size);
        candidate_lists = std::make_unique<CandidateLists>(*generator, data.nodes_num);
        static_cast<LocalSearchCandidates&>(*methods[3].algorithm).set_candidates(candidate_lists.get());
        static_cast<LocalSearchSteepestFast&>(*methods[4].algorithm).set_candidates(candidate_lists.get());

        // Methods are interleaved run by run, so fluctuations of the machine
        // load affect all of them equally instead of distorting single methods.
        std::vector<run_statistics> stats(methods.size());
        for (int run = 0; run < number_of_runs; ++run) {
            for (std::size_t i = 0; i < methods.size(); ++i) {
                const auto start = clock_type::now();
                algorithm_result result = methods[i].run(data);
                const long long elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(clock_type::now() - start).count();

                // Validation is deliberately outside the timed section.
                const std::string error = check_solution(data, result);
                if (!error.empty()) {
                    std::cerr << "Invalid solution: " << instance << ", " << methods[i].name
                              << ", run " << run + 1 << ": " << error << '\n';
                    return 1;
                }
                stats[i].add(std::move(result), elapsed);
            }
        }

        const double baseline_time = stats[0].average_time(number_of_runs);
        for (std::size_t i = 0; i < methods.size(); ++i) {
            const run_statistics& s = stats[i];
            // lab 4's candidate search takes the same moves as "candidates" (only its time is of interest),
            // so its best solution would only duplicate that one in the outputs and plots
            if (methods[i].name != "candidates_lab4") {
                methods[i].algorithm->save_result_to_file(
                    s.best, output_dir + "/" + instance + "_" + methods[i].name + ".txt", input_file);
            }
            statistics << instance << ';' << methods[i].name << ';';
            s.write(statistics, number_of_runs);

            std::cout << instance << " " << methods[i].name
                      << ": best = " << s.best.total_weight
                      << ", average = " << static_cast<double>(s.sum_weight) / number_of_runs
                      << ", average time = " << s.average_time(number_of_runs) << " microseconds"
                      << " (x" << baseline_time / s.average_time(number_of_runs) << " vs baseline)\n";
        }
    }

    std::cout << "All solutions passed the solution checker.\n";
    return 0;
}
