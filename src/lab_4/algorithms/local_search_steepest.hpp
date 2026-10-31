
#pragma once

#include <string>
#include <utility>

#include "../types.hpp"
#include "../neighbour_utils/neighbourhood_moves.hpp"
#include "../neighbour_utils/candidate.hpp"
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
                        return search_with_candidates<
                            decltype(intra_move),
                            decltype(inter_move)
                        >(
                            data,
                            initial,
                            candidate_generator
                        );
                    }
                );
            }
        );
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

    template <typename IntraMove, typename InterMove>
    algorithm_result search_with_candidates(
        const algorithm_data& data,
        const algorithm_result& initial,
        const CandidateMoveGenerator& candidate_generator
    ) {

        local_search_state state(initial.path, data.nodes_num);

        int total_weight = initial.total_weight;

        // Reuse this vector in every iteration.
        neighbour_orders_t moves;
        moves.reserve(candidate_generator.estimated_max_moves());

        while (true) {
            int best_cost_change = 0;
            int best_first = -1;
            int best_second = -1;
            int exchange_type = -1;

            // Generate eligible candidate moves into the reusable vector.
            candidate_generator.generate_neighbour_order(state, moves);

            // Evaluate every eligible move.
            for (const auto& move : moves) {

                int cost_change;

                if (move[2] == 0) {
                    cost_change = IntraMove::cost(
                        data.distance_matrix,
                        move[0],
                        move[1],
                        state
                    );
                } else {
                    cost_change = InterMove::cost(
                        data.cost_matrix,
                        move[0],
                        move[1],
                        state
                    );
                }

                if (cost_change < best_cost_change) {
                    best_cost_change = cost_change;
                    best_first = move[0];
                    best_second = move[1];
                    exchange_type = move[2];
                }
            }

            // No improving candidate move remains.
            if (best_cost_change >= 0) {
                break;
            }

            // Apply the best candidate move.
            if (exchange_type == 0) {
                IntraMove::apply(state, best_first, best_second);
            } else {
                InterMove::apply(state, best_first, best_second);
            }

            total_weight += best_cost_change;
        }

        return {state.cycle(), total_weight};
    }
};
