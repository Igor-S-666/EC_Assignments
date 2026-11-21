/**
 * @file ils.hpp
 * @brief Iterated Local Search.
 */

#pragma once

#include <chrono>
#include <limits>
#include <random>
#include <utility>

#include "local_steepest_fast.hpp"
#include "perturbations.hpp"


struct ils_result {
    algorithm_result best;
    int local_search_runs = 0;
    long long elapsed_microseconds = 0;
};


class ILS {
public:

    /**
     * @param reuse_memory if true, the local search after a perturbation starts from the final LM of the current
     *  solution and evaluates only the moves the perturbation could have made improving (same local optimum as a
     *  full search, much faster). If false, every local search evaluates all moves of the perturbed solution.
     */
    explicit ILS(LocalSearchSteepestFast& local_search, bool reuse_memory = true)
        : local_search(local_search), reuse_memory(reuse_memory) {}

    /**
     * @brief Runs Iterated Local Search.
     *
     * @param data Problem instance.
     * @param time_limit Maximum running time.
     * @param perturbation Selected perturbation.
     * @param strength Perturbation strength.
     * @param seed Random seed for perturbations.
     * @param progress if given, gets a point for the initial solution and for every improvement
     * @param max_iterations additional stopping condition: number of perturbation + LS iterations (default: none)
     */
    ils_result run(
        const algorithm_data& data,
        std::chrono::microseconds time_limit,
        PerturbationType perturbation,
        int strength,
        unsigned seed,
        std::vector<progress_point>* progress = nullptr,
        int max_iterations = std::numeric_limits<int>::max()
    ) {
        using clock_type = std::chrono::steady_clock;

        std::mt19937 rng(seed);

        const auto start = clock_type::now();
        auto record = [&](int iteration, int objective) {
            if (progress) {
                const auto us = std::chrono::duration_cast<std::chrono::microseconds>(clock_type::now() - start);
                progress->push_back({iteration, us.count(), objective});
            }
        };

        // final LMs of the current solution and of the last candidate
        LocalSearchSteepestFast::Memory current_memory, candidate_memory;

        algorithm_result current = local_search.run_random(data, &current_memory);
        record(0, current.total_weight);

        algorithm_result best = current;

        int local_search_runs = 1;

        while (clock_type::now() - start < time_limit && local_search_runs - 1 < max_iterations) {

            // Perturb current local optimum.
            algorithm_result candidate = Perturbations::apply(
                data,
                current,
                perturbation,
                strength,
                rng
            );

            // Run local search from the perturbed solution.
            candidate = reuse_memory
                ? local_search.run_from_perturbation(data, candidate, current, current_memory, &candidate_memory)
                : local_search.run_from_solution(data, candidate);

            ++local_search_runs;

            // Remember the best solution found.
            if (candidate.total_weight < best.total_weight) {
                best = candidate;
            }

            // Greedy acceptance criterion.
            // The problem is a minimization problem.
            if (candidate.total_weight < current.total_weight) {
                current = std::move(candidate);
                std::swap(current_memory, candidate_memory);
                record(local_search_runs - 1, current.total_weight);  // iterations = LS runs after the initial one
            }
        }

        const long long elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(
                clock_type::now() - start
            ).count();

        return {
            std::move(best),
            local_search_runs,
            elapsed
        };
    }

private:
    LocalSearchSteepestFast& local_search;
    bool reuse_memory;
};
