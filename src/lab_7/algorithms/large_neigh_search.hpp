/**
 * @file large_neigh_search.hpp
 * @brief Large Neighbourhood Search (LNS).
 *
 *   x := LocalSearch(random solution)
 *   repeat until the time limit:
 *       y := Destroy(x)          (destroy_methods.hpp, removes 30% of the nodes)
 *       y := Repair(y)           (repair_methods.hpp, weighted 2-regret greedy cycle; deterministic, or randomised:
 *                                 a random node among the repair_rcl best)
 *       y := LocalSearch(y)      (only in the LS version)
 *       if f(y) < f(x): x := y   (minimisation, so "better" means a lower objective)
 *
 * The local search is the best steepest search from lab 6: LM-based, full neighbourhood (2-opt + node exchange).
 * In the LS version x is always a local optimum and we keep its final LM. That lets the search after
 * destroy-repair use LocalSearchSteepestFast::run_from_perturbation: it evaluates only the moves that the
 * destroy-repair could have made improving, and it ends in exactly the same local optimum as a full search.
 * Acceptance is strict, so x is also the best solution found.
 */

#pragma once

#include <chrono>
#include <limits>
#include <random>
#include <utility>

#include "../../lab_6/algorithms/local_steepest_fast.hpp"
#include "utils/destroy_methods.hpp"
#include "utils/repair_methods.hpp"

struct lns_config {
    destroy_config destroy;
    repair_config repair;
    int repair_rcl = 1;        // 1: deterministic repair; > 1: random node among the repair_rcl best (randomised)
    bool local_search = true;  // apply LS after destroy-repair (it's always applied to the initial solution)
    bool reuse_memory = true;  // LS after destroy-repair via run_from_perturbation (same result, faster)
};

struct lns_result {
    algorithm_result best;
    int iterations = 0;  // iterations of the main loop (destroy-repair[-LS] steps)
    long long elapsed_microseconds = 0;
};

class LNS {
public:
    explicit LNS(LocalSearchSteepestFast& local_search) : local_search(local_search) {}

    /**
     * @param time_limit stopping condition (the average running time of MSLS)
     * @param seed       seed of the destroy operator; the random initial solution comes from local_search's own
     *                   generator, as in ILS
     */
    lns_result run(const algorithm_data& data, std::chrono::microseconds time_limit, const lns_config& cfg,
                   unsigned seed, std::vector<progress_point>* progress = nullptr,
                   int max_iterations = std::numeric_limits<int>::max()) {  // additional stopping condition
        using clock_type = std::chrono::steady_clock;
        const auto start = clock_type::now();
        std::mt19937 rng(seed);
        // progress (if given): a point for the initial solution and for every improvement
        auto record = [&](int iteration, int objective) {
            if (progress) {
                const auto us = std::chrono::duration_cast<std::chrono::microseconds>(clock_type::now() - start);
                progress->push_back({iteration, us.count(), objective});
            }
        };

        // final LMs of the current solution and of the last candidate (LS version only)
        LocalSearchSteepestFast::Memory current_memory, candidate_memory;
        algorithm_result current = local_search.run_random(data, &current_memory);
        record(0, current.total_weight);

        int iterations = 0;
        while (clock_type::now() - start < time_limit && iterations < max_iterations) {
            local_search_state state(current.path, data.nodes_num);
            Destroy::apply(data, state, cfg.destroy, rng);
            const int repaired_weight = cfg.repair_rcl > 1
                ? Repair::apply_randomized(data, state, cfg.repair, cfg.repair_rcl, rng)
                : Repair::apply(data, state, cfg.repair);
            algorithm_result candidate{state.cycle(), repaired_weight};

            if (cfg.local_search) {
                candidate = cfg.reuse_memory
                    ? local_search.run_from_perturbation(data, candidate, current, current_memory, &candidate_memory)
                    : local_search.run_from_solution(data, candidate, &candidate_memory);
            }
            ++iterations;

            if (candidate.total_weight < current.total_weight) {
                current = std::move(candidate);
                if (cfg.local_search) std::swap(current_memory, candidate_memory);
                record(iterations, current.total_weight);
            }
        }

        const long long elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(clock_type::now() - start).count();
        return {std::move(current), iterations, elapsed};
    }

private:
    LocalSearchSteepestFast& local_search;
};
