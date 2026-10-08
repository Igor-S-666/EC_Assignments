/**
 * @file greedy_cycle.hpp
 * @brief This file contains the implementation of algorithm for:
 * Greedy cycle, aka each time we add a node, we add it at the position
 * that minimizes the total weight of the cycle
 */

#pragma once
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
#include "../../algorithm_base.hpp"

class GreedyCycleAlgorithm : public StartNodeAlgorithm {
public:
    explicit GreedyCycleAlgorithm(unsigned seed = std::random_device{}()) : StartNodeAlgorithm(seed) {}
    using StartNodeAlgorithm::run;

    algorithm_result run(const algorithm_data& data, int start_node) override {
        if (data.nodes_num <= 0) {
            throw std::invalid_argument("run: data.nodes_num must be greater than 0");
        }

        const int final_nodes_num = (data.nodes_num + 1) / 2;
        std::vector<int> path;
        path.reserve(final_nodes_num);
        path.push_back(start_node);
        std::vector<bool> in_path(data.nodes_num, false);
        in_path[start_node] = true;

        // second node: the nearest one to the start node (distance + node weight)
        if (final_nodes_num > 1) {
            int nearest_node = -1;
            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j] && (nearest_node == -1 || data.cost_matrix[start_node][j] < data.cost_matrix[start_node][nearest_node])) {
                    nearest_node = j;
                }
            }
            path.push_back(nearest_node);
            in_path[nearest_node] = true;
        }

        while (static_cast<int>(path.size()) < final_nodes_num) {
            int nearest_node = -1;
            std::size_t insertion_point = 0;
            int nearest_distance = std::numeric_limits<int>::max();
            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j]) {
                    // try inserting j into every edge (path[k], path[k+1]) of the cycle, including the closing one
                    for (std::size_t k = 0; k < path.size(); k++) {
                        int current_node = path[k];
                        int next_node = path[(k + 1) % path.size()];
                        int distance = (data.cost_matrix[current_node][j]
                                        + data.cost_matrix[j][next_node]
                                        - data.cost_matrix[current_node][next_node]);
                        if (distance < nearest_distance) {
                            nearest_distance = distance;
                            nearest_node = j;
                            insertion_point = k + 1;
                        }
                    }
                }
            }
            if (nearest_node == -1) {
                throw std::runtime_error("run: No nearest node found");
            }
            path.insert(path.begin() + insertion_point, nearest_node);
            in_path[nearest_node] = true;
        }

        return {path, cycle_weight(data, path)};
    }
};
