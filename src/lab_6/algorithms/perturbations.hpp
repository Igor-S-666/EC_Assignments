/**
 * @file perturbations.hpp
 * @brief Perturbation operators for ILS, including large-scale moves.
 *
 * Every operator works in place on the cycle (a plain vector of node ids). The objective of the result is
 * recomputed once at the end, which costs O(m) - negligible next to the local search that follows - and can't drift
 * the way accumulated deltas can.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "../../types.hpp"

enum class PerturbationType {
    DOUBLE_BRIDGE,
    NODE_RELOCATION,
    SEGMENT_SHUFFLE,
    NODE_EXCHANGE,
    SEGMENT_RELOCATION,
    SEGMENT_EXCHANGE,
    LARGE_2OPT,
    RUIN_RECREATE
};

inline std::string perturbation_name(PerturbationType type) {
    switch (type) {
        case PerturbationType::DOUBLE_BRIDGE:      return "double_bridge";
        case PerturbationType::NODE_RELOCATION:    return "node_relocation";
        case PerturbationType::SEGMENT_SHUFFLE:    return "segment_shuffle";
        case PerturbationType::NODE_EXCHANGE:      return "node_exchange";
        case PerturbationType::SEGMENT_RELOCATION: return "segment_relocation";
        case PerturbationType::SEGMENT_EXCHANGE:   return "segment_exchange";
        case PerturbationType::LARGE_2OPT:         return "large_2opt";
        case PerturbationType::RUIN_RECREATE:      return "ruin_recreate";
    }
    return "unknown";
}

class Perturbations {
public:
    /**
     * @brief Returns a perturbed copy of `solution`.
     * @param strength number of times the operator is applied (Ruin-Recreate: applied once, see ruin_recreate)
     */
    static algorithm_result apply(
        const algorithm_data& data,
        const algorithm_result& solution,
        PerturbationType type,
        int strength,
        std::mt19937& rng
    ) {
        if (strength <= 0) return solution;

        std::vector<int> cycle = solution.path;
        std::vector<int> unselected;
        if (type == PerturbationType::NODE_EXCHANGE || type == PerturbationType::RUIN_RECREATE) {
            unselected = unselected_nodes(data, cycle);
        }

        if (type == PerturbationType::RUIN_RECREATE) {
            // One call already replaces a substantial fraction of the tour.
            ruin_recreate(cycle, unselected, rng);
        } else {
            for (int i = 0; i < strength; ++i) {
                switch (type) {
                    case PerturbationType::DOUBLE_BRIDGE:      double_bridge(cycle, rng); break;
                    case PerturbationType::NODE_RELOCATION:    node_relocation(cycle, rng); break;
                    case PerturbationType::SEGMENT_SHUFFLE:    segment_shuffle(cycle, rng); break;
                    case PerturbationType::NODE_EXCHANGE:      node_exchange(cycle, unselected, rng); break;
                    case PerturbationType::SEGMENT_RELOCATION: segment_relocation(cycle, rng); break;
                    case PerturbationType::SEGMENT_EXCHANGE:   segment_exchange(cycle, rng); break;
                    case PerturbationType::LARGE_2OPT:         large_2opt(cycle, rng); break;
                    case PerturbationType::RUIN_RECREATE:      break;  // handled above
                }
            }
        }

        const int weight = cycle_weight(data, cycle);
        return {std::move(cycle), weight};
    }

private:
    static int random_int(std::mt19937& rng, int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(rng);
    }

    // objective: sum of cost_matrix over the edges (cost_matrix[i][j] includes the weight of j)
    static int cycle_weight(const algorithm_data& data, const std::vector<int>& cycle) {
        int weight = 0;
        const std::size_t m = cycle.size();
        for (std::size_t i = 0; i + 1 < m; ++i) weight += data.cost_matrix[cycle[i]][cycle[i + 1]];
        if (m > 0) weight += data.cost_matrix[cycle[m - 1]][cycle[0]];
        return weight;
    }

    static std::vector<int> unselected_nodes(const algorithm_data& data, const std::vector<int>& cycle) {
        std::vector<char> selected(data.nodes_num, 0);
        for (const int node : cycle) selected[node] = 1;
        std::vector<int> unselected;
        unselected.reserve(data.nodes_num - cycle.size());
        for (int node = 0; node < data.nodes_num; ++node) {
            if (!selected[node]) unselected.push_back(node);
        }
        return unselected;
    }

    // Classic double bridge: cuts the cycle into A B C D and reconnects it as A C B D
    // (4 edges replaced, a move that can't be undone by a single 2-opt).
    static void double_bridge(std::vector<int>& cycle, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m < 4) return;
        const int p1 = random_int(rng, 1, m - 3);
        const int p2 = random_int(rng, p1 + 1, m - 2);
        const int p3 = random_int(rng, p2 + 1, m - 1);
        // A = [0, p1), B = [p1, p2), C = [p2, p3), D = [p3, m)
        std::rotate(cycle.begin() + p1, cycle.begin() + p2, cycle.begin() + p3);
    }

    // Moves one node to another position.
    static void node_relocation(std::vector<int>& cycle, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m < 3) return;
        const int from = random_int(rng, 0, m - 1);
        const int to = random_int(rng, 0, m - 1);
        if (from < to) {
            std::rotate(cycle.begin() + from, cycle.begin() + from + 1, cycle.begin() + to + 1);
        } else if (to < from) {
            std::rotate(cycle.begin() + to, cycle.begin() + from, cycle.begin() + from + 1);
        }
    }

    // Randomly reorders the nodes of a random segment [i, j] (of length 2..m).
    static void segment_shuffle(std::vector<int>& cycle, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m < 3) return;
        const int i = random_int(rng, 0, m - 2);
        const int j = random_int(rng, i + 1, m - 1);
        std::shuffle(cycle.begin() + i, cycle.begin() + j + 1, rng);
    }

    // Replaces a random selected node with a random unselected one.
    static void node_exchange(std::vector<int>& cycle, std::vector<int>& unselected, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m == 0 || unselected.empty()) return;
        const int i = random_int(rng, 0, m - 1);
        const int k = random_int(rng, 0, static_cast<int>(unselected.size()) - 1);
        std::swap(cycle[i], unselected[k]);  // the removed node becomes unselected
    }

    // Removes a contiguous block (2..m/3 nodes) and reinserts it elsewhere, preserving its order.
    static void segment_relocation(std::vector<int>& cycle, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m < 5) return;
        const int max_len = std::max(2, m / 3);
        const int len = random_int(rng, 2, std::min(max_len, m - 1));
        const int from = random_int(rng, 0, m - len);
        // insertion index in the cycle without the block
        const int to = random_int(rng, 0, m - len);
        if (to < from) {
            std::rotate(cycle.begin() + to, cycle.begin() + from, cycle.begin() + from + len);
        } else if (to > from) {
            std::rotate(cycle.begin() + from, cycle.begin() + from + len, cycle.begin() + to + len);
        }
    }

    // Swaps two disjoint blocks of equal length (2..m/4). Blocks shorter than m/2 guarantee that the swap isn't
    // just a rotation of the cycle (two halves swapped).
    static void segment_exchange(std::vector<int>& cycle, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m < 6) return;
        const int max_len = std::min(std::max(2, m / 4), (m - 1) / 2);
        const int len = random_int(rng, 2, max_len);
        // each (a, b) pair with a + len <= b, b + len <= m is chosen with equal probability:
        // the number of valid b for a given a is m - 2 * len - a + 1, so draw a pair index and decode it
        const int k = m - 2 * len + 1;              // number of valid a
        const int pairs = k * (k + 1) / 2;
        int index = random_int(rng, 0, pairs - 1);
        int a = 0;
        while (index >= k - a) {
            index -= k - a;
            ++a;
        }
        const int b = a + len + index;
        std::swap_ranges(cycle.begin() + a, cycle.begin() + a + len, cycle.begin() + b);
    }

    // Reverses a random segment of 3..m-3 nodes. Reversing L nodes changes the same two edges as reversing the
    // other m - L, so longer segments would only repeat these moves (m - 1 or m nodes: no change at all).
    static void large_2opt(std::vector<int>& cycle, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m < 6) return;
        const int len = random_int(rng, 3, m - 3);
        const int from = random_int(rng, 0, m - len);
        std::reverse(cycle.begin() + from, cycle.begin() + from + len);
    }

    // Replaces max(2, m / 8) selected nodes, at distinct positions, with distinct unselected nodes.
    static void ruin_recreate(std::vector<int>& cycle, std::vector<int>& unselected, std::mt19937& rng) {
        const int m = static_cast<int>(cycle.size());
        if (m == 0 || unselected.empty()) return;
        const int count = std::min({static_cast<int>(unselected.size()), m, std::max(2, m / 8)});

        // partial Fisher-Yates: the first `count` entries become distinct random positions / unselected nodes,
        // so a node removed here is never put back in the same call
        std::vector<int> positions(m);
        for (int i = 0; i < m; ++i) positions[i] = i;
        const int u = static_cast<int>(unselected.size());
        for (int i = 0; i < count; ++i) {
            std::swap(positions[i], positions[random_int(rng, i, m - 1)]);
            std::swap(unselected[i], unselected[random_int(rng, i, u - 1)]);
            std::swap(cycle[positions[i]], unselected[i]);
        }
    }
};
