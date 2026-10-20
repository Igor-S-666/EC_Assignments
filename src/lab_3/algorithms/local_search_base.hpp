
/**
 * @file local_search_base.hpp
 * @brief This file contains the base class for local search algorithms.
 */

#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <stdexcept>
#include "../../algorithm_base.hpp"
#include "../../types.hpp"
#include "../types.hpp"
#include "../../lab_1/algorithms/greedy_cycle.hpp"
#include "../../lab_1/algorithms/random.hpp"

// Derived classes need `using LocalSearchBase::run;`, otherwise their
// 5-argument run hides run(data) (C++ name hiding).
class LocalSearchBase : public AlgorithmBase {
public:
    using AlgorithmBase::AlgorithmBase;

    virtual algorithm_result run(const algorithm_data& data,
                                 std::string intra_cost_function_name,
                                 std::string inter_cost_function_name,
                                 std::string initial_solution_method,
                                 int initial_solution_start_node) = 0;

    algorithm_result run(const algorithm_data& data) override {
        return run(data, "intra_node_exchange", "inter_node_exchange", "greedy_cycle", 0);
    }

protected:
    /**
     * @brief Builds the starting solution for the local search.
     * @param method "random" or "greedy_cycle" (both from lab 1)
     * @param start_node start node of the greedy cycle, ignored for "random"
     */
    algorithm_result initial_solution(const algorithm_data& data, const std::string& method, int start_node) {
        if (method == "random") {
            return RandomAlgorithm(gen()).run(data);
        } else if (method == "greedy_cycle") {
            return GreedyCycleAlgorithm(seed).run(data, start_node);
        }
        throw std::invalid_argument("Invalid initial solution method: " + method);
    }
};
