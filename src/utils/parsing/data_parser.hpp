/**
 * This file contains utility functions for parsing data in various formats.
 */

#pragma once
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../types.hpp"
#include "dist_preprocessing.hpp"

// shared by parse_csv and generate_instance, so all derived fields are filled in one place
inline algorithm_data make_algorithm_data(const coordinates_t& coordinates,
                                          std::vector<int> node_weights,
                                          const preprocessing_function& preprocess) {
    if (!preprocess) {
        throw std::invalid_argument("make_algorithm_data: preprocessing function is empty");
    }

    algorithm_data data;
    data.distance_matrix = preprocess(coordinates);
    data.cost_matrix = data.distance_matrix;
    for (auto& row : data.cost_matrix) {
        for (std::size_t j = 0; j < row.size(); ++j) {
            row[j] += node_weights[j];
        }
    }
    data.node_weights = std::move(node_weights);
    data.nodes_num = static_cast<int>(coordinates.size());
    return data;
}

/**
 * Parses a file with one node per line in the form `x;y;weight`
 * and builds the distance matrix with `preprocess`.
 */
inline algorithm_data parse_csv(const std::string& filename,
                                const preprocessing_function& preprocess = make_preprocessor(euclidean_distance),
                                char delimiter = ';') {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("parse_csv: cannot open file '" + filename + "'");
    }

    coordinates_t coordinates;
    std::vector<int> node_weights;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // Windows line endings
        }
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string cell;
        std::vector<int> values;
        while (std::getline(ss, cell, delimiter)) {
            values.push_back(std::stoi(cell));
        }

        if (values.size() != 3) {
            throw std::runtime_error("parse_csv: expected 3 columns (x;y;weight), got " +
                                     std::to_string(values.size()) + " in line: " + line);
        }

        coordinates.emplace_back(values[0], values[1]);
        node_weights.push_back(values[2]);
    }

    return make_algorithm_data(coordinates, std::move(node_weights), preprocess);
}
