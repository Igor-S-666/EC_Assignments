/**
 * @file perturbations.hpp
 * @brief Perturbation operators for ILS, including large-scale moves.
 */

#pragma once

#include <algorithm>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "../../types.hpp"
#include "../../utils/neighbour_utils/types.hpp"
#include "../../utils/neighbour_utils/neighbour_exchanges.hpp"

enum class PerturbationType {
    MULTI_2OPT,
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
        case PerturbationType::MULTI_2OPT:        return "multi_2opt";
        case PerturbationType::DOUBLE_BRIDGE:     return "double_bridge";
        case PerturbationType::NODE_RELOCATION:   return "node_relocation";
        case PerturbationType::SEGMENT_SHUFFLE:   return "segment_shuffle";
        case PerturbationType::NODE_EXCHANGE:     return "node_exchange";
        case PerturbationType::SEGMENT_RELOCATION:return "segment_relocation";
        case PerturbationType::SEGMENT_EXCHANGE:  return "segment_exchange";
        case PerturbationType::LARGE_2OPT:        return "large_2opt";
        case PerturbationType::RUIN_RECREATE:     return "ruin_recreate";
    }
    return "unknown";
}

class Perturbations {
public:
    static algorithm_result apply(
        const algorithm_data& data,
        const algorithm_result& solution,
        PerturbationType type,
        int strength,
        std::mt19937& rng
    ) {
        if (strength <= 0) return solution;

        local_search_state state(solution.path, data.nodes_num);
        int weight = solution.total_weight;

        switch (type) {
            case PerturbationType::DOUBLE_BRIDGE:
                for (int i = 0; i < strength; ++i)
                    double_bridge(data, state, weight, rng);
                break;
            case PerturbationType::NODE_RELOCATION:
                for (int i = 0; i < strength; ++i)
                    node_relocation(data, state, weight, rng);
                break;
            case PerturbationType::SEGMENT_SHUFFLE:
                for (int i = 0; i < strength; ++i)
                    segment_shuffle(data, state, weight, rng);
                break;
            case PerturbationType::NODE_EXCHANGE:
                for (int i = 0; i < strength; ++i)
                    node_exchange(data, state, weight, rng);
                break;
            case PerturbationType::SEGMENT_RELOCATION:
                for (int i = 0; i < strength; ++i)
                    segment_relocation(data, state, weight, rng);
                break;
            case PerturbationType::SEGMENT_EXCHANGE:
                for (int i = 0; i < strength; ++i)
                    segment_exchange(data, state, weight, rng);
                break;
            case PerturbationType::LARGE_2OPT:
                for (int i = 0; i < strength; ++i)
                    large_2opt(data, state, weight, rng);
                break;
            case PerturbationType::RUIN_RECREATE:
                // One call already replaces a substantial fraction of the tour.
                ruin_recreate(data, state, weight, rng);
                break;
        }

        return {state.cycle(), weight};
    }

private:
    static long long calculate_distance(
        const algorithm_data& data,
        const std::vector<int>& order
    ) {
        long long total = 0;
        const int m = static_cast<int>(order.size());
        for (int i = 0; i < m; ++i)
            total += data.distance_matrix[order[i]][order[(i + 1) % m]];
        return total;
    }

    static void update_order(
        const algorithm_data& data,
        local_search_state& state,
        int& weight,
        const std::vector<int>& old_order,
        const std::vector<int>& new_order
    ) {
        const long long old_distance = calculate_distance(data, old_order);
        const long long new_distance = calculate_distance(data, new_order);
        weight += static_cast<int>(new_distance - old_distance);

        for (int i = 0; i < state.size(); ++i)
            state.path[i + 1] = new_order[i];

        state.sync_padding();
    }

    static std::vector<int> get_order(const local_search_state& state) {
        return state.cycle();
    }

    static void double_bridge(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m < 5) return;

        const auto order = get_order(state);
        const int a = std::uniform_int_distribution<int>(1, m - 4)(rng);
        const int b = std::uniform_int_distribution<int>(a + 1, m - 3)(rng);
        const int c = std::uniform_int_distribution<int>(b + 1, m - 2)(rng);
        const int d = std::uniform_int_distribution<int>(c + 1, m - 1)(rng);

        std::vector<int> result;
        result.reserve(m);
        result.insert(result.end(), order.begin(), order.begin() + a);
        result.insert(result.end(), order.begin() + c, order.begin() + d);
        result.insert(result.end(), order.begin() + b, order.begin() + c);
        result.insert(result.end(), order.begin() + a, order.begin() + b);
        result.insert(result.end(), order.begin() + d, order.end());

        update_order(data, state, weight, order, result);
    }

    static void node_relocation(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m < 3) return;

        auto order = get_order(state);
        const int from = std::uniform_int_distribution<int>(0, m - 1)(rng);
        const int to = std::uniform_int_distribution<int>(0, m - 1)(rng);
        if (from == to) return;

        const int node = order[from];
        order.erase(order.begin() + from);
        order.insert(order.begin() + to, node);
        update_order(data, state, weight, get_order(state), order);
    }

    static void segment_shuffle(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m < 3) return;

        auto order = get_order(state);
        const int i = std::uniform_int_distribution<int>(0, m - 2)(rng);
        const int j = std::uniform_int_distribution<int>(i + 1, m - 1)(rng);
        const auto old_order = order;
        std::shuffle(order.begin() + i, order.begin() + j + 1, rng);
        update_order(data, state, weight, old_order, order);
    }

    static void node_exchange(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m == 0 || state.unselected.empty()) return;

        const int i = std::uniform_int_distribution<int>(1, m)(rng);
        const int k = std::uniform_int_distribution<int>(
            0, static_cast<int>(state.unselected.size()) - 1
        )(rng);

        const int v = state.path[i];
        const int u = state.unselected[k];
        const int p = state.path[i - 1];
        const int n = state.path[i + 1];

        const int delta =
            data.cost_matrix[p][u] + data.distance_matrix[n][u] -
            data.cost_matrix[p][v] - data.distance_matrix[n][v];

        inter_node_swap(state, i, k);
        weight += delta;
    }

    // Removes a contiguous block and reinserts it elsewhere, preserving its order.
    static void segment_relocation(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m < 5) return;

        auto order = get_order(state);
        const int max_len = std::max(2, m / 3);
        const int len = std::uniform_int_distribution<int>(2, std::min(max_len, m - 1))(rng);
        const int from = std::uniform_int_distribution<int>(0, m - len)(rng);

        std::vector<int> block(order.begin() + from, order.begin() + from + len);
        order.erase(order.begin() + from, order.begin() + from + len);

        const int to = std::uniform_int_distribution<int>(
            0, static_cast<int>(order.size())
        )(rng);
        order.insert(order.begin() + to, block.begin(), block.end());

        update_order(data, state, weight, get_order(state), order);
    }

    // Swaps two disjoint blocks of equal length.
    static void segment_exchange(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m < 6) return;

        const int max_len = std::max(2, m / 4);
        const int len = std::uniform_int_distribution<int>(2, std::min(max_len, m / 2))(rng);

        std::vector<std::pair<int, int>> starts;
        for (int a = 0; a + len <= m; ++a)
            for (int b = a + len; b + len <= m; ++b)
                starts.emplace_back(a, b);

        if (starts.empty()) return;
        const auto chosen = starts[std::uniform_int_distribution<std::size_t>(
            0, starts.size() - 1
        )(rng)];

        auto order = get_order(state);
        const auto old_order = order;
        std::swap_ranges(
            order.begin() + chosen.first,
            order.begin() + chosen.first + len,
            order.begin() + chosen.second
        );
        update_order(data, state, weight, old_order, order);
    }

    // Reverses a randomly selected long segment (never the whole cycle).
    static void large_2opt(
        const algorithm_data& data, local_search_state& state,
        int& weight, std::mt19937& rng
    ) {
        const int m = state.size();
        if (m < 4) return;

        auto order = get_order(state);
        const int len = std::uniform_int_distribution<int>(3, m - 1)(rng);
        const int from = std::uniform_int_distribution<int>(0, m - len)(rng);
        const auto old_order = order;
        std::reverse(order.begin() + from, order.begin() + from + len);
        update_order(data, state, weight, old_order, order);
    }

   // Strong ruin-and-recreate: replace roughly one quarter of selected nodes
// with currently unselected nodes at distinct tour positions.
static void ruin_recreate(
    const algorithm_data& data,
    local_search_state& state,
    int& weight,
    std::mt19937& rng
) {
    const int m = state.size();

    if (m == 0 || state.unselected.empty()) {
        return;
    }

    const int count = std::min(
        static_cast<int>(state.unselected.size()),
        std::max(2, m / 8)
    );

    // Select distinct positions in the current tour.
    std::vector<int> positions(m);
    for (int i = 0; i < m; ++i) {
        positions[i] = i + 1;
    }

    std::shuffle(positions.begin(), positions.end(), rng);

    // Replace selected nodes with unselected nodes.
    for (int i = 0; i < count; ++i) {
        if (state.unselected.empty()) {
            break;
        }

        const int pos = positions[i];

        const int k = std::uniform_int_distribution<int>(
            0,
            static_cast<int>(state.unselected.size()) - 1
        )(rng);

        inter_node_swap(state, pos, k);
    }

    // Recalculate the complete objective after all replacements.
    // This avoids accumulating incorrect deltas across multiple swaps.
    weight = 0;

    for (int i = 1; i <= m; ++i) {
        const int prev = state.path[i - 1];
        const int curr = state.path[i];
        const int next = state.path[i + 1];

        weight += data.cost_matrix[prev][curr]
                + data.distance_matrix[next][curr];
    }
}
};
