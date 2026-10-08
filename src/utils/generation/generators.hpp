/**
 * @brief This file contains various generators for nodes instances, including random generators and grid-based generators.
 */

#pragma once
#include "../../types.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

class InstanceGenerator {
public:
    InstanceGenerator(int max_weight, unsigned seed = std::random_device{}())
        : max_weight(max_weight), rng(seed) {}
    virtual ~InstanceGenerator() = default;

    virtual coordinates_t generate_coordinates(int n) = 0;

    std::vector<int> generate_weights(int n) {
        std::uniform_int_distribution<> dis_weight(0, max_weight);
        std::vector<int> weights;
        weights.reserve(n);
        for (int i = 0; i < n; ++i) {
            weights.push_back(dis_weight(rng));
        }
        return weights;
    }

protected:
    int max_weight;
    std::mt19937 rng;
};

class DenseRandomGenerator : public InstanceGenerator {
public:
    using InstanceGenerator::InstanceGenerator;

    coordinates_t generate_coordinates(int n) override {
        int range = std::max(static_cast<int>(std::sqrt(7 * n)), 4);
        std::uniform_int_distribution<> dis_x(0, range);
        std::uniform_int_distribution<> dis_y(0, range);
        coordinates_t coordinates;
        coordinates.reserve(n);
        for (int i = 0; i < n; ++i) {
            coordinates.emplace_back(dis_x(rng), dis_y(rng));
        }
        return coordinates;
    }
};

class SparseRandomGenerator : public InstanceGenerator {
public:
    using InstanceGenerator::InstanceGenerator;

    coordinates_t generate_coordinates(int n) override {
        int range = std::max(n * n, 10 * n);
        std::uniform_int_distribution<> dis_x(0, range);
        std::uniform_int_distribution<> dis_y(0, range);
        coordinates_t coordinates;
        coordinates.reserve(n);
        for (int i = 0; i < n; ++i) {
            coordinates.emplace_back(dis_x(rng), dis_y(rng));
        }
        return coordinates;
    }
};
