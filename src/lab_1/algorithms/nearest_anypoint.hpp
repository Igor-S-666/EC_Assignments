/**
 * @file nearest_anypoint.hpp
 * @brief This file contains the implementation of algorithm for:
 * "Nearest neighbor considering adding the node at all possible position,
 * i.e. at the end, at the beginning, or at any place inside the current path"
 */

#pragma once
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
#include "../../algorithm_base.hpp"

class NearestAnypointAlgorithm : public StartNodeAlgorithm {
public:
    explicit NearestAnypointAlgorithm(unsigned seed = std::random_device{}()) : StartNodeAlgorithm(seed) {}
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

        for (int i = 0; i < final_nodes_num-1; i++) {
            int nearest_node = -1;
            std::size_t insertion_point = 0;
            int nearest_distance = std::numeric_limits<int>::max();
            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j]) {
                    for (std::size_t k = 0; k <= path.size(); k++) {
                        int distance;
                        if (k == 0) {
                            // cost_matrix[j][path[0]] would count path[0]'s weight instead of j's
                            distance = data.distance_matrix[j][path[0]] + data.node_weights[j];
                        } else if (k == path.size()) {
                            distance = data.cost_matrix[path.back()][j];
                        } else {
                            distance = (data.cost_matrix[path[k-1]][j]
                                        + data.cost_matrix[j][path[k]]
                                        - data.cost_matrix[path[k-1]][path[k]]);
                        }
                        if (distance < nearest_distance) {
                            nearest_distance = distance;
                            nearest_node = j;
                            insertion_point = k;
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
