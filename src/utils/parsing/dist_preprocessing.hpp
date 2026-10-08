/**
 * @brief This file contains utility functions for getting distance matrices from nodes coordinates.
 *
 * Usage: parse_csv(file, make_preprocessor(euclidean_distance));
 * Any callable `int(const point_t&, const point_t&)` (e.g. a lambda) can be used as a distance.
 */

#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include "../../types.hpp"

/// Euclidean distance rounded to the nearest integer.
inline int euclidean_distance(const point_t& a, const point_t& b) {
    const double dx = a.first - b.first;
    const double dy = a.second - b.second;
    return static_cast<int>(std::lround(std::sqrt(dx * dx + dy * dy)));
}

inline int manhattan_distance(const point_t& a, const point_t& b) {
    return std::abs(a.first - b.first) + std::abs(a.second - b.second);
}

inline int chebyshev_distance(const point_t& a, const point_t& b) {
    return std::max(std::abs(a.first - b.first), std::abs(a.second - b.second));
}

/// Builds the full n x n distance matrix using the given distance function.
inline distance_matrix_t compute_distance_matrix(const coordinates_t& coordinates,
                                                 const distance_function& distance) {
    const std::size_t n = coordinates.size();
    distance_matrix_t distance_matrix(n, std::vector<int>(n, 0));

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i != j) {
                distance_matrix[i][j] = distance(coordinates[i], coordinates[j]);
            }
        }
    }

    return distance_matrix;
}

/// Binds a distance function, giving a preprocessing_function that parse_csv accepts.
inline preprocessing_function make_preprocessor(distance_function distance) {
    return [distance = std::move(distance)](const coordinates_t& coordinates) {
        return compute_distance_matrix(coordinates, distance);
    };
}
