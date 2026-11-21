/**
 * @file convergence.cpp
 * @brief Lab 7, additional experiment: how the solution quality of ILS and LNS improves with the number of
 * iterations (and with time).
 *
 * The two best ILS versions (Lab 6) and the two best LNS versions are run NUMBER_OF_RUNS times per instance, with the
 * same time limits (average MSLS time from data/lab_6/results/statistics.csv), seeds and interleaving as the main
 * experiments. Every run records the initial local optimum and each improvement of the current solution, plus a final
 * point at the end of the run.
 *
 * Build and run from the repository root:
 *   g++ -std=c++17 -O2 -Wall -Wextra src/lab_7/convergence.cpp -o build/lab_7_convergence.exe
 *   ./build/lab_7_convergence.exe
 *
 * Result: data/lab_7/results/convergence.csv (instance;method;run;iteration;time(microsec);objective)
 *
 * With --iterations N, every run stops after exactly N iterations instead of the MSLS time
 * (result: data/lab_7/results/convergence_<N>.csv), e.g. ./build/lab_7_convergence.exe --iterations 10000
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../lab_6/algorithms/ils.hpp"
#include "algorithms/large_neigh_search.hpp"

#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"
#include "../utils/performance/high_performance.hpp"

constexpr unsigned SEED = 42;
constexpr int NUMBER_OF_RUNS = 20;
constexpr int PERTURBATION_STRENGTH = 4;  // as in Lab 6
const std::string MSLS_STATISTICS = "data/lab_6/results/statistics.csv";

std::chrono::microseconds msls_time(const std::string& instance) {
    std::ifstream in(MSLS_STATISTICS);
    if (!in) throw std::runtime_error("Could not open " + MSLS_STATISTICS + " (run lab 6 first).");
    std::string line;
    std::getline(in, line);
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

struct method_t {
    std::string name;
    bool is_ils;
    PerturbationType perturbation;  // ILS
    lns_config lns;                 // LNS
};

int main(int argc, char** argv) {
    std::cout << "Performance setup: " << request_high_performance() << '\n';
    // --iterations N: stop after N iterations (no time limit) instead of after the MSLS time
    int max_iterations = std::numeric_limits<int>::max();
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--iterations") max_iterations = std::stoi(argv[i + 1]);
    }
    const bool fixed_iterations = max_iterations != std::numeric_limits<int>::max();
    const std::string output = fixed_iterations
        ? "data/lab_7/results/convergence_" + std::to_string(max_iterations) + ".csv"
        : "data/lab_7/results/convergence.csv";
    const std::vector<std::string> instances = {"TSPA", "TSPB"};

    lns_config multi_ls;
    multi_ls.destroy.type = DestroyType::MULTI_SEGMENT;
    lns_config multi_no_ls = multi_ls;
    multi_no_ls.local_search = false;
    const std::vector<method_t> methods = {
        {"ILS_double_bridge", true, PerturbationType::DOUBLE_BRIDGE, {}},
        {"ILS_segment_relocation", true, PerturbationType::SEGMENT_RELOCATION, {}},
        {"LNS_multi_segment_LS", false, {}, multi_ls},
        {"LNS_multi_segment_noLS", false, {}, multi_no_ls},
    };

    std::vector<algorithm_data> datasets;
    std::vector<std::chrono::microseconds> time_limits;
    for (const std::string& instance : instances) {
        datasets.push_back(parse_csv("data/input/" + instance + ".csv"));
        time_limits.push_back(fixed_iterations ? std::chrono::microseconds(std::chrono::hours(1)) : msls_time(instance));
    }

    std::filesystem::create_directories("data/lab_7/results");
    std::ofstream out(output);
    if (!out) throw std::runtime_error("Could not open " + output);
    out << "instance;method;run;iteration;time(microsec);objective\n";

    // one local search object per (method, instance) with the same seed, as in the main experiments
    std::vector<std::vector<LocalSearchSteepestFast>> local_searches(
        methods.size(), std::vector<LocalSearchSteepestFast>(instances.size(), LocalSearchSteepestFast(SEED)));

    for (int run = 0; run < NUMBER_OF_RUNS; ++run) {
        std::cout << "Run " << run + 1 << '/' << NUMBER_OF_RUNS << "...\n";
        const unsigned seed = SEED + static_cast<unsigned>(run);
        for (std::size_t k = 0; k < methods.size(); ++k) {
            for (std::size_t d = 0; d < instances.size(); ++d) {
                std::vector<progress_point> progress;
                algorithm_result best;
                int iterations = 0;
                long long elapsed = 0;
                if (methods[k].is_ils) {
                    ILS ils(local_searches[k][d]);
                    const ils_result r = ils.run(datasets[d], time_limits[d], methods[k].perturbation,
                                                 PERTURBATION_STRENGTH, seed, &progress, max_iterations);
                    best = r.best;
                    iterations = r.local_search_runs - 1;
                    elapsed = r.elapsed_microseconds;
                } else {
                    LNS lns(local_searches[k][d]);
                    const lns_result r =
                        lns.run(datasets[d], time_limits[d], methods[k].lns, seed, &progress, max_iterations);
                    best = r.best;
                    iterations = r.iterations;
                    elapsed = r.elapsed_microseconds;
                }
                const std::string error = check_solution(datasets[d], best);
                if (!error.empty()) throw std::runtime_error("Invalid solution: " + error);
                progress.push_back({iterations, elapsed, best.total_weight});  // end of the run
                for (const progress_point& p : progress) {
                    out << instances[d] << ';' << methods[k].name << ';' << run + 1 << ';' << p.iteration << ';'
                        << p.microseconds << ';' << p.objective << '\n';
                }
            }
        }
    }
    std::cout << "Done: " << output << '\n';
    return 0;
}
