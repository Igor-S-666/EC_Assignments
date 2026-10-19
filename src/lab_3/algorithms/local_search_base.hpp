
/**
 * @file local_search_base.hpp
 * @brief This file contains the base class for local search algorithms.
 */

#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
#include "../../algorithm_base.hpp"
#include "../../types.hpp"
#include "types.hpp"

class LocalSearchBase : public AlgorithmBase {
public:
    using LocalSearchBase::AlgorithmBase;

    virtual algorithm_result run(const algorithm_data& data, 
                                 std::string intra_cost_function_name,
                                 std::string inter_cost_function_name,
                                 std::string initial_solution_method,
                                 int initial_solution_start_node) = 0;

    algorithm_result run(const algorithm_data& data) override {
        return run(data, "node_exchange", "external_node_exchange","greedy_cycle", 0);
    }
};
