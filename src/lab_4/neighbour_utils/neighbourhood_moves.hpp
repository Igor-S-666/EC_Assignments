/** @file neighbourhood_moves.hpp
 *  @brief Compile-time bundles of (cost, exchange) functions and string -> move dispatch.
 *  @details
 *  Generally fancy trick to avoid virtual function calls and std::function overhead,
 *  while still allowing runtime selection of the neighbourhood move type.
 */

#pragma once
#include <stdexcept>
#include <string>
#include "../types.hpp"
#include "neighbor_cost_calculator.hpp"
#include "neighbour_exchanges.hpp"

template <auto Cost, auto Exchange>
struct neighbourhood_move {
    static int cost(const distance_matrix_t& cost_matrix, int first, int second, const local_search_state& state) {
        return Cost(cost_matrix, first, second, state);
    }
    static void apply(local_search_state& state, int first, int second) {
        Exchange(state, first, second);
    }
};

using intra_node_move = neighbourhood_move<intra_node_exchange_cost, intra_node_swap>;
using intra_edge_move = neighbourhood_move<intra_edge_exchange_cost, intra_edge_swap>;
using inter_node_move = neighbourhood_move<external_node_exchange_cost, inter_node_swap>;

/// Calls f(intra_move{}) with the intra move matching `name`, returns its result.
template <typename F>
decltype(auto) dispatch_intra_move(const std::string& name, F&& f) {
    if (name == "intra_node_exchange") {
        return f(intra_node_move{});
    } else if (name == "intra_edge_exchange") {
        return f(intra_edge_move{});
    }
    throw std::invalid_argument("Invalid intra neighbourhood: " + name);
}

/// Calls f(inter_move{}) with the inter move matching `name`, returns its result.
template <typename F>
decltype(auto) dispatch_inter_move(const std::string& name, F&& f) {
    if (name == "inter_node_exchange") {
        return f(inter_node_move{});
    }
    throw std::invalid_argument("Invalid inter neighbourhood: " + name);
}
