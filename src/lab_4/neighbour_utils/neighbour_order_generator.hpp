/** @file neighbour_order_generator.hpp
 *  @brief File for generating neighbour order for the TSP problem.
 */

#pragma once
#include <algorithm>
#include <array>
#include <random>
#include <stdexcept>
#include <vector>
#include "../types.hpp"

class NeighbourOrderGenerator {

    int path_size;
    int unselected_size;
    std::mt19937 rng;
public:
    NeighbourOrderGenerator(unsigned seed, int path_size, int unselected_size)
        : path_size(path_size), unselected_size(unselected_size), rng(seed) {
        if (path_size <= 0 || unselected_size < 0) {
            throw std::invalid_argument("Path size must be positive and unselected size non-negative.");
        }
    }

    neighbour_orders_t generate_neighbour_order() {
        /**
        * @brief Generate neighbour order for the TSP problem; contains both intra and inter neighborhood.
        * Path positions are padded, so they are in range 1..path_size.
        */
        neighbour_orders_t neighbour_orders;
        neighbour_orders.reserve(path_size * (path_size - 1) / 2 + path_size * unselected_size);
        // a cycle of <= 2 nodes has a single arrangement; intra cost formulas
        // assume distinct neighbours and would report fake improvements there
        const int intra_limit = path_size > 2 ? path_size : 0;
        for (int i = 1; i <= path_size; ++i) {
            for (int j = i + 1; j <= intra_limit; ++j) {
                neighbour_orders.push_back({i, j, 0});
            }
            for (int k = 0; k < unselected_size; ++k) {
                neighbour_orders.push_back({i, k, 1});
            }
        }
        std::shuffle(neighbour_orders.begin(), neighbour_orders.end(), rng);
        return neighbour_orders;
    }
};
