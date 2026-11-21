/**
 * @file repair_methods.hpp
 * @brief Repair operator for Large Neighbourhood Search.
 *
 * Repair = the best greedy heuristic from lab 2: the weighted 2-regret greedy cycle
 * (score = regret_weight * (2nd best - best insertion cost) - cost_weight * best insertion cost,
 * default weights 0.3 / 0.7, see WeightedRegret2GreedyCycleAlgorithm).
 * It's cycle-based, so it continues straight from the partial cycle left by the destroy operator. No cycle breaking
 * is needed.
 *
 * The repair works in place on a local_search_state: nodes from `unselected` are inserted into the cycle until it
 * holds ceil(n / 2) nodes. The padding and `unselected` are kept consistent.
 *
 * Cached insertion costs
 * ----------------------
 * Recomputing every insertion cost after every insertion costs O(k * u * m) (k insertions, u unselected nodes,
 * m cycle edges). Instead, each unselected node keeps its cheapest and second-cheapest insertion
 * (cost + edge, an edge being named by its start node s: s -> next[s]). Inserting v into a -> b removes only that
 * edge and adds a -> v, v -> b, so for every other node:
 *  - if its best or second edge was a -> b, its insertions are recomputed over the whole cycle (O(m)),
 *  - otherwise the two new edges are compared with its cached pair (O(1)).
 * Total: O(u * m) for the initial pass + O(u + affected * m) per insertion.
 *
 * Two versions: apply() is deterministic, and apply_randomized() picks a random node among the rcl_size best
 * ones (GRASP-like restricted candidate list). Both use the same cached costs.
 *
 * The result is exactly the same as recomputing everything (and as the lab-2 implementation). Ties between edges
 * are broken by their order in the cycle vector, the earliest edge winning, like the naive scan. An insertion keeps
 * the relative order of all other edges, and the two new edges take the place of the removed one. So comparing
 * the current positions of the edges' start nodes gives the same order as the naive scan would.
 */

#pragma once

#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "../../../types.hpp"
#include "../../../utils/neighbour_utils/types.hpp"

struct repair_config {
    double regret_weight = 0.3;
    double cost_weight = 0.7;
};

class Repair {
public:
    /**
     * @brief Greedily inserts nodes into `state` until the cycle has ceil(nodes_num / 2) nodes.
     * Deterministic: always the node with the best score (ties -> smallest node id).
     * @return objective value of the repaired cycle
     */
    static int apply(const algorithm_data& data, local_search_state& state, const repair_config& cfg = {}) {
        return run(data, state, cfg, [](const std::vector<candidate_t>& top) { return top.front().index; }, 1);
    }

    /**
     * @brief Randomised (GRASP-like) version: each step builds a restricted candidate list of the `rcl_size`
     * unselected nodes with the best scores (ties -> smaller node id first), then inserts a uniformly random one
     * of them at its cheapest position. rcl_size = 1 gives exactly apply().
     * @return objective value of the repaired cycle
     */
    static int apply_randomized(const algorithm_data& data, local_search_state& state, const repair_config& cfg,
                                int rcl_size, std::mt19937& rng) {
        return run(data, state, cfg, [&rng](const std::vector<candidate_t>& top) {
            return top[std::uniform_int_distribution<int>(0, static_cast<int>(top.size()) - 1)(rng)].index;
        }, rcl_size);
    }

    /// Objective of a plain cycle: sum of cost_matrix over its edges (cost_matrix[i][j] includes the weight of j).
    static int cycle_cost(const algorithm_data& data, const std::vector<int>& cycle) {
        const int m = static_cast<int>(cycle.size());
        int cost = 0;
        for (int i = 0; i < m; ++i) cost += data.cost_matrix[cycle[i]][cycle[i + 1 == m ? 0 : i + 1]];
        return cost;
    }

private:
    // cheapest and second-cheapest insertion of a node; edge = start node of the edge, -1 = none
    struct insertion_t {
        int best_cost = std::numeric_limits<int>::max();
        int best_edge = -1;
        int second_cost = std::numeric_limits<int>::max();
        int second_edge = -1;
    };

    // an entry of the restricted candidate list: index in `unselected`, its node and score
    struct candidate_t {
        int index;
        int node;
        double score;
    };

    /**
     * Shared body of both repairs. Each step, `top` gets the `rcl_size` best nodes ordered by (score desc, node id
     * asc), and `choose(top)` returns the index (in `unselected`) of the node to insert.
     */
    template <typename Choose>
    static int run(const algorithm_data& data, local_search_state& state, const repair_config& cfg, Choose choose,
                   int rcl_size) {
        std::vector<int> cycle = state.cycle();
        if (cycle.empty()) {
            throw std::invalid_argument("Repair::apply: the partial cycle must contain at least one node");
        }

        const int n = data.nodes_num;
        const int target = (n + 1) / 2;
        const auto& cost = data.cost_matrix;
        std::vector<int>& unselected = state.unselected;
        cycle.reserve(target);

        // next[s]: successor of s in the cycle; pos[s]: index of s in `cycle` (the order used for ties)
        std::vector<int> next(n, -1), pos(n, -1);
        auto link_all = [&]() {
            const int m = static_cast<int>(cycle.size());
            for (int i = 0; i < m; ++i) {
                next[cycle[i]] = cycle[i + 1 == m ? 0 : i + 1];
                pos[cycle[i]] = i;
            }
        };
        link_all();

        // (cost, edge) ordered by cost, then by the position of the edge in the cycle
        auto before = [&](int c1, int e1, int c2, int e2) {
            return c1 < c2 || (c1 == c2 && e2 >= 0 && pos[e1] < pos[e2]);
        };
        auto consider = [&](insertion_t& ins, int delta, int edge) {
            if (before(delta, edge, ins.best_cost, ins.best_edge)) {
                ins.second_cost = ins.best_cost;
                ins.second_edge = ins.best_edge;
                ins.best_cost = delta;
                ins.best_edge = edge;
            } else if (before(delta, edge, ins.second_cost, ins.second_edge)) {
                ins.second_cost = delta;
                ins.second_edge = edge;
            }
        };
        auto delta_of = [&](int j, int s) { return cost[s][j] + cost[j][next[s]] - cost[s][next[s]]; };
        auto recompute = [&](int j, insertion_t& ins) {
            ins = insertion_t{};
            for (const int s : cycle) consider(ins, delta_of(j, s), s);
        };

        // cache[u] belongs to unselected[u]
        std::vector<insertion_t> cache(unselected.size());
        for (std::size_t u = 0; u < unselected.size(); ++u) recompute(unselected[u], cache[u]);

        // (score desc, node id asc): ties -> smallest node id (as in lab 2, independent of the order of
        // `unselected`)
        auto better = [](const candidate_t& x, const candidate_t& y) {
            return x.score > y.score || (x.score == y.score && x.node < y.node);
        };
        rcl_size = std::max(1, rcl_size);
        std::vector<candidate_t> top;
        top.reserve(rcl_size + 1);

        while (static_cast<int>(cycle.size()) < target && !unselected.empty()) {
            // the rcl_size best nodes, kept sorted by insertion (rcl_size is small)
            top.clear();
            for (int u = 0; u < static_cast<int>(unselected.size()); ++u) {
                const insertion_t& ins = cache[u];
                const int regret = ins.second_edge < 0 ? 0 : ins.second_cost - ins.best_cost;
                const candidate_t c{u, unselected[u], cfg.regret_weight * regret - cfg.cost_weight * ins.best_cost};
                if (static_cast<int>(top.size()) == rcl_size && !better(c, top.back())) continue;
                if (static_cast<int>(top.size()) == rcl_size) top.pop_back();
                top.insert(std::upper_bound(top.begin(), top.end(), c, better), c);
            }
            const int best_idx = choose(top);

            // insert v into the edge a -> b
            const int v = unselected[best_idx];
            const int a = cache[best_idx].best_edge;
            const int b = next[a];
            const int at = pos[a] + 1;
            cycle.insert(cycle.begin() + at, v);
            next[a] = v;
            next[v] = b;
            for (int i = at; i < static_cast<int>(cycle.size()); ++i) pos[cycle[i]] = i;

            unselected[best_idx] = unselected.back();
            unselected.pop_back();
            cache[best_idx] = cache.back();
            cache.pop_back();

            // edge a -> b is gone (a now starts a -> v), new edges: a -> v, v -> b
            for (std::size_t u = 0; u < unselected.size(); ++u) {
                insertion_t& ins = cache[u];
                const int j = unselected[u];
                if (ins.best_edge == a || ins.second_edge == a) {
                    recompute(j, ins);
                } else {
                    consider(ins, delta_of(j, a), a);
                    consider(ins, delta_of(j, v), v);
                }
            }
        }

        // rebuild the padded path: path[0] = last node, path[m + 1] = first node
        state.path.clear();
        state.path.reserve(cycle.size() + 2);
        state.path.push_back(cycle.back());
        state.path.insert(state.path.end(), cycle.begin(), cycle.end());
        state.path.push_back(cycle.front());

        return cycle_cost(data, cycle);
    }
};
