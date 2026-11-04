
/**
 * @file local_search_base.hpp
 * @brief This file contains the base class for local search algorithms with the neighbourhood fixed
 *  to the best one from lab 3 (intra edge exchange + inter node exchange).
 *  It has its own name, so lab 5 can also use LocalSearchBase-based algorithms of lab 4.
 */

#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <stdexcept>
#include "../../algorithm_base.hpp"
#include "../../types.hpp"
#include "../../utils/neighbour_utils/types.hpp"
#include "../../lab_1/algorithms/greedy_cycle.hpp"
#include "../../lab_1/algorithms/random.hpp"

// Derived classes need `using FixedLocalSearchBase::run;`, otherwise their
// 3-argument run hides run(data) (C++ name hiding).
class FixedLocalSearchBase : public AlgorithmBase {
public:
    using AlgorithmBase::AlgorithmBase;

    virtual algorithm_result run(const algorithm_data& data,
                                 std::string initial_solution_method,
                                 int initial_solution_start_node) = 0;

    algorithm_result run(const algorithm_data& data) override {
        return run(data, "random", 0);
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
