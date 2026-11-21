/**
 * @file main.cpp
 * @brief Lab 6: Multiple Start Local Search (MSLS) and Iterated Local Search (ILS) with several perturbations.
 *
 * MSLS runs the steepest local search (LM version from lab 5, full neighbourhood) from MSLS_ITERATIONS random
 * solutions. ILS gets the average running time of MSLS on the instance as its time limit. The instances are
 * interleaved run by run, so slowdowns of the machine under continuous load affect all of them equally.
 *
 * Results:
 *  data/lab_6/output/<instance>_<method>.txt   best solution of each method
 *  data/lab_6/results/statistics.csv           objective and time statistics
 *  data/lab_6/results/ils_iterations.csv       number of local search runs of ILS
 *  data/lab_6/results/runs.csv                 every single run (objective, time, local search runs)
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "algorithms/ils.hpp"
#include "algorithms/local_steepest_fast.hpp"
#include "algorithms/msls.hpp"
#include "algorithms/perturbations.hpp"

#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"
#include "../utils/performance/high_performance.hpp"

using clock_type = std::chrono::steady_clock;

constexpr unsigned SEED = 42;
constexpr int NUMBER_OF_RUNS = 20;
constexpr int MSLS_ITERATIONS = 400;
constexpr int PERTURBATION_STRENGTH = 4;

struct run_statistics {
    int runs = 0;

    long long total_time = 0;
    long long min_time = std::numeric_limits<long long>::max();
    long long max_time = 0;

    long long sum_weight = 0;
    int max_weight = std::numeric_limits<int>::min();

    long long total_ls_runs = 0;
    int min_ls_runs = std::numeric_limits<int>::max();
    int max_ls_runs = 0;

    algorithm_result best{{}, std::numeric_limits<int>::max()};

    void add(const algorithm_result& result, long long elapsed, int ls_runs) {
        ++runs;
        total_time += elapsed;
        min_time = std::min(min_time, elapsed);
        max_time = std::max(max_time, elapsed);

        sum_weight += result.total_weight;
        max_weight = std::max(max_weight, result.total_weight);

        total_ls_runs += ls_runs;
        min_ls_runs = std::min(min_ls_runs, ls_runs);
        max_ls_runs = std::max(max_ls_runs, ls_runs);

        if (result.total_weight < best.total_weight) best = result;
    }

    double average_time() const { return static_cast<double>(total_time) / runs; }
    double average_weight() const { return static_cast<double>(sum_weight) / runs; }
    double average_ls_runs() const { return static_cast<double>(total_ls_runs) / runs; }
};

void write_statistics(std::ostream& out, const std::string& instance, const std::string& method,
                      const run_statistics& stats) {
    out << instance << ';' << method << ';'
        << stats.best.total_weight << ';' << stats.max_weight << ';' << stats.average_weight() << ';'
        << stats.average_time() << ';' << stats.min_time << ';' << stats.max_time << '\n';
}

void print_statistics(const std::string& instance, const std::string& method, const run_statistics& stats) {
    std::cout << instance << ' ' << method
              << ": best = " << stats.best.total_weight
              << ", average = " << stats.average_weight()
              << ", average time = " << stats.average_time() << " us"
              << ", average LS runs = " << stats.average_ls_runs() << '\n';
}

// Fails the whole experiment on an invalid solution (checked outside the timed sections).
void validate(const algorithm_data& data, const algorithm_result& result, const std::string& what) {
    const std::string error = check_solution(data, result);
    if (!error.empty()) throw std::runtime_error("Invalid solution (" + what + "): " + error);
}

int main() {
    // without it Windows may throttle the process (EcoQoS) at random moments, see high_performance.hpp
    std::cout << "Performance setup: " << request_high_performance() << '\n';
    const std::vector<std::string> instances = {"TSPA", "TSPB"};
    const std::vector<PerturbationType> perturbations = {
        PerturbationType::DOUBLE_BRIDGE,
        PerturbationType::NODE_RELOCATION,
        PerturbationType::SEGMENT_SHUFFLE,
        PerturbationType::NODE_EXCHANGE,
        PerturbationType::SEGMENT_RELOCATION,
        PerturbationType::SEGMENT_EXCHANGE,
        PerturbationType::LARGE_2OPT,
        PerturbationType::RUIN_RECREATE,
    };

    const std::string output_dir = "data/lab_6/output";
    const std::string results_dir = "data/lab_6/results";
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    std::ofstream statistics(results_dir + "/statistics.csv");
    std::ofstream iterations(results_dir + "/ils_iterations.csv");
    std::ofstream runs(results_dir + "/runs.csv");
    if (!statistics || !iterations || !runs) throw std::runtime_error("Could not open the result files.");

    statistics << "instance;method;best;max;average;avg_time(microsec);min_time(microsec);max_time(microsec)\n";
    iterations << "instance;perturbation;strength;average_LS_runs;min_LS_runs;max_LS_runs\n";
    runs << "instance;method;run;objective;time(microsec);LS_runs\n";
    for (std::ostream* out : {static_cast<std::ostream*>(&statistics), static_cast<std::ostream*>(&iterations),
                              static_cast<std::ostream*>(&std::cout)}) {
        *out << std::fixed << std::setprecision(2);
    }

    const std::size_t instance_count = instances.size();
    std::vector<algorithm_data> datasets;
    std::vector<std::string> input_files;
    for (const std::string& instance : instances) {
        input_files.push_back("data/input/" + instance + ".csv");
        datasets.push_back(parse_csv(input_files.back()));
    }

    // Instances are interleaved run by run (as the methods in lab 5): after a few seconds of continuous load the
    // machine runs noticeably slower, and running all instances side by side makes this affect all of them equally,
    // instead of only the one measured later.

    // ---- MSLS ----
    std::vector<LocalSearchSteepestFast> msls_ls(instance_count, LocalSearchSteepestFast(SEED));
    std::vector<run_statistics> msls_stats(instance_count);
    std::cout << "Running MSLS...\n";
    for (int run = 0; run < NUMBER_OF_RUNS; ++run) {
        for (std::size_t d = 0; d < instance_count; ++d) {
            MSLS msls(msls_ls[d]);
            const auto start = clock_type::now();
            const algorithm_result result = msls.run(datasets[d], MSLS_ITERATIONS);
            const long long elapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(clock_type::now() - start).count();
            validate(datasets[d], result, instances[d] + " MSLS");
            msls_stats[d].add(result, elapsed, MSLS_ITERATIONS);
            runs << instances[d] << ";MSLS;" << run + 1 << ';' << result.total_weight << ';' << elapsed << ';'
                 << MSLS_ITERATIONS << '\n';
        }
    }
    // ILS time limit = average MSLS time of the instance
    std::vector<std::chrono::microseconds> time_limits;
    for (std::size_t d = 0; d < instance_count; ++d) {
        print_statistics(instances[d], "MSLS", msls_stats[d]);
        time_limits.emplace_back(static_cast<long long>(msls_stats[d].average_time()));
    }

    // ---- ILS ----
    // ils_stats[d][t]: instance d, perturbation t
    std::vector<std::vector<run_statistics>> ils_stats(instance_count,
                                                       std::vector<run_statistics>(perturbations.size()));
    for (std::size_t t = 0; t < perturbations.size(); ++t) {
        const PerturbationType type = perturbations[t];
        const std::string method = "ILS_" + perturbation_name(type) + "_strength_" +
                                   std::to_string(PERTURBATION_STRENGTH);
        std::cout << "Running " << method << "...\n";
        // one local search object per instance, with the same seed: the same random starting solutions
        std::vector<LocalSearchSteepestFast> ils_ls(instance_count, LocalSearchSteepestFast(SEED));
        for (int run = 0; run < NUMBER_OF_RUNS; ++run) {
            const unsigned ils_seed = SEED + static_cast<unsigned>(run);
            for (std::size_t d = 0; d < instance_count; ++d) {
                ILS ils(ils_ls[d]);
                const ils_result result =
                    ils.run(datasets[d], time_limits[d], type, PERTURBATION_STRENGTH, ils_seed);
                validate(datasets[d], result.best, instances[d] + " " + method);
                ils_stats[d][t].add(result.best, result.elapsed_microseconds, result.local_search_runs);
                runs << instances[d] << ';' << method << ';' << run + 1 << ';' << result.best.total_weight << ';'
                     << result.elapsed_microseconds << ';' << result.local_search_runs << '\n';
            }
        }
    }

    // ---- results, grouped by instance ----
    for (std::size_t d = 0; d < instance_count; ++d) {
        const std::string& instance = instances[d];
        msls_ls[d].save_result_to_file(msls_stats[d].best, output_dir + "/" + instance + "_MSLS.txt",
                                       input_files[d]);
        write_statistics(statistics, instance, "MSLS", msls_stats[d]);

        for (std::size_t t = 0; t < perturbations.size(); ++t) {
            const std::string name = perturbation_name(perturbations[t]);
            const std::string method = "ILS_" + name + "_strength_" + std::to_string(PERTURBATION_STRENGTH);
            const run_statistics& stats = ils_stats[d][t];
            msls_ls[d].save_result_to_file(stats.best, output_dir + "/" + instance + "_" + method + ".txt",
                                           input_files[d]);
            write_statistics(statistics, instance, method, stats);
            print_statistics(instance, method, stats);
            iterations << instance << ';' << name << ';' << PERTURBATION_STRENGTH << ';'
                       << stats.average_ls_runs() << ';' << stats.min_ls_runs << ';' << stats.max_ls_runs << '\n';
        }
    }

    std::cout << "All experiments completed. All solutions passed the solution checker.\n";
    return 0;
}
