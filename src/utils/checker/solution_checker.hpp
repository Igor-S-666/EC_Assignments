/**
 * @brief This file contains function to verify the correctness of a solution.
 */

#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "../../types.hpp"

/**
 * @brief Checks if the given solution is valid.
 * @param data The input data.
 * @param result The solution to check.
 * @return empty string if the solution is valid, otherwise the reason why it is not.
 */
inline std::string check_solution(const algorithm_data& data, const algorithm_result& result) {
    const int n = data.nodes_num;
    // check if exactly half of the nodes are visited (rounded up)
    const std::size_t expected_size = (n + 1) / 2;
    if(result.path.size() != expected_size) {
        return "path has " + std::to_string(result.path.size()) + " nodes, expected " + std::to_string(expected_size);
    }
    // check if all nodes in the path are unique
    std::vector<bool> visited(n, false);
    for(int node : result.path) {
        if(node < 0 || node >= n) {
            return "invalid node index " + std::to_string(node);
        }
        if(visited[node]) {
            return "duplicate node " + std::to_string(node);
        }
        visited[node] = true;
    }
    // check if the total weight is correct (independently of cost_matrix)
    int calculated_weight = 0;
    for(std::size_t i = 0; i < result.path.size(); ++i) {
        int current_node = result.path[i];
        calculated_weight += data.node_weights[current_node];
        int next_node = result.path[(i + 1) % result.path.size()];  // wrap around to the first node
        calculated_weight += data.distance_matrix[current_node][next_node];
    }
    if(calculated_weight != result.total_weight) {
        return "total weight is " + std::to_string(result.total_weight) + ", expected " + std::to_string(calculated_weight);
    }
    return "";
}
