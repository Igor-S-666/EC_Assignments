/** @file neighbour_exchanges.hpp
 *  @brief File with functions applying neighbourhood moves to the local search state.
 */

#pragma once
#include <algorithm>
#include <utility>
#include <vector>
#include "../types.hpp"

inline void intra_node_swap(local_search_state& state, int i, int j) {
    std::swap(state.path[i], state.path[j]);
    state.sync_padding();
}

inline void inter_node_swap(local_search_state& state, int position, int unselected_index) {
    // old node goes to the unselected pool, so both sets stay consistent
    std::swap(state.path[position], state.unselected[unselected_index]);
    state.sync_padding();
}

inline void intra_edge_swap(local_search_state& state, int i, int j) {
    std::reverse(state.path.begin() + i + 1, state.path.begin() + j + 1);
    state.sync_padding();
}
