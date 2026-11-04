
#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../utils/neighbour_utils/types.hpp"
#include "../../utils/neighbour_utils/neighbourhood_moves.hpp"
#include "../../utils/neighbour_utils/candidate.hpp"
#include "local_search_base.hpp"

class LocalSearchSteepest : public LocalSearchBase {
public:
    using LocalSearchBase::LocalSearchBase;
    using LocalSearchBase::run;

    algorithm_result run(
        const algorithm_data& data,
        std::string intra_cost_function_name,
        std::string inter_cost_function_name,
        std::string initial_solution_method,
        int initial_solution_start_node
    ) override {

        const algorithm_result initial = initial_solution(
            data,
            initial_solution_method,
            initial_solution_start_node
        );

        return dispatch_intra_move(
            intra_cost_function_name,
            [&](auto intra_move) {
                return dispatch_inter_move(
                    inter_cost_function_name,
                    [&](auto inter_move) {
                        return search<
                            decltype(intra_move),
                            decltype(inter_move)
                        >(data, initial);
                    }
                );
            }
        );
    }

    algorithm_result run_with_candidates(
        const algorithm_data& data,
        std::string intra_cost_function_name,
        std::string inter_cost_function_name,
        std::string initial_solution_method,
        int initial_solution_start_node,
        const CandidateMoveGenerator& candidate_generator
    ) {

        // Candidate moves are defined for 2-opt + node exchange only.
        if (intra_cost_function_name != "intra_edge_exchange" ||
            inter_cost_function_name != "inter_node_exchange") {
            throw std::invalid_argument(
                "Candidate moves support only intra_edge_exchange "
                "with inter_node_exchange.");
        }

        const algorithm_result initial = initial_solution(
            data,
            initial_solution_method,
            initial_solution_start_node
        );

        return search_with_candidates(data, initial, candidate_generator);
    }

private:

    template <typename IntraMove, typename InterMove>
    algorithm_result search(
        const algorithm_data& data,
        const algorithm_result& initial
    ) {

        local_search_state state(initial.path, data.nodes_num);

        int total_weight = initial.total_weight;

        const int path_size = state.size();
        const int unselected_size =
            static_cast<int>(state.unselected.size());

        const int intra_limit = path_size > 2 ? path_size : 0;

        while (true) {
            int best_cost_change = 0;
            int best_first = -1;
            int best_second = -1;
            int exchange_type = -1;

            // Examine every intra-route move.
            for (int i = 1; i <= path_size; ++i) {
                for (int j = i + 1; j <= intra_limit; ++j) {

                    const int cost_change = IntraMove::cost(
                        data.distance_matrix,
                        i,
                        j,
                        state
                    );

                    if (cost_change < best_cost_change) {
                        best_cost_change = cost_change;
                        best_first = i;
                        best_second = j;
                        exchange_type = 0;
                    }
                }
            }

            // Examine every inter-route move.
            for (int i = 1; i <= path_size; ++i) {
                for (int k = 0; k < unselected_size; ++k) {

                    const int cost_change = InterMove::cost(
                        data.cost_matrix,
                        i,
                        k,
                        state
                    );

                    if (cost_change < best_cost_change) {
                        best_cost_change = cost_change;
                        best_first = i;
                        best_second = k;
                        exchange_type = 1;
                    }
                }
            }

            // No improving move remains.
            if (best_cost_change >= 0) {
                break;
            }

            // Apply the best move.
            if (exchange_type == 0) {
                IntraMove::apply(state, best_first, best_second);
            } else {
                InterMove::apply(state, best_first, best_second);
            }

            total_weight += best_cost_change;
        }

        return {state.cycle(), total_weight};
    }

    /**
     * Steepest search over candidate moves only (moves adding >= 1 candidate edge).
     * Moves are evaluated directly while walking the candidate lists, without
     * building a move list first. For a candidate edge (u, v) with u selected:
     *  - v selected:   two 2-opt moves add (u, v): (pos u, pos v) and (pos u-1, pos v-1)
     *  - v unselected: two exchanges put v next to u: v replaces u's successor or predecessor
     */
    algorithm_result search_with_candidates(
        const algorithm_data& data,
        const algorithm_result& initial,
        const CandidateMoveGenerator& candidate_generator
    ) {
        local_search_state state(initial.path, data.nodes_num);
        int total_weight = initial.total_weight;

        const int nodes_num = data.nodes_num;
        const int path_size = state.size();
        const distance_matrix_t& distances = data.distance_matrix;
        const distance_matrix_t& costs = data.cost_matrix;

        // position[node] = position in path (1..m) or -1;
        // unselected_index[node] = index in state.unselected or -1.
        // Built once and then updated only where a move changes something.
        std::vector<int> position(nodes_num, -1);
        std::vector<int> unselected_index(nodes_num, -1);
        for (int i = 1; i <= path_size; ++i) {
            position[state.path[i]] = i;
        }
        for (int i = 0; i < static_cast<int>(state.unselected.size()); ++i) {
            unselected_index[state.unselected[i]] = i;
        }

        auto previous = [path_size](int p) { return p == 1 ? path_size : p - 1; };
        auto next = [path_size](int p) { return p == path_size ? 1 : p + 1; };

        while (true) {
            int best_cost_change = 0;
            int best_first = -1;
            int best_second = -1;
            bool best_is_intra = false;

            // 2-opt removing edges (first, first+1) and (second, second+1).
            auto try_intra = [&](int first, int second) {
                if (first > second) {
                    std::swap(first, second);
                }
                // removed edges must not share a vertex
                if (second == first + 1 || (first == 1 && second == path_size)) {
                    return;
                }
                const int cost_change = intra_edge_exchange_cost(distances, first, second, state);
                if (cost_change < best_cost_change) {
                    best_cost_change = cost_change;
                    best_first = first;
                    best_second = second;
                    best_is_intra = true;
                }
            };

            // Replace node at path position `pos` with unselected[index].
            auto try_inter = [&](int pos, int index) {
                const int cost_change = external_node_exchange_cost(costs, pos, index, state);
                if (cost_change < best_cost_change) {
                    best_cost_change = cost_change;
                    best_first = pos;
                    best_second = index;
                    best_is_intra = false;
                }
            };

            for (int pos_u = 1; pos_u <= path_size; ++pos_u) {
                const int u = state.path[pos_u];

                for (const int v : candidate_generator.get_candidate_neighbours(u)) {
                    const int pos_v = position[v];

                    if (pos_v != -1) {
                        // Lists are symmetric, so handle each selected pair once.
                        if (u < v && path_size >= 4) {
                            try_intra(pos_u, pos_v);
                            try_intra(previous(pos_u), previous(pos_v));
                        }
                    } else {
                        const int index = unselected_index[v];
                        try_inter(next(pos_u), index);
                        try_inter(previous(pos_u), index);
                    }
                }
            }

            // No improving candidate move remains.
            if (best_cost_change >= 0) {
                break;
            }

            if (best_is_intra) {
                intra_edge_swap(state, best_first, best_second);
                // only the reversed segment first+1..second moved
                for (int i = best_first + 1; i <= best_second; ++i) {
                    position[state.path[i]] = i;
                }
            } else {
                const int removed = state.path[best_first];
                const int added = state.unselected[best_second];
                inter_node_swap(state, best_first, best_second);
                position[added] = best_first;
                position[removed] = -1;
                unselected_index[removed] = best_second;
                unselected_index[added] = -1;
            }

            total_weight += best_cost_change;
        }

        return {state.cycle(), total_weight};
    }
};
