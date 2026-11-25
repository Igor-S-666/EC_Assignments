
/**
 * @file main.cpp
 * @brief Global convexity experiment.
 *
 * Generates 1000 random local optima for TSPA and TSPB,
 * calculates similarity measures and Pearson correlations,
 * and exports all plotting data to CSV files.
 */

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "algorithms/local_search_greedy.hpp"
#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"

namespace fs = std::filesystem;

constexpr int RUNS = 1000;
constexpr unsigned SEED = 42;

struct LocalOptimum {
    algorithm_result solution;

    double average_edges = 0.0;
    double average_nodes = 0.0;

    double best1000_edges = 0.0;
    double best1000_nodes = 0.0;

    double best_method_edges = 0.0;
    double best_method_nodes = 0.0;

    bool has_best1000_similarity = true;
};

using Edge = std::pair<int, int>;
using EdgeSet = std::set<Edge>;

struct Similarity {
    int common_edges = 0;
    int common_nodes = 0;
};

int calculate_objective(
    const algorithm_data& data,
    const std::vector<int>& path
) {
    int total = 0;

    for (std::size_t i = 0; i < path.size(); ++i) {
        const int current = path[i];
        const int next = path[(i + 1) % path.size()];

        total += data.node_weights[current];
        total += data.distance_matrix[current][next];
    }

    return total;
}

// Edges are undirected.
// The closing edge of the cycle is included.

EdgeSet get_edges(const std::vector<int>& path) {
    EdgeSet edges;

    for (std::size_t i = 0; i < path.size(); ++i) {
        int a = path[i];
        int b = path[(i + 1) % path.size()];

        if (a > b) {
            std::swap(a, b);
        }

        edges.insert({a, b});
    }

    return edges;
}

Similarity calculate_similarity(
    const std::vector<int>& first,
    const std::vector<int>& second
) {
    Similarity result;

    // Common selected nodes.
    const std::set<int> first_nodes(
        first.begin(),
        first.end()
    );

    for (int node : second) {
        if (first_nodes.count(node)) {
            ++result.common_nodes;
        }
    }

    // Common undirected edges.
    const EdgeSet first_edges = get_edges(first);
    const EdgeSet second_edges = get_edges(second);

    for (const Edge& edge : first_edges) {
        if (second_edges.count(edge)) {
            ++result.common_edges;
        }
    }

    return result;
}

// Input reference paths are 1-based.
// Internal algorithm representation is 0-based.

algorithm_result make_reference_solution(
    const algorithm_data& data,
    const std::vector<int>& one_based_path,
    int expected_weight,
    const std::string& instance
) {
    algorithm_result reference;

    reference.path.reserve(one_based_path.size());

    for (int node : one_based_path) {
        reference.path.push_back(node - 1);
    }

    reference.total_weight =
        calculate_objective(data, reference.path);

    const std::string error =
        check_solution(data, reference);

    if (!error.empty()) {
        throw std::runtime_error(
            "Invalid reference solution for " +
            instance + ": " + error
        );
    }

    if (reference.total_weight != expected_weight) {
        std::cerr
            << "WARNING: Reference objective for "
            << instance << " is "
            << reference.total_weight
            << ", supplied value was "
            << expected_weight
            << ". Using calculated value.\n";
    }

    return reference;
}

double pearson_correlation(
    const std::vector<double>& x,
    const std::vector<double>& y
) {
    if (x.size() != y.size() || x.size() < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double mean_x =
        std::accumulate(x.begin(), x.end(), 0.0) / x.size();

    const double mean_y =
        std::accumulate(y.begin(), y.end(), 0.0) / y.size();

    double numerator = 0.0;
    double sum_x = 0.0;
    double sum_y = 0.0;

    for (std::size_t i = 0; i < x.size(); ++i) {
        const double dx = x[i] - mean_x;
        const double dy = y[i] - mean_y;

        numerator += dx * dy;
        sum_x += dx * dx;
        sum_y += dy * dy;
    }

    const double denominator =
        std::sqrt(sum_x * sum_y);

    if (denominator == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return numerator / denominator;
}

void calculate_all_similarities(
    std::vector<LocalOptimum>& optima,
    const algorithm_result& best_method,
    int best_index
) {
    const int n = static_cast<int>(optima.size());

    std::vector<double> edge_sums(n, 0.0);
    std::vector<double> node_sums(n, 0.0);

    // Precalculate edges for all solutions.
    std::vector<EdgeSet> all_edges;
    all_edges.reserve(n);

    for (const auto& optimum : optima) {
        all_edges.push_back(
            get_edges(optimum.solution.path)
        );
    }

    // Compare every pair of distinct local optima.
    for (int i = 0; i < n; ++i) {
        const auto& first_path =
            optima[i].solution.path;

        for (int j = i + 1; j < n; ++j) {
            const auto& second_path =
                optima[j].solution.path;

            // Common nodes.
            std::set<int> first_nodes(
                first_path.begin(),
                first_path.end()
            );

            int common_nodes = 0;

            for (int node : second_path) {
                if (first_nodes.count(node)) {
                    ++common_nodes;
                }
            }

            // Common edges.
            int common_edges = 0;

            for (const Edge& edge : all_edges[i]) {
                if (all_edges[j].count(edge)) {
                    ++common_edges;
                }
            }

            edge_sums[i] += common_edges;
            edge_sums[j] += common_edges;

            node_sums[i] += common_nodes;
            node_sums[j] += common_nodes;
        }
    }

    // Average similarity to the other 999 solutions.
    for (int i = 0; i < n; ++i) {
        optima[i].average_edges =
            edge_sums[i] / static_cast<double>(n - 1);

        optima[i].average_nodes =
            node_sums[i] / static_cast<double>(n - 1);
    }

    // Similarity to the best local optimum.
    // Exclude the best solution itself.
    for (int i = 0; i < n; ++i) {
        if (i == best_index) {
            optima[i].has_best1000_similarity = false;
            continue;
        }

        const Similarity similarity =
            calculate_similarity(
                optima[i].solution.path,
                optima[best_index].solution.path
            );

        optima[i].best1000_edges =
            similarity.common_edges;

        optima[i].best1000_nodes =
            similarity.common_nodes;
    }

    // Similarity to the best-so-far reference solution.
    for (int i = 0; i < n; ++i) {
        const Similarity similarity =
            calculate_similarity(
                optima[i].solution.path,
                best_method.path
            );

        optima[i].best_method_edges =
            similarity.common_edges;

        optima[i].best_method_nodes =
            similarity.common_nodes;
    }
}


void save_local_optima_csv(
    const std::vector<LocalOptimum>& optima,
    const std::string& filename
) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error(
            "Cannot open " + filename
        );
    }

    file << std::fixed << std::setprecision(6);

    file
        << "objective;"
        << "average_common_edges;"
        << "average_common_nodes;"
        << "best1000_common_edges;"
        << "best1000_common_nodes;"
        << "best_method_common_edges;"
        << "best_method_common_nodes\n";

    for (const auto& optimum : optima) {
        file
            << optimum.solution.total_weight << ';'
            << optimum.average_edges << ';'
            << optimum.average_nodes << ';';

        if (optimum.has_best1000_similarity) {
            file
                << optimum.best1000_edges << ';'
                << optimum.best1000_nodes;
        } else {
            // Empty values for the best solution itself.
            file << ';';
        }

        file
            << ';'
            << optimum.best_method_edges << ';'
            << optimum.best_method_nodes
            << '\n';
    }
}

void save_correlations(
    const std::vector<LocalOptimum>& optima,
    const std::string& instance,
    std::ofstream& file
) {
    std::vector<double> objectives;

    std::vector<double> average_edges;
    std::vector<double> average_nodes;

    std::vector<double> best_objectives;
    std::vector<double> best_edges;
    std::vector<double> best_nodes;

    std::vector<double> method_edges;
    std::vector<double> method_nodes;

    for (const auto& optimum : optima) {
        const double objective =
            optimum.solution.total_weight;

        objectives.push_back(objective);

        average_edges.push_back(
            optimum.average_edges
        );

        average_nodes.push_back(
            optimum.average_nodes
        );

        method_edges.push_back(
            optimum.best_method_edges
        );

        method_nodes.push_back(
            optimum.best_method_nodes
        );

        if (optimum.has_best1000_similarity) {
            best_objectives.push_back(objective);

            best_edges.push_back(
                optimum.best1000_edges
            );

            best_nodes.push_back(
                optimum.best1000_nodes
            );
        }
    }

    auto write_correlation =
        [&](const std::string& version,
            const std::string& measure,
            const std::vector<double>& x,
            const std::vector<double>& y) {

            const double correlation =
                pearson_correlation(x, y);

            file
                << instance << ';'
                << version << ';'
                << measure << ';'
                << x.size() << ';'
                << std::setprecision(8)
                << correlation << '\n';

            std::cout
                << instance << " | "
                << version << " | "
                << measure << " | r = "
                << correlation << '\n';
        };

    write_correlation(
        "average_similarity",
        "common_edges",
        objectives,
        average_edges
    );

    write_correlation(
        "average_similarity",
        "common_nodes",
        objectives,
        average_nodes
    );

    write_correlation(
        "best_1000",
        "common_edges",
        best_objectives,
        best_edges
    );

    write_correlation(
        "best_1000",
        "common_nodes",
        best_objectives,
        best_nodes
    );

    write_correlation(
        "best_method",
        "common_edges",
        objectives,
        method_edges
    );

    write_correlation(
        "best_method",
        "common_nodes",
        objectives,
        method_nodes
    );
}

void run_experiment(
    const std::string& instance,
    const std::vector<int>& reference_path,
    int reference_weight,
    LocalSearchGreedy& algorithm,
    std::ofstream& correlations
) {
    std::cout
        << "Instance: " << instance << '\n';

    const std::string input_file =
        "data/input/" + instance + ".csv";

    const algorithm_data data =
        parse_csv(input_file);

    std::cout << "Nodes: "
              << data.nodes_num << '\n';

    std::cout
        << "Generating " << RUNS
        << " random local optima...\n";

    std::vector<LocalOptimum> optima;
    optima.reserve(RUNS);

    // Generate 1000 random local optima.
    for (int i = 0; i < RUNS; ++i) {
        algorithm_result result =
            algorithm.run(
                data,
                "intra_node_exchange",
                "inter_node_exchange",
                "random",
                0
            );

        const std::string error =
            check_solution(data, result);

        if (!error.empty()) {
            throw std::runtime_error(
                "Invalid local optimum for " +
                instance + ", run " +
                std::to_string(i) + ": " + error
            );
        }

        optima.push_back({result});

        if ((i + 1) % 100 == 0) {
            std::cout
                << "Generated "
                << i + 1 << "/" << RUNS
                << '\n';
        }
    }

    // Find the best local optimum.
    int best_index = 0;

    for (int i = 1; i < RUNS; ++i) {
        if (
            optima[i].solution.total_weight <
            optima[best_index].solution.total_weight
        ) {
            best_index = i;
        }
    }

    std::cout
        << "\nBest local optimum: "
        << optima[best_index].solution.total_weight
        << " (run " << best_index + 1 << ")\n";

    // Create reference solution.
    const algorithm_result best_method =
        make_reference_solution(
            data,
            reference_path,
            reference_weight,
            instance
        );

    std::cout
        << "Best-so-far reference objective: "
        << best_method.total_weight << '\n';

    // Calculate all similarity measures.
    std::cout << "Calculating similarities...\n";

    calculate_all_similarities(
        optima,
        best_method,
        best_index
    );

    // Save local optima data.
    const std::string csv_filename =
        "data/global_convexity/" +
        instance + "_local_optima.csv";

    save_local_optima_csv(
        optima,
        csv_filename
    );

    std::cout
        << "Saved: " << csv_filename << '\n';

    // Calculate and save six correlations.
    save_correlations(
        optima,
        instance,
        correlations
    );
}

int main() {
    try {
        const std::string output_dir =
            "data/global_convexity";

        fs::create_directories(output_dir);

        // TSPA reference solution (1-based).
        const std::vector<int> tspa_reference = {
            71,157,91,80,48,31,60,116,149,63,
            168,49,108,152,100,123,83,160,97,127,
            69,129,104,92,171,2,189,187,30,141,
            87,102,185,45,73,135,114,181,192,158,
            58,84,134,72,176,174,150,106,113,89,
            42,151,182,57,64,107,88,78,193,153,
            170,146,159,130,47,93,50,3,169,1,
            101,29,178,184,136,186,164,142,8,54,
            9,33,131,36,177,52,179,53,140,144,
            67,16,51,35,156,28,180,132,76,4
        };

        // TSPB reference solution (1-based).
        const std::vector<int> tspb_reference = {
            28,149,13,146,37,116,147,42,121,198,
            96,48,156,89,57,190,70,187,81,94,
            105,61,196,114,10,78,54,162,192,80,
            127,129,90,153,14,2,20,74,17,171,
            99,88,179,87,82,51,85,59,73,167,
            181,152,34,24,67,66,76,27,151,3,
            184,46,72,117,8,120,133,136,172,160,
            69,183,30,21,38,47,140,154,26,125,
            177,170,115,155,71,40,60,157,45,134,
            175,36,197,6,29,132,103,178,65,145
        };

        // Open correlations CSV.
        std::ofstream correlations(
            output_dir + "/correlations.csv"
        );

        if (!correlations.is_open()) {
            throw std::runtime_error(
                "Cannot create correlations.csv"
            );
        }

        correlations
            << "instance;"
            << "similarity_type;"
            << "measure;"
            << "sample_size;"
            << "pearson_r\n";

        std::cout
            << std::fixed
            << std::setprecision(4);

        // Greedy local search with reproducible seed.
        LocalSearchGreedy algorithm(SEED);

        // Run both instances.
        run_experiment(
            "TSPA",
            tspa_reference,
            71579,
            algorithm,
            correlations
        );

        run_experiment(
            "TSPB",
            tspb_reference,
            48185,
            algorithm,
            correlations
        );

        correlations.close();

        std::cout
            << "EXPERIMENT COMPLETED\n"
            << "Results saved in "
            << output_dir << "/\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr
            << "ERROR: " << e.what() << '\n';

        return 1;
    }
}
