/**
 * @brief This file contains base abstract algorithm class
 */

#pragma once
#include <cstddef>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "types.hpp"

class AlgorithmBase {
public:
    virtual ~AlgorithmBase() = default;
    explicit AlgorithmBase(unsigned seed = std::random_device{}()) : seed(seed), gen(seed) {}

    /**
     * @brief Run the algorithm on the given data
     *
     * @param data The input data for the algorithm
     * @return algorithm_result The result of the algorithm
     */
    virtual algorithm_result run(const algorithm_data& data) = 0;

    /**
     * @brief Save the result of the algorithm to a file
     *
     * @param result The result of the algorithm
     * @param filename The name of the file to save the result to
     * @param file_type The type of the file to save the result to (e.g., "txt", "csv")
     */
    void save_result_to_file(const algorithm_result& result,
                             const std::string& filename,
                             const std::string& input_file,
                             const std::string& file_type = "txt") {
        if (file_type != "txt") {
            throw std::invalid_argument("Unsupported file type: " + file_type);
        }
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filename);
        }
        file << "Input file: " << input_file << '\n';
        file << "Path: ";
        for (const auto& node : result.path) {
            file << node << " ";
        }
        file << '\n';
        file << "Total weight: " << result.total_weight << '\n';
    }

protected:
    unsigned seed;
    std::mt19937 gen;

    // objective of the closed cycle: distances between consecutive nodes + node weights
    static int cycle_weight(const algorithm_data& data, const std::vector<int>& path) {
        int total_weight = 0;
        for (std::size_t i = 0; i < path.size(); ++i) {
            total_weight += data.cost_matrix[path[i]][path[(i + 1) % path.size()]];
        }
        return total_weight;
    }
};

// Base for algorithms that build a solution from a given start node.
// Derived classes need `using StartNodeAlgorithm::run;`, otherwise their
// run(data, start_node) hides run(data) (C++ name hiding).
class StartNodeAlgorithm : public AlgorithmBase {
public:
    using AlgorithmBase::AlgorithmBase;

    virtual algorithm_result run(const algorithm_data& data, int start_node) = 0;

    algorithm_result run(const algorithm_data& data) override {
        std::uniform_int_distribution<> dis_node(0, data.nodes_num - 1);
        return run(data, dis_node(gen));
    }
};
