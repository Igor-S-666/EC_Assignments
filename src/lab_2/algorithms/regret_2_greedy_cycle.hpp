
/**
 * @file regret_2_greedy_cycle.hpp
 * @brief 2-regret greedy cycle with a weighted sum criterion:
 * 2-regret + best change of the objective function.
 * Default weights are equal.
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

class WeightedRegret2GreedyCycleAlgorithm : public StartNodeAlgorithm {
private:
    double regret_weight;
    double cost_weight;

public:
    explicit WeightedRegret2GreedyCycleAlgorithm(
        unsigned seed = std::random_device{}(),
        double regret_weight = 0.5,
        double cost_weight = 0.5
    )
        : StartNodeAlgorithm(seed),
          regret_weight(regret_weight),
          cost_weight(cost_weight) {}

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

        // Second node: the nearest one to the start node
        if (final_nodes_num > 1) {
            int nearest_node = -1;
            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j] &&
                    (nearest_node == -1 ||
                     data.cost_matrix[start_node][j] <
                     data.cost_matrix[start_node][nearest_node])) {
                    nearest_node = j;
                }
            }
            path.push_back(nearest_node);
            in_path[nearest_node] = true;
        }

        while (static_cast<int>(path.size()) < final_nodes_num) {
            int best_node = -1;
            std::size_t insertion_point = 0;
            double best_score = -std::numeric_limits<double>::infinity();
            int n = static_cast<int>(path.size());

            for (int j = 0; j < data.nodes_num; j++) {
                if (!in_path[j]) {
                    const std::vector<int>& cost_matrix = data.cost_matrix[j];
                    // Track the cheapest and second-cheapest insertion over every cycle edge
                    int best_cost = std::numeric_limits<int>::max();
                    int second_cost = std::numeric_limits<int>::max();
                    std::size_t best_position = 0;
                    for (int k = 0; k < n; k++) {
                        int current_node = path[k];
                        int next_node = path[k + 1 == n ? 0 : k + 1];
                        int distance = data.cost_matrix[current_node][j]
                                     + cost_matrix[next_node]
                                     - data.cost_matrix[current_node][next_node];

                        if (distance < best_cost) {
                            second_cost = best_cost;
                            best_cost = distance;
                            best_position = k + 1;
                        } else if (distance < second_cost) {
                            second_cost = distance;
                        }
                    }

                    // 2-regret: second-best cost minus best cost
                    int regret = second_cost == std::numeric_limits<int>::max()
                               ? 0
                               : second_cost - best_cost;

                    // Weighted sum: maximize regret, minimize insertion cost
                    double score =
                        regret_weight * regret
                        - cost_weight * best_cost;

                    if (best_node == -1 || score > best_score) {
                        best_score = score;
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
