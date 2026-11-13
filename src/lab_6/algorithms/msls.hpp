
/**
 * @file msls.hpp
 * @brief Multiple Start Local Search.
 */

#pragma once

#include <limits>
#include <utility>

#include "local_steepest_fast.hpp"


class MSLS {
public:

    explicit MSLS(LocalSearchSteepestFast& local_search)
        : local_search(local_search) {}

    /**
     * @brief Performs multiple independent random local searches.
     *
     * @param data Problem instance.
     * @param iterations Number of random starting solutions.
     */
    algorithm_result run(
        const algorithm_data& data,
        int iterations = 400
    ) {
        algorithm_result best{
            {},
            std::numeric_limits<int>::max()
        };

        for (int i = 0; i < iterations; ++i) {

            // run(data) generates a random starting solution.
            algorithm_result current = local_search.run(data);

            if (current.total_weight < best.total_weight) {
                best = std::move(current);
            }
        }

        return best;
    }

private:
    LocalSearchSteepestFast& local_search;
};
