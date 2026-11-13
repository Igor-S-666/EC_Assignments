/**
 * @file local_search_candidates.hpp
 * @brief Steepest local search over candidate moves only (moves introducing at least one candidate edge),
 *  an optimized version of lab 4's LocalSearchSteepest::run_with_candidates.
 *
 *  It walks the candidate lists in the same order and breaks ties the same way, so it takes the same moves
 *  and ends in the same local optima; only the evaluation is cheaper: deltas are computed from node ids with
 *  fixed matrix rows, and everything that depends only on the selected node u is computed once per u.
 */

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../../utils/neighbour_utils/types.hpp"
#include "../../utils/neighbour_utils/neighbour_exchanges.hpp"
#include "candidate_lists.hpp"
#include "local_search_base.hpp"

class LocalSearchCandidates : public FixedLocalSearchBase {
public:
    using FixedLocalSearchBase::FixedLocalSearchBase;
    using FixedLocalSearchBase::run;

    // candidate lists of the instance the next runs are on
    void set_candidates(const CandidateLists* candidate_lists) { candidates = candidate_lists; }

    algorithm_result run(const algorithm_data& data,
                         std::string initial_solution_method,
                         int initial_solution_start_node) override {
        if (candidates == nullptr) throw std::logic_error("LocalSearchCandidates: candidate lists are not set");
        const algorithm_result initial = initial_solution(data, initial_solution_method, initial_solution_start_node);
        return search(data, initial);
    }

private:
    const CandidateLists* candidates = nullptr;

    algorithm_result search(const algorithm_data& data, const algorithm_result& initial) {
        const distance_matrix_t& dist = data.distance_matrix;
        const distance_matrix_t& cost = data.cost_matrix;
        local_search_state state(initial.path, data.nodes_num);
        std::vector<int>& path = state.path;
        const int m = state.size();
        int total_weight = initial.total_weight;

        // position[node] = position in path (1..m) or -1, unselected_index[node] = index in state.unselected or -1
        std::vector<int> position(data.nodes_num, -1), unselected_index(data.nodes_num, -1);
        for (int i = 1; i <= m; ++i) position[path[i]] = i;
        for (int k = 0; k < static_cast<int>(state.unselected.size()); ++k) unselected_index[state.unselected[k]] = k;

        auto previous = [m](int p) { return p == 1 ? m : p - 1; };
        auto next = [m](int p) { return p == m ? 1 : p + 1; };

        while (true) {
            int best_cost_change = 0, best_first = -1, best_second = -1;
            bool best_is_intra = false;
            // 2-opt removing the edges at path positions first and second (any order)
            auto take_intra = [&](int cost_change, int first, int second) {
                if (cost_change < best_cost_change) {
                    best_cost_change = cost_change;
                    best_first = std::min(first, second);
                    best_second = std::max(first, second);
                    best_is_intra = true;
                }
            };
            auto take_inter = [&](int cost_change, int pos, int index) {
                if (cost_change < best_cost_change) {
                    best_cost_change = cost_change;
                    best_first = pos;
                    best_second = index;
                    best_is_intra = false;
                }
            };

            for (int pos_u = 1; pos_u <= m; ++pos_u) {
                // u's neighbourhood on the cycle: pu2 -> pu -> u -> nu -> nu2
                const int u = path[pos_u], pu = path[pos_u - 1], nu = path[pos_u + 1];
                const int pos_pu = previous(pos_u), pos_nu = next(pos_u);
                const int pu2 = path[pos_pu - 1], nu2 = path[pos_nu + 1];
                const int* du = dist[u].data();
                const int* cu = cost[u].data();
                // removed parts of an exchange of nu (edges u-nu-nu2) and of pu (edges pu2-pu-u)
                const int removed_next = cu[nu] + cost[nu][nu2];
                const int removed_prev = cost[pu2][pu] + cost[pu][u];

                for (const int* it = candidates->begin(u); it != candidates->end(u); ++it) {
                    const int v = *it, pos_v = position[v];
                    if (pos_v != -1) {
                        // both selected: the two 2-opts adding (u, v); each candidate pair is handled once
                        if (u < v && m >= 4) {
                            const int nv = path[pos_v + 1], pv = path[pos_v - 1];
                            if (nu != v && nv != u) {  // removed edges u->nu and v->nv must not share a node
                                take_intra(du[v] + dist[nu][nv] - du[nu] - dist[v][nv], pos_u, pos_v);
                            }
                            if (pv != u && pu != v) {  // removed edges pu->u and pv->v
                                take_intra(dist[pu][pv] + du[v] - dist[pu][u] - dist[pv][v],
                                           pos_pu, previous(pos_v));
                            }
                        }
                    } else {
                        // v unselected: v replaces u's successor or predecessor, which puts it next to u
                        const int index = unselected_index[v];
                        const int* cv = cost[v].data();
                        take_inter(cu[v] + cv[nu2] - removed_next, pos_nu, index);
                        take_inter(cost[pu2][v] + cv[u] - removed_prev, pos_pu, index);
                    }
                }
            }

            if (best_cost_change >= 0) break;  // no improving candidate move remains

            if (best_is_intra) {
                intra_edge_swap(state, best_first, best_second);
                // only the reversed segment first+1..second moved
                for (int i = best_first + 1; i <= best_second; ++i) position[path[i]] = i;
            } else {
                const int removed = path[best_first], added = state.unselected[best_second];
                inter_node_swap(state, best_first, best_second);
                position[added] = best_first, unselected_index[added] = -1;
                position[removed] = -1, unselected_index[removed] = best_second;
            }
            total_weight += best_cost_change;
        }

        return {state.cycle(), total_weight};
    }
};
