#pragma once

#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <array>

#include "../types.hpp"
#include "../neighbour_utils/neighbour_order_generator.hpp"
#include "../neighbour_utils/neighbourhood_moves.hpp"
#include "local_search_base.hpp"
class LocalSearchGreedy : public LocalSearchBase {
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

        const neighbour_orders_t neighbour_orders = NeighbourOrderGenerator(
            gen(), state.size(), static_cast<int>(state.unselected.size())
        ).generate_neighbour_order();
        const int orders_num = static_cast<int>(neighbour_orders.size());
        if (orders_num == 0) {
            return {state.cycle(), total_weight};
        }
        std::uniform_int_distribution<int> start_dist(0, orders_num - 1);

        bool improved = true;
        int first_node, second_node, exchange_type;
        while (improved) {
            improved = false;
            // each pass starts at a random point of the (cyclic) order, so moves at
            // the front aren't favoured after every restart
            int index = start_dist(gen);
            for (int k = 0; k < orders_num; ++k, ++index) {
                if (index == orders_num) {
                    index = 0;
                }
                const neighbour_order_t& order = neighbour_orders[index];
                first_node = order[0];
                second_node = order[1];
                exchange_type = order[2];
                // intra moves keep the node set, so node weights cancel out -> plain distances
                int cost_change = exchange_type == 0
                                    ? IntraMove::cost(data.distance_matrix, first_node, second_node, state)
                                    : InterMove::cost(data.cost_matrix, first_node, second_node, state);
                if (cost_change < 0) {
                    if (exchange_type == 0) {
                        IntraMove::apply(state, first_node, second_node);
                    } else {
                        InterMove::apply(state, first_node, second_node);
                    }
                    total_weight += cost_change;
                    improved = true;
                    break; // Exit the loop to restart the search
                }
            }
        }

        return {state.cycle(), total_weight};
    }
};
