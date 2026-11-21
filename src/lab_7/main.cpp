/**
 * @file main.cpp
 * @brief Lab 7: Large Neighbourhood Search (LNS).
 *
 * Every destroy operator (random nodes, one subpath, 3-5 subpaths), in a uniform and a cost-weighted version, is
 * run with and without local search after destroy-repair, each with the deterministic and the randomised repair
 * (REPAIR_RCLS): 24 versions, NUMBER_OF_RUNS runs per instance each.
 * The time limit of every run is the average MSLS time of the instance from lab 6
 * (data/lab_6/results/statistics.csv).
 *
 * This machine slows down a lot (up to ~10x) after a few seconds of continuous load. So the runs are interleaved:
 * run r of every method on every instance is done before run r + 1 of any of them. That way the slowdown affects
 * all methods equally, rather than only the ones measured later.
 *
 * Results:
 *  data/lab_7/output/<instance>_<method>.txt   best solution of each method
 *  data/lab_7/results/statistics.csv           objective and time statistics
 *  data/lab_7/results/lns_iterations.csv       number of iterations of the main loop
 *  data/lab_7/results/runs.csv                 every single run (objective, time, iterations)
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "algorithms/large_neigh_search.hpp"

#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"
#include "../utils/performance/high_performance.hpp"

constexpr unsigned SEED = 42;
constexpr int NUMBER_OF_RUNS = 20;
constexpr double DESTROY_FRACTION = 0.3;
// exponents of the weighted destroy: probability ~ cost^exponent. Subpath costs differ much less than single node
// weights (a sum over ~30 nodes), so the segment versions need a larger exponent to be noticeably non-uniform.
constexpr double NODE_EXPONENT = 1.0;
constexpr double SEGMENT_EXPONENT = 3.0;
// repair versions: 1 = deterministic, k > 1 = randomised (random node among the k best, method suffix "_rclk")
const std::vector<int> REPAIR_RCLS = {1, 3};
const std::string MSLS_STATISTICS = "data/lab_6/results/statistics.csv";

struct run_statistics {
    int runs = 0;

    long long total_time = 0;
    long long min_time = std::numeric_limits<long long>::max();
    long long max_time = 0;

    long long sum_weight = 0;
    int max_weight = std::numeric_limits<int>::min();

    long long total_iterations = 0;
    int min_iterations = std::numeric_limits<int>::max();
    int max_iterations = 0;

    algorithm_result best{{}, std::numeric_limits<int>::max()};

    void add(const lns_result& result) {
        ++runs;
        total_time += result.elapsed_microseconds;
        min_time = std::min(min_time, result.elapsed_microseconds);
        max_time = std::max(max_time, result.elapsed_microseconds);

        sum_weight += result.best.total_weight;
        max_weight = std::max(max_weight, result.best.total_weight);

        total_iterations += result.iterations;
        min_iterations = std::min(min_iterations, result.iterations);
        max_iterations = std::max(max_iterations, result.iterations);

        if (result.best.total_weight < best.total_weight) best = result.best;
    }

    double average_time() const { return static_cast<double>(total_time) / runs; }
    double average_weight() const { return static_cast<double>(sum_weight) / runs; }
    double average_iterations() const { return static_cast<double>(total_iterations) / runs; }
};

struct method_t {
    std::string name;
    lns_config config;
};

// LNS_<destroy>_LS / LNS_<destroy>_noLS [+ _rclk] for every destroy operator (uniform and weighted) and repair
std::vector<method_t> make_methods() {
    std::vector<method_t> methods;
    for (const int rcl : REPAIR_RCLS) {
        for (const DestroyType type : {DestroyType::RANDOM_NODES, DestroyType::SINGLE_SEGMENT,
                                       DestroyType::MULTI_SEGMENT}) {
            for (const bool weighted : {false, true}) {
                for (const bool local_search : {true, false}) {
                    lns_config cfg;
                    cfg.destroy.type = type;
                    cfg.destroy.weighted = weighted;
                    cfg.destroy.fraction = DESTROY_FRACTION;
                    cfg.destroy.exponent = type == DestroyType::RANDOM_NODES ? NODE_EXPONENT : SEGMENT_EXPONENT;
                    cfg.repair_rcl = rcl;
                    cfg.local_search = local_search;
                    std::string name = "LNS_" + destroy_name(cfg.destroy) + (local_search ? "_LS" : "_noLS");
                    if (rcl > 1) name += "_rcl" + std::to_string(rcl);
                    methods.push_back({name, cfg});
                }
            }
        }
    }
    return methods;
}

// average MSLS time of `instance` (column avg_time of the lab 6 statistics)
std::chrono::microseconds msls_time(const std::string& instance) {
    std::ifstream in(MSLS_STATISTICS);
    if (!in) throw std::runtime_error("Could not open " + MSLS_STATISTICS + " (run lab 6 first).");
    std::string line;
    std::getline(in, line);  // header: instance;method;best;max;average;avg_time(microsec);...
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::vector<std::string> cols;
        for (std::string col; std::getline(ss, col, ';');) cols.push_back(col);
        if (cols.size() > 5 && cols[0] == instance && cols[1] == "MSLS") {
            return std::chrono::microseconds(static_cast<long long>(std::stod(cols[5])));
        }
    }
    throw std::runtime_error("No MSLS time for " + instance + " in " + MSLS_STATISTICS);
}

// Fails the whole experiment on an invalid solution (checked outside the timed sections).
void validate(const algorithm_data& data, const algorithm_result& result, const std::string& what) {
    const std::string error = check_solution(data, result);
    if (!error.empty()) throw std::runtime_error("Invalid solution (" + what + "): " + error);
}

int main() {
    std::cout << "Performance setup: " << request_high_performance() << '\n';
    const std::vector<std::string> instances = {"TSPA", "TSPB"};
    const std::vector<method_t> methods = make_methods();

    const std::string output_dir = "data/lab_7/output";
    const std::string results_dir = "data/lab_7/results";
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    std::ofstream statistics(results_dir + "/statistics.csv");
    std::ofstream iterations(results_dir + "/lns_iterations.csv");
    std::ofstream runs(results_dir + "/runs.csv");
    if (!statistics || !iterations || !runs) throw std::runtime_error("Could not open the result files.");

    statistics << "instance;method;best;max;average;avg_time(microsec);min_time(microsec);max_time(microsec)\n";
    iterations << "instance;method;average_iterations;min_iterations;max_iterations\n";
    runs << "instance;method;run;objective;time(microsec);iterations\n";
    for (std::ostream* out : {static_cast<std::ostream*>(&statistics), static_cast<std::ostream*>(&iterations),
                              static_cast<std::ostream*>(&std::cout)}) {
        *out << std::fixed << std::setprecision(2);
    }

    const std::size_t instance_count = instances.size();
    std::vector<algorithm_data> datasets;
    std::vector<std::string> input_files;
    std::vector<std::chrono::microseconds> time_limits;
    for (const std::string& instance : instances) {
        input_files.push_back("data/input/" + instance + ".csv");
        datasets.push_back(parse_csv(input_files.back()));
        time_limits.push_back(msls_time(instance));
        std::cout << instance << ": time limit = " << time_limits.back().count() << " us (average MSLS time)\n";
    }

    // one local search object per (method, instance), all with the same seed: every method gets the same
    // sequence of random starting solutions
    std::vector<std::vector<LocalSearchSteepestFast>> local_searches(
        methods.size(), std::vector<LocalSearchSteepestFast>(instance_count, LocalSearchSteepestFast(SEED)));
    std::vector<std::vector<run_statistics>> stats(methods.size(), std::vector<run_statistics>(instance_count));

    for (int run = 0; run < NUMBER_OF_RUNS; ++run) {
        std::cout << "Run " << run + 1 << '/' << NUMBER_OF_RUNS << "...\n";
        const unsigned destroy_seed = SEED + static_cast<unsigned>(run);
        for (std::size_t k = 0; k < methods.size(); ++k) {
            for (std::size_t d = 0; d < instance_count; ++d) {
                LNS lns(local_searches[k][d]);
                const lns_result result = lns.run(datasets[d], time_limits[d], methods[k].config, destroy_seed);
                validate(datasets[d], result.best, instances[d] + " " + methods[k].name);
                stats[k][d].add(result);
                runs << instances[d] << ';' << methods[k].name << ';' << run + 1 << ';' << result.best.total_weight
                     << ';' << result.elapsed_microseconds << ';' << result.iterations << '\n';
            }
        }
    }

    // ---- results, grouped by instance ----
    for (std::size_t d = 0; d < instance_count; ++d) {
        const std::string& instance = instances[d];
        for (std::size_t k = 0; k < methods.size(); ++k) {
            const std::string& method = methods[k].name;
            const run_statistics& s = stats[k][d];
            local_searches[k][d].save_result_to_file(s.best, output_dir + "/" + instance + "_" + method + ".txt",
                                                     input_files[d]);
            statistics << instance << ';' << method << ';' << s.best.total_weight << ';' << s.max_weight << ';'
                       << s.average_weight() << ';' << s.average_time() << ';' << s.min_time << ';' << s.max_time
                       << '\n';
            iterations << instance << ';' << method << ';' << s.average_iterations() << ';' << s.min_iterations
                       << ';' << s.max_iterations << '\n';
            std::cout << instance << ' ' << method << ": best = " << s.best.total_weight
                      << ", average = " << s.average_weight() << ", max = " << s.max_weight
                      << ", average iterations = " << s.average_iterations() << '\n';
        }
    }

    std::cout << "All experiments completed. All solutions passed the solution checker.\n";
    return 0;
}
