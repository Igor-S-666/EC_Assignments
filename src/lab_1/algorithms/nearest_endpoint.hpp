/**
 * @file nearest_endpoint.hpp
 * @brief This file contains the implementation of algorithm for:
 * "Nearest neighbor considering adding the node only at the end of the current path"
*/

#pragma once
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
#include "../../algorithm_base.hpp"

class NearestEndpointAlgorithm : public StartNodeAlgorithm {
public:
    explicit NearestEndpointAlgorithm(unsigned seed = std::random_device{}()) : StartNodeAlgorithm(seed) {}
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
            int last_node = path.back();
            int nearest_node = -1;
            int nearest_distance = std::numeric_limits<int>::max();
            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j]) {
                    int distance = data.cost_matrix[last_node][j];
                    if (distance < nearest_distance) {
                        nearest_distance = distance;
                        nearest_node = j;
                    }
                }
            }
            if (nearest_node == -1) {
                throw std::runtime_error("run: No nearest node found");
            }
            path.push_back(nearest_node);
            in_path[nearest_node] = true;
        }

        return {path, cycle_weight(data, path)};
    }
};
