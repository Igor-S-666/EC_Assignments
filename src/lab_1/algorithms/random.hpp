/**
 * @brief This file contains algorithm generating random solutions for the problem.
 */

#pragma once
#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>
#include "../../algorithm_base.hpp"

class RandomAlgorithm : public AlgorithmBase {
public:
    explicit RandomAlgorithm(unsigned seed = std::random_device{}()) : AlgorithmBase(seed) {}

    algorithm_result run(const algorithm_data& data) override {
        if (data.nodes_num <= 0) {
            throw std::invalid_argument("run: data.nodes_num must be greater than 0");
        }

        std::vector<int> path(data.nodes_num);
        std::iota(path.begin(), path.end(), 0);
        std::shuffle(path.begin(), path.end(), gen);

        const int final_nodes_num = (data.nodes_num + 1) / 2;
        path.resize(final_nodes_num);

        return {path, cycle_weight(data, path)};
    }
};
