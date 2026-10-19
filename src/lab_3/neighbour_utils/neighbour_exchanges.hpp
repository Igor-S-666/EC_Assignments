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
#include "types.hpp"

/// Euclidean distance rounded to the nearest integer.
void intra_node_swap(const std::vector<int>& path, int i, int j) {
    std::swap(path[i], path[j]);
}

void inter_node_swap(const std::vector<int>& path, int i, int j) {
    std::swap(path[i], path[j]);
}

void intra_edge_swap(const std::vector<int>& path, int i, int j) {
    for(int k = 0; k < (j - i + 1) / 2; ++k) {
        std::swap(path[i + k], path[j - k]);
    }
}


exchange_function get_exchange_function(string exchange_type) {
    if (exchange_type == "intra_node_exchange") {
        return intra_node_swap;
    } else if (exchange_type == "inter_node_exchange") {
        return inter_node_swap;
    } else if (exchange_type == "intra_edge_exchange") {
        return intra_edge_swap;
    } else {
        throw std::invalid_argument("Invalid exchange type: " + exchange_type);
    }
}