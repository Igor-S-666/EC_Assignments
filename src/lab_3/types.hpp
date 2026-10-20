/**
 * @brief This files contain structures and types used in lab_3.
 */

#pragma once
#include <array>
#include <utility>
#include <vector>
#include "../types.hpp"

/// {first, second, exchange_type}; exchange_type 0 = intra (two path positions),
/// 1 = inter (path position, index into unselected)
using neighbour_order_t = std::array<int, 3>;
using neighbour_orders_t = std::vector<neighbour_order_t>;

struct local_search_state {
    // padded cycle: real nodes live at positions 1..m, path[0] == path[m] and
    // path[m + 1] == path[1], so neighbours of i are always i - 1 and i + 1 (no modulo)
    std::vector<int> path;
    // nodes not in the solution
    std::vector<int> unselected;

    local_search_state(const std::vector<int>& cycle, int nodes_num) {
        path.reserve(cycle.size() + 2);
        path.push_back(cycle.back());
        path.insert(path.end(), cycle.begin(), cycle.end());
        path.push_back(cycle.front());

        std::vector<bool> in_path(nodes_num, false);
        for (int node : cycle) {
            in_path[node] = true;
        }
        for (int node = 0; node < nodes_num; ++node) {
            if (!in_path[node]) {
                unselected.push_back(node);
            }
        }
    }

    int size() const { return static_cast<int>(path.size()) - 2; }

    /// Must be called after every move that may change position 1 or m.
    void sync_padding() {
        path.front() = path[path.size() - 2];
        path.back() = path[1];
    }

    std::vector<int> cycle() const { return {path.begin() + 1, path.end() - 1}; }
};
