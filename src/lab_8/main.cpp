/**
 * @file main.cpp
 * @brief Lab 8: global convexity (fitness-similarity correlation) tests.
 *
 * For each instance:
 *  - generates 1000 local optima: greedy local search (two-edges exchange, Lab 3) from random solutions,
 *  - for every local optimum calculates its similarity (common edges, common selected nodes) to
 *      1. all other local optima (average),
 *      2. the best of the 1000 local optima (the best one itself is excluded),
 *      3. a very good solution of the best method so far (the best ILS solution from Lab 6),
 *  - calculates the Pearson correlation between the objective and each of the 6 similarities.
 *
 * Run from the repository root. Results:
 *  - data/lab_8/results/<instance>_local_optima.csv   objective and the 6 similarities of each local optimum
 *  - data/lab_8/results/correlations.csv              the 12 correlation coefficients
 *  - data/lab_8/results/statistics.csv                objective of the local optima (min / max / average)
 *  - data/lab_8/output/<instance>_best_local_optimum.txt
 */

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../lab_3/algorithms/local_search_greedy.hpp"
#include "../utils/checker/solution_checker.hpp"
#include "../utils/parsing/data_parser.hpp"

namespace fs = std::filesystem;

constexpr int RUNS = 1000;
constexpr unsigned SEED = 42;
const std::string RESULTS_DIR = "data/lab_8/results";
const std::string OUTPUT_DIR = "data/lab_8/output";

struct Instance {
    std::string name;
    // best solution of the best method so far (ILS, Lab 6)
    std::string reference_method;
    std::string reference_file;
};

/**
 * @brief Neighbours of the selected nodes in a cycle, for O(1) membership tests of nodes and edges.
 * succ[v] / pred[v] are -1 for unselected nodes.
 */
struct CycleIndex {
    std::vector<int> path;
    std::vector<int> succ;
    std::vector<int> pred;

    CycleIndex(const std::vector<int>& cycle, int nodes_num) : path(cycle), succ(nodes_num, -1), pred(nodes_num, -1) {
        const std::size_t k = cycle.size();
        for (std::size_t i = 0; i < k; ++i) {
            succ[cycle[i]] = cycle[(i + 1) % k];
            pred[cycle[(i + 1) % k]] = cycle[i];
        }
    }

    bool selected(int v) const { return succ[v] != -1; }
    // edges are undirected
    bool has_edge(int a, int b) const { return succ[a] == b || pred[a] == b; }
};

int common_nodes(const CycleIndex& p, const CycleIndex& q) {
    int count = 0;
    for (int v : p.path) {
        count += q.selected(v);
    }
    return count;
}

int common_edges(const CycleIndex& p, const CycleIndex& q) {
    int count = 0;
    for (int v : p.path) {
        count += q.has_edge(v, p.succ[v]);  // each edge of the cycle P exactly once, the closing one included
    }
    return count;
}

double pearson(const std::vector<double>& x, const std::vector<double>& y) {
    const double n = static_cast<double>(x.size());
    const double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
    const double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        sxy += (x[i] - mean_x) * (y[i] - mean_y);
        sxx += (x[i] - mean_x) * (x[i] - mean_x);
        syy += (y[i] - mean_y) * (y[i] - mean_y);
    }
    if (sxx == 0.0 || syy == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return sxy / std::sqrt(sxx * syy);
}

/**
 * @brief Reads a solution saved by an earlier lab ("Path: ..." and "Total weight: ..." lines, 0-based nodes)
 * and checks that it is valid and that its objective is the saved one.
 */
algorithm_result read_reference(const algorithm_data& data, const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file) {
        throw std::runtime_error("cannot open " + file_name);
    }
    algorithm_result result;
    int saved_weight = -1;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("Path:", 0) == 0) {
            std::istringstream nodes(line.substr(5));
            int v;
            while (nodes >> v) {
                result.path.push_back(v);
            }
        } else if (line.rfind("Total weight:", 0) == 0) {
            saved_weight = std::stoi(line.substr(13));
        }
    }
    result.total_weight = saved_weight;
    const std::string error = check_solution(data, result);
    if (!error.empty()) {
        throw std::runtime_error("invalid reference solution " + file_name + ": " + error);
    }
    return result;
}

void run_instance(const Instance& instance, LocalSearchGreedy& local_search, std::ofstream& correlations,
                  std::ofstream& statistics) {
    const algorithm_data data = parse_csv("data/input/" + instance.name + ".csv");
    const algorithm_result reference = read_reference(data, instance.reference_file);
    std::cout << instance.name << ": reference " << instance.reference_method << ", objective "
              << reference.total_weight << '\n';

    // 1000 local optima: greedy local search with the two-edges exchange from random solutions
    std::vector<algorithm_result> optima;
    optima.reserve(RUNS);
    for (int run = 0; run < RUNS; ++run) {
        algorithm_result result = local_search.run(data, "intra_edge_exchange", "inter_node_exchange", "random", 0);
        const std::string error = check_solution(data, result);
        if (!error.empty()) {
            throw std::runtime_error(instance.name + ", run " + std::to_string(run) + ": " + error);
        }
        optima.push_back(std::move(result));
    }

    std::vector<CycleIndex> index;
    index.reserve(RUNS);
    for (const auto& optimum : optima) {
        index.emplace_back(optimum.path, data.nodes_num);
    }
    const CycleIndex reference_index(reference.path, data.nodes_num);

    // the first local optimum with the lowest objective
    int best = 0;
    for (int i = 1; i < RUNS; ++i) {
        if (optima[i].total_weight < optima[best].total_weight) {
            best = i;
        }
    }

    // average similarity to all other local optima (a solution is not compared with itself)
    std::vector<double> sum_edges(RUNS, 0.0), sum_nodes(RUNS, 0.0);
    for (int i = 0; i < RUNS; ++i) {
        for (int j = i + 1; j < RUNS; ++j) {
            const int edges = common_edges(index[i], index[j]);
            const int nodes = common_nodes(index[i], index[j]);
            sum_edges[i] += edges;
            sum_edges[j] += edges;
            sum_nodes[i] += nodes;
            sum_nodes[j] += nodes;
        }
    }

    std::ofstream csv(RESULTS_DIR + "/" + instance.name + "_local_optima.csv");
    csv << "objective;avg_edges;avg_nodes;best1000_edges;best1000_nodes;reference_edges;reference_nodes\n";
    csv << std::fixed << std::setprecision(4);

    // x / y values of the 6 charts: average, best of 1000 (without the best itself), reference
    std::vector<double> objective, objective_without_best;
    std::vector<double> avg_edges, avg_nodes, best_edges, best_nodes, ref_edges, ref_nodes;
    for (int i = 0; i < RUNS; ++i) {
        const double f = optima[i].total_weight;
        objective.push_back(f);
        avg_edges.push_back(sum_edges[i] / (RUNS - 1));
        avg_nodes.push_back(sum_nodes[i] / (RUNS - 1));
        ref_edges.push_back(common_edges(index[i], reference_index));
        ref_nodes.push_back(common_nodes(index[i], reference_index));

        csv << optima[i].total_weight << ';' << avg_edges.back() << ';' << avg_nodes.back() << ';';
        if (i == best) {
            csv << ';';  // excluded: it would have 100% similarity to itself
        } else {
            objective_without_best.push_back(f);
            best_edges.push_back(common_edges(index[i], index[best]));
            best_nodes.push_back(common_nodes(index[i], index[best]));
            csv << static_cast<int>(best_edges.back()) << ';' << static_cast<int>(best_nodes.back());
        }
        csv << ';' << static_cast<int>(ref_edges.back()) << ';' << static_cast<int>(ref_nodes.back()) << '\n';
    }

    auto write = [&](const std::string& version, const std::string& measure, const std::vector<double>& x,
                     const std::vector<double>& y) {
        const double r = pearson(x, y);
        correlations << instance.name << ';' << version << ';' << measure << ';' << x.size() << ';'
                     << std::fixed << std::setprecision(4) << r << '\n';
        std::cout << "  " << std::left << std::setw(10) << version << std::setw(7) << measure << " r = "
                  << std::fixed << std::setprecision(4) << r << '\n';
    };
    write("average", "edges", objective, avg_edges);
    write("average", "nodes", objective, avg_nodes);
    write("best1000", "edges", objective_without_best, best_edges);
    write("best1000", "nodes", objective_without_best, best_nodes);
    write("reference", "edges", objective, ref_edges);
    write("reference", "nodes", objective, ref_nodes);

    const auto [min_it, max_it] = std::minmax_element(objective.begin(), objective.end());
    const double average = std::accumulate(objective.begin(), objective.end(), 0.0) / RUNS;
    statistics << instance.name << ';' << static_cast<int>(*min_it) << ';' << static_cast<int>(*max_it) << ';'
               << std::fixed << std::setprecision(2) << average << ';' << reference.total_weight << ';'
               << instance.reference_method << '\n';

    std::ofstream best_file(OUTPUT_DIR + "/" + instance.name + "_best_local_optimum.txt");
    best_file << "Input file: data/input/" << instance.name << ".csv\nPath: ";
    for (int v : optima[best].path) {
        best_file << v << ' ';
    }
    best_file << "\nTotal weight: " << optima[best].total_weight << '\n';
}

int main() {
    try {
        fs::create_directories(RESULTS_DIR);
        fs::create_directories(OUTPUT_DIR);

        const std::vector<Instance> instances = {
            {"TSPA", "ILS_double_bridge_strength_4", "data/lab_6/output/TSPA_ILS_double_bridge_strength_4.txt"},
            {"TSPB", "ILS_segment_relocation_strength_4",
             "data/lab_6/output/TSPB_ILS_segment_relocation_strength_4.txt"},
        };

        std::ofstream correlations(RESULTS_DIR + "/correlations.csv");
        correlations << "instance;version;measure;sample_size;pearson_r\n";
        std::ofstream statistics(RESULTS_DIR + "/statistics.csv");
        statistics << "instance;min;max;average;reference;reference_method\n";

        LocalSearchGreedy local_search(SEED);
        for (const Instance& instance : instances) {
            run_instance(instance, local_search, correlations, statistics);
        }
        std::cout << "Results saved in " << RESULTS_DIR << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
