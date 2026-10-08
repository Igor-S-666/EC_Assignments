/**
 * @brief This files contain structures and types used in the project.
 */

#pragma once
#include <functional>
#include <utility>
#include <vector>

using point_t = std::pair<int, int>;
using coordinates_t = std::vector<point_t>;
using distance_matrix_t = std::vector<std::vector<int>>;

/// Distance between two points, e.g. euclidean_distance (see dist_preprocessing.hpp).
using distance_function = std::function<int(const point_t&, const point_t&)>;

/// Turns node coordinates into a distance matrix (see dist_preprocessing.hpp).
using preprocessing_function = std::function<distance_matrix_t(const coordinates_t&)>;

struct algorithm_data {
    distance_matrix_t distance_matrix;
    // cost_matrix[i][j] = distance_matrix[i][j] + node_weights[j], so a solution's
    // objective is just the sum of cost_matrix over the cycle's edges
    distance_matrix_t cost_matrix;
    std::vector<int> node_weights;
    int nodes_num = 0;
};

struct algorithm_result {
    std::vector<int> path;
    int total_weight = 0;
};
