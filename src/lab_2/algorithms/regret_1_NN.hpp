
/**
 * @file regret_1_NN.hpp
 * @brief This file contains the implementation of the algorithm:
 * 2-regret nearest neighbor considering insertion at any position
 * in the current path. Each time, choose the node with the greatest
 * 2-regret and insert it at its best position.
 */

#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
#include "../../algorithm_base.hpp"

class Regret2NNAlgorithm : public StartNodeAlgorithm {
public:
    explicit Regret2NNAlgorithm(unsigned seed = std::random_device{}()) : StartNodeAlgorithm(seed) {}
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
        int distance;
        for (int i = 0; i < final_nodes_num - 1; i++) {
            int best_node = -1;
            int insertion_point = 0;
            int greatest_regret = std::numeric_limits<int>::min();
            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j]) {
                    // Track the cheapest and second-cheapest insertion over every position
                    int best_cost = std::numeric_limits<int>::max();
                    int second_cost = std::numeric_limits<int>::max();
                    int best_position = 0;
                    for (int k = 0; k <= path.size(); k++) {
                        if (k == 0) {
                            // Insert at the beginning
                            distance = data.distance_matrix[j][path[0]] + data.node_weights[j];
                        } else if (k == path.size()) {
                            // Insert at the end
                            distance = data.cost_matrix[path.back()][j];
                        } else {
                            // Insert inside the path
                            distance = data.cost_matrix[path[k - 1]][j]
                                     + data.cost_matrix[j][path[k]]
                                     - data.cost_matrix[path[k - 1]][path[k]];
                        }

                        if (distance < best_cost) {
                            second_cost = best_cost;
                            best_cost = distance;
                            best_position = k;
                        } else if (distance < second_cost) {
                            second_cost = distance;
                        }
                    }

                    // 2-regret: difference between the second-best and best options
                    int regret = second_cost == std::numeric_limits<int>::max()
                               ? 0
                               : second_cost - best_cost;

                    if (best_node == -1 || regret > greatest_regret) {
                        greatest_regret = regret;
                        best_node = j;
                        insertion_point = best_position;
                    }
                }
            }

            if (best_node == -1) {
                throw std::runtime_error("run: No nearest node found");
            }

            path.insert(path.begin() + insertion_point, best_node);
            in_path[best_node] = true;
        }

        return {path, cycle_weight(data, path)};
    }
};
