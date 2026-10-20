/** @file neighbor_cost_calculator.hpp
 *  @brief File with functions computing objective change of neighbourhood moves.
 *
 *  Intra moves don't change the set of selected nodes, so they should get the plain
 *  distance_matrix (edge reversal in intra_edge_exchange would break with the
 *  asymmetric cost_matrix). Inter moves change the node set, so they need cost_matrix.
 */

#pragma once
#include <vector>
#include "../types.hpp"

/// Cost of swapping nodes at positions i < j, where `first` directly precedes `second` in the cycle.
inline int adjacent_node_exchange_cost(const distance_matrix_t& cost_matrix,
    int prev, int first, int second, int next) {
    return cost_matrix[prev][second] + cost_matrix[second][first] + cost_matrix[first][next] -
           (cost_matrix[prev][first] + cost_matrix[first][second] + cost_matrix[second][next]);
}

inline int intra_node_exchange_cost(const distance_matrix_t& cost_matrix,
    int i, int j, const local_search_state& state) {
    const std::vector<int>& path = state.path;
    const int m = state.size();
    if (j == i + 1) {
        return adjacent_node_exchange_cost(cost_matrix, path[i - 1], path[i], path[j], path[j + 1]);
    }
    if (i == 1 && j == m) {
        // adjacent through the cycle closure: path[m] precedes path[1]
        return adjacent_node_exchange_cost(cost_matrix, path[m - 1], path[m], path[1], path[2]);
    }
    return cost_matrix[path[i - 1]][path[j]] + cost_matrix[path[j]][path[i + 1]] +
           cost_matrix[path[j - 1]][path[i]] + cost_matrix[path[i]][path[j + 1]] -
           (cost_matrix[path[i - 1]][path[i]] + cost_matrix[path[i]][path[i + 1]] +
            cost_matrix[path[j - 1]][path[j]] + cost_matrix[path[j]][path[j + 1]]);
}

/// 2-opt: remove edges (i, i + 1) and (j, j + 1), reverse the segment i + 1..j.
inline int intra_edge_exchange_cost(const distance_matrix_t& cost_matrix,
    int i, int j, const local_search_state& state) {
    const std::vector<int>& path = state.path;
    return cost_matrix[path[i]][path[j]] + cost_matrix[path[i + 1]][path[j + 1]] -
           (cost_matrix[path[i]][path[i + 1]] + cost_matrix[path[j]][path[j + 1]]);
}

/// Replace node at path position `position` with unselected[unselected_index].
inline int external_node_exchange_cost(const distance_matrix_t& cost_matrix,
    int position, int unselected_index, const local_search_state& state) {
    const std::vector<int>& path = state.path;
    const int new_node = state.unselected[unselected_index];
    return cost_matrix[path[position - 1]][new_node]
           + cost_matrix[new_node][path[position + 1]]
           - cost_matrix[path[position - 1]][path[position]]
           - cost_matrix[path[position]][path[position + 1]];
}
