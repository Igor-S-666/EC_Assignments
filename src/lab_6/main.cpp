
/**
 * @file main.cpp
 * @brief Lab 6: MSLS and ILS computational experiments with automatic result saving.
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "algorithms/msls.hpp"
#include "algorithms/ils.hpp"
#include "algorithms/perturbations.hpp"
#include "algorithms/local_steepest_fast.hpp"

#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"


using clock_type = std::chrono::steady_clock;


constexpr unsigned SEED = 42;

constexpr int NUMBER_OF_RUNS = 20;
constexpr int MSLS_ITERATIONS = 400;

constexpr int PERTURBATION_STRENGTH = 4;

const std::vector<std::string> INSTANCES = {
    "TSPA",
    "TSPB"
};


struct run_statistics {

    long long total_time = 0;

    long long min_time =
        std::numeric_limits<long long>::max();

    long long max_time = 0;

    long long sum_weight = 0;

    int max_weight =
        std::numeric_limits<int>::min();

    long long total_ls_runs = 0;

    int min_ls_runs =
        std::numeric_limits<int>::max();

    int max_ls_runs = 0;

    algorithm_result best{
        {},
        std::numeric_limits<int>::max()
    };

    void add(
        const algorithm_result& result,
        long long elapsed,
        int ls_runs
    ) {
        total_time += elapsed;

        min_time = std::min(min_time, elapsed);
        max_time = std::max(max_time, elapsed);

        sum_weight += result.total_weight;

        max_weight = std::max(
            max_weight,
            result.total_weight
        );

        total_ls_runs += ls_runs;

        min_ls_runs = std::min(
            min_ls_runs,
            ls_runs
        );

        max_ls_runs = std::max(
            max_ls_runs,
            ls_runs
        );

        if (result.total_weight < best.total_weight) {
            best = result;
        }
    }

    double average_time() const {
        return static_cast<double>(total_time) / NUMBER_OF_RUNS;
    }

    double average_weight() const {
        return static_cast<double>(sum_weight) / NUMBER_OF_RUNS;
    }

    double average_ls_runs() const {
        return static_cast<double>(total_ls_runs) / NUMBER_OF_RUNS;
    }
};

void write_statistics(
    std::ostream& out,
    const std::string& instance,
    const std::string& method,
    const run_statistics& stats
) {
    out << instance << ';'
        << method << ';'
        << stats.best.total_weight << ';'
        << stats.max_weight << ';'
        << stats.average_weight() << ';'
        << stats.average_time() << ';'
        << stats.min_time << ';'
        << stats.max_time << '\n';
}


void print_statistics(
    const std::string& instance,
    const std::string& method,
    const run_statistics& stats,
    double baseline_time
) {
    std::cout
        << instance << " "
        << method
        << ": best = " << stats.best.total_weight
        << ", average = " << stats.average_weight()
        << ", average time = " << stats.average_time()
        << " microseconds"
        << ", average LS runs = " << stats.average_ls_runs()
        << ", speed = x"
        << baseline_time / stats.average_time()
        << '\n';
}



int main() {

    const std::string output_dir = "data/lab_6/output";
    const std::string results_dir = "data/lab_6/results";

    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(results_dir);

    const std::string statistics_file =
        results_dir + "/statistics_msls_ils.csv";

    const std::string iterations_file =
        results_dir + "/ils_iterations.csv";

    std::ofstream statistics(statistics_file);
    std::ofstream iterations(iterations_file);

    if (!statistics || !iterations) {
        throw std::runtime_error(
            "Could not open output files."
        );
    }

    statistics
        << "instance;method;best;max;average;"
        << "avg_time(microsec);min_time(microsec);"
        << "max_time(microsec)\n";

    iterations
        << "instance;perturbation;strength;"
        << "average_LS_runs;min_LS_runs;max_LS_runs\n";

    statistics << std::fixed << std::setprecision(2);
    iterations << std::fixed << std::setprecision(2);
    std::cout << std::fixed << std::setprecision(2);


    // Perturbations to compare.
    const std::vector<PerturbationType> perturbations = {
    PerturbationType::DOUBLE_BRIDGE,
    PerturbationType::NODE_RELOCATION,
    PerturbationType::SEGMENT_SHUFFLE,
    PerturbationType::NODE_EXCHANGE,

    // New stronger perturbations
    PerturbationType::SEGMENT_RELOCATION,
    PerturbationType::SEGMENT_EXCHANGE,
    PerturbationType::LARGE_2OPT,
    PerturbationType::RUIN_RECREATE
    };

    for (const std::string& instance : INSTANCES) {

        std::cout
            << "Instance: " << instance << '\n';

        const std::string input_file =
            "data/input/" + instance + ".csv";

        const algorithm_data data = parse_csv(input_file);


        std::cout << "\nRunning MSLS...\n";

        LocalSearchSteepestFast msls_ls(SEED);

        MSLS msls(msls_ls);

        run_statistics msls_stats;

        for (int run = 0; run < NUMBER_OF_RUNS; ++run) {

            const auto start = clock_type::now();

            algorithm_result result =
                msls.run(data, MSLS_ITERATIONS);

            const long long elapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    clock_type::now() - start
                ).count();

            const std::string error =
                check_solution(data, result);

            if (!error.empty()) {
                std::cerr
                    << "Invalid MSLS solution: "
                    << instance << ", run " << run + 1
                    << ": " << error << '\n';

                return 1;
            }

            msls_stats.add(
                result,
                elapsed,
                MSLS_ITERATIONS
            );

            std::cout
                << "MSLS run " << run + 1
                << ": " << result.total_weight
                << ", time = " << elapsed << " us\n";
        }


        // The average runtime of one complete MSLS run.
        const double msls_time = msls_stats.average_time();

        msls_ls.save_result_to_file(
            msls_stats.best,
            output_dir + "/" + instance + "_MSLS.txt",
            input_file
        );

        write_statistics(
            statistics,
            instance,
            "MSLS",
            msls_stats
        );

        print_statistics(
            instance,
            "MSLS",
            msls_stats,
            msls_time
        );


        std::cout << "\nRunning ILS...\n";

        const auto time_limit =
            std::chrono::microseconds(
                static_cast<long long>(msls_time)
            );


        for (const PerturbationType type : perturbations) {

            const std::string name = perturbation_name(type);

            std::cout
                << "\nPerturbation: " << name
                << ", strength = " << PERTURBATION_STRENGTH
                << '\n';

            LocalSearchSteepestFast ils_ls(SEED);

            ILS ils(ils_ls);

            run_statistics ils_stats;


            for (int run = 0; run < NUMBER_OF_RUNS; ++run) {

                const unsigned ils_seed =
                    SEED + static_cast<unsigned>(run);

                ils_result result = ils.run(
                    data,
                    time_limit,
                    type,
                    PERTURBATION_STRENGTH,
                    ils_seed
                );


                // Validation is outside the timed section.
                const std::string error =
                    check_solution(data, result.best);

                if (!error.empty()) {
                    std::cerr
                        << "Invalid ILS solution: "
                        << instance << ", "
                        << name << ", run " << run + 1
                        << ": " << error << '\n';

                    return 1;
                }


                ils_stats.add(
                    result.best,
                    result.elapsed_microseconds,
                    result.local_search_runs
                );


                std::cout
                    << "ILS run " << run + 1
                    << ": best = " << result.best.total_weight
                    << ", LS runs = " << result.local_search_runs
                    << ", time = "
                    << result.elapsed_microseconds << " us\n";
            }


            // Save best solution.
            const std::string method_name =
                "ILS_" + name +
                "_strength_" +
                std::to_string(PERTURBATION_STRENGTH);

            ils_ls.save_result_to_file(
                ils_stats.best,
                output_dir + "/" + instance + "_" +
                    method_name + ".txt",
                input_file
            );


            // Save statistics.
            write_statistics(
                statistics,
                instance,
                method_name,
                ils_stats
            );


            print_statistics(
                instance,
                method_name,
                ils_stats,
                msls_time
            );


            // Save the number of basic local searches.
            iterations
                << instance << ';'
                << name << ';'
                << PERTURBATION_STRENGTH << ';'
                << ils_stats.average_ls_runs() << ';'
                << ils_stats.min_ls_runs << ';'
                << ils_stats.max_ls_runs << '\n';
        }
    }


    statistics.close();
    iterations.close();

    std::cout
        << "All experiments completed.\n"
        << "All solutions passed the checker.\n";

    return 0;
}
