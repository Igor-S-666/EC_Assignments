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
inline int intra_node_exchange_cost(const distance_matrix_t& cost_matrix, 
    int i, int j, const std::vector<int>& path) {
    return cost_matrix[path[i - 1]][path[j]] + cost_matrix[path[j]][path[i + 1]] +
           cost_matrix[path[j - 1]][path[i]] + cost_matrix[path[i]][path[j + 1]] -
           (cost_matrix[path[i - 1]][path[i]] + cost_matrix[path[i]][path[i + 1]] +
            cost_matrix[path[j - 1]][path[j]] + cost_matrix[path[j]][path[j + 1]]);
}

inline int intra_edge_exchange_cost(const distance_matrix_t& cost_matrix, 
    int i, int j, const std::vector<int>& path) {
    return cost_matrix[path[i]][path[j]] + cost_matrix[path[i + 1]][path[j + 1]] -
           (cost_matrix[path[i]][path[i + 1]] + cost_matrix[path[j]][path[j + 1]]);
}

inline int external_node_exchange_cost(const distance_matrix_t& cost_matrix, 
    int existing_node, int new_node, const std::vector<int>& path) {
    return cost_matrix[path[existing_node - 1]][path[new_node]] 
           + cost_matrix[path[new_node]][path[existing_node + 1]] 
           - cost_matrix[path[existing_node - 1]][path[existing_node]] 
           - cost_matrix[path[existing_node]][path[existing_node + 1]];
}

cost_function get_cost_function(const std::string& cost_function_name) {
    if (cost_function_name == "intra_node_exchange") {
        return intra_node_exchange_cost;
    } else if (cost_function_name == "intra_edge_exchange") {
        return intra_edge_exchange_cost;
    } else if (cost_function_name == "inter_node_exchange") {
        return external_node_exchange_cost;
    } else {
        throw std::invalid_argument("Invalid cost function name: " + cost_function_name);
    }
}
        