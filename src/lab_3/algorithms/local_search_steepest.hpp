#pragma once

#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <array>

#include "../types.hpp"
#include "../neighbour_utils/neighbourhood_moves.hpp"
#include "local_search_base.hpp"
class LocalSearchSteepest : public LocalSearchBase {
public:
    using LocalSearchBase::LocalSearchBase;
    using LocalSearchBase::run;

    algorithm_result run(const algorithm_data& data,
                         std::string intra_cost_function_name,
                         std::string inter_cost_function_name,
                         std::string initial_solution_method,
                         int initial_solution_start_node) override {
        const algorithm_result initial = initial_solution(data, initial_solution_method, initial_solution_start_node);

        // fancy way of iterating over all possible combinations of intra and inter moves, 
        // without virtual calls or std::function overhead
        return dispatch_intra_move(intra_cost_function_name, [&](auto intra_move) {
            return dispatch_inter_move(inter_cost_function_name, [&](auto inter_move) {
                return search<decltype(intra_move), decltype(inter_move)>(data, initial);
            });
        });
    }

private:
    template <typename IntraMove, typename InterMove>
    algorithm_result search(const algorithm_data& data, const algorithm_result& initial) {
        local_search_state state(initial.path, data.nodes_num);
        int total_weight = initial.total_weight;
        const int path_size = state.size();
        const int unselected_size = static_cast<int>(state.unselected.size());
        // a cycle of <= 2 nodes has a single arrangement (see NeighbourOrderGenerator)
        const int intra_limit = path_size > 2 ? path_size : 0;

        bool improved = true;
        int best_cost_change = 0;
        // path positions (padded, 1..path_size) / index into unselected - what apply() expects
        int best_first = -1, best_second = -1, exchange_type = -1;
        while (improved) {
            improved = false;
            for (int i = 1; i <= path_size; ++i) {
                for (int j = i + 1; j <= intra_limit; ++j) {
                    // intra moves keep the node set, so node weights cancel out -> plain distances
                    int cost_change = IntraMove::cost(data.distance_matrix, i, j, state);
                    if (cost_change < best_cost_change) {
                        best_cost_change = cost_change;
                        best_first = i;
                        best_second = j;
                        exchange_type = 0;
                    }
                }
            }
            for (int i = 1; i <= path_size; ++i) {
                for (int k = 0; k < unselected_size; ++k) {
                    int cost_change = InterMove::cost(data.cost_matrix, i, k, state);
                    if (cost_change < best_cost_change) {
                        best_cost_change = cost_change;
                        best_first = i;
                        best_second = k;
                        exchange_type = 1;
                    }
                }
            }
            if (best_cost_change < 0) {
                if (exchange_type == 0) {
                    IntraMove::apply(state, best_first, best_second);
                } else {
                    InterMove::apply(state, best_first, best_second);
                }
                total_weight += best_cost_change;
                improved = true;
                best_cost_change = 0;
                best_first = -1;
                best_second = -1;
                exchange_type = -1;
            }
        }

        return {state.cycle(), total_weight};
    }
};
