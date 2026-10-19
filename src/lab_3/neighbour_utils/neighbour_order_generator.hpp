/** @file main.cpp
 *  @brief FIle for generating neighbour order for the TSP problem.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <array>
#include <random>

class NeighbourOrderGenerator {

    int num_nodes;
    std::mt19937 rng;
public:
    NeighbourOrderGenerator(int seed = 0, int num_nodes) : num_nodes(num_nodes), rng(seed) {
        if (num_nodes <= 0) {
            throw std::invalid_argument("Number of nodes must be positive.");
        }
    }

    std::vector<std::array<int, 3>> generate_neighbour_order() {
        /**
        * @brief Generate neighbour order for the TSP problem; contains both intra and inter neighborhood
        */
        for (int i = 0; i < num_nodes; ++i) {
            for (int j = i + 1; j < num_nodes; ++j) {
                neighbour_orders.push_back({i, j, 0});
                neighbour_orders.push_back({i, j, 1}); 
            }
        }
        std::shuffle(neighbour_orders.begin(), neighbour_orders.end(), rng);
        return neighbour_orders;
    }    
};
        