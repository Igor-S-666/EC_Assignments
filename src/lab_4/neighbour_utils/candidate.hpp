
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../types.hpp"

class CandidateMoveGenerator {
private:
    int nodes_num;
    int k;

    // Symmetric candidate adjacency lists.
    std::vector<std::vector<int>> candidate_neighbours;

    // Reused scratch buffers. They are allocated only once.
    mutable std::vector<int> position;
    mutable std::vector<int> unselected_index;

    // Number of undirected candidate edges.
    std::size_t candidate_edge_count = 0;

    void build_candidate_list(const algorithm_data& data) {
        candidate_neighbours.assign(nodes_num, {});

        std::vector<std::pair<int, int>> neighbours;
        neighbours.reserve(nodes_num > 0 ? nodes_num - 1 : 0);

        for (int i = 0; i < nodes_num; ++i) {
            neighbours.clear();

            for (int j = 0; j < nodes_num; ++j) {
                if (i == j) {
                    continue;
                }

                // distance(i,j) + cost of destination vertex j
                neighbours.emplace_back(data.cost_matrix[i][j], j);
            }

            const int limit = std::min(
                k,
                static_cast<int>(neighbours.size())
            );

            if (limit == 0) {
                continue;
            }

            if (limit < static_cast<int>(neighbours.size())) {
                std::nth_element(
                    neighbours.begin(),
                    neighbours.begin() + limit,
                    neighbours.end()
                );
            }

            std::sort(
                neighbours.begin(),
                neighbours.begin() + limit
            );

            // Treat candidate connections as undirected:
            // if either endpoint selects the other, the edge is a candidate.
            for (int j = 0; j < limit; ++j) {
                const int v = neighbours[j].second;

                candidate_neighbours[i].push_back(v);
                candidate_neighbours[v].push_back(i);
            }
        }

        candidate_edge_count = 0;

        for (auto& list : candidate_neighbours) {
            std::sort(list.begin(), list.end());

            list.erase(
                std::unique(list.begin(), list.end()),
                list.end()
            );

            candidate_edge_count += list.size();
        }

        candidate_edge_count /= 2;
    }

    static inline int previous_position(int position, int path_size) {
        return position == 1 ? path_size : position - 1;
    }

    static inline int next_position(int position, int path_size) {
        return position == path_size ? 1 : position + 1;
    }

    static inline void add_intra_move(
        int first,
        int second,
        int path_size,
        neighbour_orders_t& moves
    ) {
        if (first == second) {
            return;
        }

        if (first > second) {
            std::swap(first, second);
        }

        // The removed edges must not share a vertex.
        if (second == first + 1) {
            return;
        }

        // The first and last cycle edges share a vertex.
        if (first == 1 && second == path_size) {
            return;
        }

        moves.push_back({first, second, 0});
    }

public:
    explicit CandidateMoveGenerator(
        const algorithm_data& data,
        int candidate_list_size = 10
    )
        : nodes_num(data.nodes_num),
          k(candidate_list_size),
          position(nodes_num, -1),
          unselected_index(nodes_num, -1) {

        if (nodes_num < 0) {
            throw std::invalid_argument("Invalid node count.");
        }

        if (k <= 0) {
            throw std::invalid_argument(
                "Candidate list size must be positive."
            );
        }

        if (static_cast<int>(data.cost_matrix.size()) != nodes_num) {
            throw std::invalid_argument("Invalid cost matrix dimensions.");
        }

        for (const auto& row : data.cost_matrix) {
            if (static_cast<int>(row.size()) != nodes_num) {
                throw std::invalid_argument("Invalid cost matrix dimensions.");
            }
        }

        build_candidate_list(data);
    }

    bool is_candidate_edge(int first, int second) const {
        if (first < 0 || first >= nodes_num ||
            second < 0 || second >= nodes_num) {
            return false;
        }

        const auto& neighbours = candidate_neighbours[first];

        return std::binary_search(
            neighbours.begin(),
            neighbours.end(),
            second
        );
    }

    const std::vector<int>& get_candidate_neighbours(int vertex) const {
        return candidate_neighbours.at(vertex);
    }

    std::size_t get_candidate_edge_count() const {
        return candidate_edge_count;
    }

    // Upper bound on generated moves: each candidate edge yields two moves
    // (both endpoints selected -> 2-opt, one selected -> exchange) or none.
    std::size_t estimated_max_moves() const {
        return candidate_edge_count * 2;
    }

    // Explicit list of all candidate moves for the current state.
    // Not used by the search itself (it evaluates moves on the fly, which is
    // faster); kept as a reference for testing the candidate neighbourhood.
    void generate_neighbour_order(
        const local_search_state& state,
        neighbour_orders_t& moves
    ) const {

        moves.clear();

        const int path_size = state.size();

        if (path_size == 0) {
            return;
        }

        // Update reusable lookup arrays.
        std::fill(position.begin(), position.end(), -1);
        std::fill(unselected_index.begin(), unselected_index.end(), -1);

        for (int i = 1; i <= path_size; ++i) {
            position[state.path[i]] = i;
        }

        for (int i = 0;
             i < static_cast<int>(state.unselected.size());
             ++i) {
            unselected_index[state.unselected[i]] = i;
        }

        // At most two intra and two inter moves per candidate edge.
        moves.reserve(std::max(
            moves.capacity(),
            estimated_max_moves()
        ));

        if (path_size >= 4) {
            for (int u = 0; u < nodes_num; ++u) {
                const int pos_u = position[u];

                if (pos_u == -1) {
                    continue;
                }

                for (const int v : candidate_neighbours[u]) {
                    // Process each undirected candidate edge once.
                    if (u >= v) {
                        continue;
                    }

                    const int pos_v = position[v];

                    if (pos_v == -1) {
                        continue;
                    }

                    // Candidate edge (u,v) is the first introduced edge.
                    add_intra_move(
                        pos_u,
                        pos_v,
                        path_size,
                        moves
                    );

                    // Candidate edge (u,v) is the second introduced edge.
                    add_intra_move(
                        previous_position(pos_u, path_size),
                        previous_position(pos_v, path_size),
                        path_size,
                        moves
                    );
                }
            }
        }


        for (int selected = 0; selected < nodes_num; ++selected) {
            const int selected_position = position[selected];

            if (selected_position == -1) {
                continue;
            }

            const int successor_position =
                next_position(selected_position, path_size);

            const int predecessor_position =
                previous_position(selected_position, path_size);

            for (const int unselected : candidate_neighbours[selected]) {
                const int replacement_index =
                    unselected_index[unselected];

                if (replacement_index == -1) {
                    continue;
                }

                // Replace the successor of selected.
                // Introduces the edge (selected, unselected).
                moves.push_back({
                    successor_position,
                    replacement_index,
                    1
                });

                // Replace the predecessor of selected.
                // Also introduces the edge (unselected, selected).
                moves.push_back({
                    predecessor_position,
                    replacement_index,
                    1
                });
            }
        }
    }

    // Convenience overload for code that expects a returned vector.
    neighbour_orders_t generate_neighbour_order(
        const local_search_state& state
    ) const {
        neighbour_orders_t moves;
        generate_neighbour_order(state, moves);
        return moves;
    }
};
