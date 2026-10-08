/**
 * @brief This file contains utilities to generate synthetic instances of the problem.
 *
 */

#pragma once
#include "../../types.hpp"
#include "../parsing/data_parser.hpp"
#include "../parsing/dist_preprocessing.hpp"
#include "generators.hpp"
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

inline void save_instance_to_file(const coordinates_t& coordinates,
                                  const std::vector<int>& node_weights,
                                  const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        file << coordinates[i].first << ';' << coordinates[i].second << ';' << node_weights[i] << '\n';
    }
}

// If filename is given, the instance is also saved in the same format as data/input/TSPA.csv
inline algorithm_data generate_instance(InstanceGenerator& generator, int n,
                                        const std::string& filename = "",
                                        const preprocessing_function& preprocess = make_preprocessor(euclidean_distance)) {
    coordinates_t coordinates = generator.generate_coordinates(n);
    std::vector<int> node_weights = generator.generate_weights(n);

    if (!filename.empty()) {
        save_instance_to_file(coordinates, node_weights, filename);
    }

    return make_algorithm_data(coordinates, std::move(node_weights), preprocess);
}
