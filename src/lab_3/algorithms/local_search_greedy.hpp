#pragma once

#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
#include <array>

#include "types.hpp"
#include "../neighbor_utils/neighbor_cost_calculator.hpp"
#include "../neighbor_utils/neighbor_order_generator.hpp"
#include "../local_search_base.hpp"
#include "../neighbor_utils/neighbour_exchanges.hpp"
#include "../../lab_1/algorithms/greedy_cycle.hpp"
#include "../../lab_1/algorithms/random.hpp"
class LocalSearchGreedy : public LocalSearchBase {
public:
    using LocalSearchBase::AlgorithmBase;

    algorithm_result run(const algorithm_data& data, 
                         std::string intra_cost_function_name,
                         std::string inter_cost_function_name,
                         std::string initial_solution_method,
                         int initial_solution_start_node) override {
        // Implement the greedy local search algorithm here
        // This is a placeholder implementation
        std::vector<int> path(data.nodes_num);
        if (initial_solution_method == "greedy_cycle") {
            path = greedy_cycle(data.distance_matrix, initial_solution_start_node);
        } else if (initial_solution_method == "random") {
            path = random_path(data.nodes_num, gen);
        } else {
            throw std::invalid_argument("Invalid initial solution method: " + initial_solution_method);
        }

        int total_weight = cycle_weight(data, path);
        cost_function cost_func_intra = get_cost_function(cost_function_name);
        cost_function cost_func_inter = get_cost_function(inter_cost_function_name);
        exchange_function exchange_func_intra = get_exchange_function(intra_cost_function_name);
        exchange_function exchange_func_inter = get_exchange_function(inter_cost_function_name);

        bool improved = true;
        int first_node, second_node, exchange_type;
        neighbor_orders_t neighbour_orders = NeighbourOrderGenerator(gen(), data.nodes_num).generate_neighbour_order();
        while (improved) {
            improved = false;
            for (std::array<int, 3> order : neighbour_orders) {
                first_node = order[0];
                second_node = order[1];
                exchange_type = order[2];
                int cost_change = exchange_type == 0 
                                    ? cost_func_intra(data.cost_matrix, first_node, second_node, path) 
                                    :cost_func_inter(data.cost_matrix, first_node, second_node, path);
                    if (cost_change < 0) {
                        if (exchange_type == 0) {
                            exchange_func_intra(path, first_node, second_node);
                        } else {
                            exchange_func_inter(path, first_node, second_node);
                        }
                        total_weight += cost_change;
                        improved = true;
                        break; // Exit the loop to restart the search
                    }
            }
        }

        return {path, total_weight};
        
        
    }
};