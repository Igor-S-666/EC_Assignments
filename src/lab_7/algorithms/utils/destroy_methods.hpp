/**
 * @file destroy_methods.hpp
 * @brief Destroy operators for Large Neighbourhood Search.
 *
 * Every operator removes `fraction` (default 30%) of the m nodes of the current cycle. It works in place on a
 * local_search_state: the kept nodes stay in their original cyclic order (the padding is rebuilt) and the removed
 * nodes are appended to `unselected`, so a greedy-insertion repair can extend the partial cycle directly.
 *
 * Operators (DestroyType):
 *  - RANDOM_NODES   - k single positions, no two the same.
 *  - SINGLE_SEGMENT - one subpath of k consecutive nodes.
 *  - MULTI_SEGMENT  - s in [min_segments, max_segments] subpaths whose lengths add up to k (a random split
 *                     with each length >= 1). When possible, the segments don't touch each other. Otherwise two of
 *                     them would merge into one longer segment.
 *
 * Selection (destroy_config::weighted):
 *  - false - uniform: every node / every start position is equally likely.
 *  - true  - randomised heuristic: the probability is proportional to score^exponent, where
 *      RANDOM_NODES: score = weight of the node (expensive nodes are removed more often),
 *      segments:     score = cost of the subpath = weights of its L nodes + lengths of its L + 1 edges
 *                    (the edge into the segment, the L - 1 inner edges and the edge out of it).
 *    Scores are clamped to >= 1, so every candidate keeps a non-zero chance. exponent = 0 gives uniform selection,
 *    and larger exponents make the choice greedier.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "../../../types.hpp"
#include "../../../utils/neighbour_utils/types.hpp"

enum class DestroyType {
    RANDOM_NODES,
    SINGLE_SEGMENT,
    MULTI_SEGMENT
};

struct destroy_config {
    DestroyType type = DestroyType::RANDOM_NODES;
    bool weighted = false;   // false: uniform selection, true: probability ~ score^exponent
    double fraction = 0.3;   // part of the cycle that is removed
    double exponent = 1.0;   // only used when weighted
    int min_segments = 3;    // only used by MULTI_SEGMENT
    int max_segments = 5;
};

inline std::string destroy_name(const destroy_config& cfg) {
    std::string name;
    switch (cfg.type) {
        case DestroyType::RANDOM_NODES:   name = "random_nodes"; break;
        case DestroyType::SINGLE_SEGMENT: name = "single_segment"; break;
        case DestroyType::MULTI_SEGMENT:  name = "multi_segment"; break;
    }
    return cfg.weighted ? "weighted_" + name : name;
}

class Destroy {
public:
    /**
     * @brief Removes nodes from `state` in place according to `cfg`.
     * @return ids of the removed nodes (they are also appended to state.unselected)
     */
    static std::vector<int> apply(
        const algorithm_data& data,
        local_search_state& state,
        const destroy_config& cfg,
        std::mt19937& rng
    ) {
        const int m = state.size();
        if (m <= 2) return {};

        const std::vector<int> cycle = state.cycle();
        const int k = removal_count(m, cfg.fraction);
        std::vector<char> removed(m, 0);

        switch (cfg.type) {
            case DestroyType::RANDOM_NODES:
                select_nodes(data, cycle, k, cfg, rng, removed);
                break;
            case DestroyType::SINGLE_SEGMENT:
                select_segments(data, cycle, k, 1, cfg, rng, removed);
                break;
            case DestroyType::MULTI_SEGMENT: {
                const int lo = std::max(1, std::min(cfg.min_segments, cfg.max_segments));
                const int hi = std::max(lo, cfg.max_segments);
                select_segments(data, cycle, k, random_int(rng, lo, hi), cfg, rng, removed);
                break;
            }
        }

        return remove_marked(state, cycle, removed);
    }

private:
    static int random_int(std::mt19937& rng, int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(rng);
    }

    // Number of nodes to remove: round(fraction * m), at least 1. At least 2 nodes are kept, so the repair always
    // starts from a proper cycle.
    static int removal_count(int m, double fraction) {
        const int k = static_cast<int>(std::lround(fraction * m));
        return std::clamp(k, 1, m - 2);
    }

    static double to_weight(double score, double exponent) {
        return std::pow(std::max(score, 1.0), exponent);
    }

    // Chooses k distinct indices, where index i is picked with probability proportional to weights[i]
    // (Efraimidis-Spirakis: key_i = Exp(1) / w_i, keep the k smallest keys). Runs in O(n).
    static std::vector<int> weighted_sample(const std::vector<double>& weights, int k, std::mt19937& rng) {
        const int n = static_cast<int>(weights.size());
        k = std::min(k, n);
        std::exponential_distribution<double> exp1(1.0);
        std::vector<std::pair<double, int>> keys(n);
        for (int i = 0; i < n; ++i) {
            const double key = weights[i] > 0.0 ? exp1(rng) / weights[i]
                                                : std::numeric_limits<double>::infinity();
            keys[i] = {key, i};
        }
        std::nth_element(keys.begin(), keys.begin() + k, keys.end());
        std::vector<int> chosen(k);
        for (int i = 0; i < k; ++i) chosen[i] = keys[i].second;
        return chosen;
    }

    // Random split of `total` into `parts` positive integers: parts - 1 distinct cut points in [1, total - 1].
    static std::vector<int> random_composition(int total, int parts, std::mt19937& rng) {
        if (parts <= 1) return {total};
        std::vector<int> cuts(total - 1);
        for (int i = 0; i < total - 1; ++i) cuts[i] = i + 1;
        for (int i = 0; i < parts - 1; ++i) {  // partial Fisher-Yates
            std::swap(cuts[i], cuts[random_int(rng, i, total - 2)]);
        }
        cuts.resize(parts - 1);
        std::sort(cuts.begin(), cuts.end());
        std::vector<int> lengths(parts);
        int prev = 0;
        for (int i = 0; i < parts - 1; ++i) {
            lengths[i] = cuts[i] - prev;
            prev = cuts[i];
        }
        lengths[parts - 1] = total - prev;
        return lengths;
    }

    // ---- random nodes --------------------------------------------------------------------------------------------

    static void select_nodes(
        const algorithm_data& data,
        const std::vector<int>& cycle,
        int k,
        const destroy_config& cfg,
        std::mt19937& rng,
        std::vector<char>& removed
    ) {
        const int m = static_cast<int>(cycle.size());
        std::vector<double> weights(m, 1.0);
        if (cfg.weighted) {
            for (int i = 0; i < m; ++i) {
                weights[i] = to_weight(data.node_weights[cycle[i]], cfg.exponent);
            }
        }
        for (const int pos : weighted_sample(weights, k, rng)) removed[pos] = 1;
    }

    // ---- subpaths ------------------------------------------------------------------------------------------------

    static void select_segments(
        const algorithm_data& data,
        const std::vector<int>& cycle,
        int k,
        int segments,
        const destroy_config& cfg,
        std::mt19937& rng,
        std::vector<char>& removed
    ) {
        const int m = static_cast<int>(cycle.size());
        segments = std::clamp(segments, 1, k);
        std::vector<int> lengths = random_composition(k, segments, rng);
        // place the longest segments first, while the most free space is left
        std::sort(lengths.rbegin(), lengths.rend());

        // Prefix sums over the cycle written out twice, so a window that wraps around is still one contiguous range.
        //   node_pref: node weights; edge_pref: edge_in(i) = d(cycle[i - 1], cycle[i]).
        // cost(p, L) = weights of positions p..p+L-1 + edges entering p..p+L (the last one is the exit edge).
        std::vector<long long> node_pref, edge_pref;
        if (cfg.weighted) {
            node_pref.assign(2 * m + 1, 0);
            edge_pref.assign(2 * m + 1, 0);
            for (int i = 0; i < 2 * m; ++i) {
                const int cur = cycle[i % m];
                const int prev = cycle[(i - 1 + m) % m];
                node_pref[i + 1] = node_pref[i] + data.node_weights[cur];
                edge_pref[i + 1] = edge_pref[i] + data.distance_matrix[prev][cur];
            }
        }
        auto segment_cost = [&](int p, int len) -> double {
            return static_cast<double>(node_pref[p + len] - node_pref[p])
                 + static_cast<double>(edge_pref[p + len + 1] - edge_pref[p]);
        };

        std::vector<int> removed_pref(2 * m + 1, 0);
        std::vector<double> weights(m);

        for (std::size_t s = 0; s < lengths.size(); ++s) {
            const int len = lengths[s];

            for (int i = 0; i < 2 * m; ++i) removed_pref[i + 1] = removed_pref[i] + removed[i % m];
            // number of removed positions in the cyclic window [from, from + count)
            auto removed_in = [&](int from, int count) {
                from = (from % m + m) % m;
                return removed_pref[from + count] - removed_pref[from];
            };

            // strict: the window and both of its neighbours are free (the segments don't touch);
            // relaxed: only the window itself is free
            bool any = false;
            for (int strict = 1; strict >= 0 && !any; --strict) {
                for (int p = 0; p < m; ++p) {
                    const bool free = strict ? (len + 2 <= m && removed_in(p - 1, len + 2) == 0)
                                             : removed_in(p, len) == 0;
                    if (free) {
                        weights[p] = cfg.weighted ? to_weight(segment_cost(p, len), cfg.exponent) : 1.0;
                        any = true;
                    } else {
                        weights[p] = 0.0;
                    }
                }
            }

            if (!any) {
                // Fragmented cycle (only possible for big fractions): remove the rest as single random nodes.
                int remaining = 0;
                for (std::size_t r = s; r < lengths.size(); ++r) remaining += lengths[r];
                std::vector<double> free_weights(m);
                for (int i = 0; i < m; ++i) free_weights[i] = removed[i] ? 0.0 : 1.0;
                for (const int pos : weighted_sample(free_weights, remaining, rng)) removed[pos] = 1;
                return;
            }

            std::discrete_distribution<int> pick(weights.begin(), weights.end());
            const int start = pick(rng);
            for (int j = 0; j < len; ++j) removed[(start + j) % m] = 1;
        }
    }

    // ---- applying the removal ------------------------------------------------------------------------------------

    static std::vector<int> remove_marked(
        local_search_state& state,
        const std::vector<int>& cycle,
        const std::vector<char>& removed
    ) {
        const int m = static_cast<int>(cycle.size());
        std::vector<int> kept, removed_nodes;
        kept.reserve(m);
        for (int i = 0; i < m; ++i) {
            (removed[i] ? removed_nodes : kept).push_back(cycle[i]);
        }

        state.path.clear();
        state.path.reserve(kept.size() + 2);
        state.path.push_back(kept.back());
        state.path.insert(state.path.end(), kept.begin(), kept.end());
        state.path.push_back(kept.front());

        state.unselected.insert(state.unselected.end(), removed_nodes.begin(), removed_nodes.end());
        return removed_nodes;
    }
};
