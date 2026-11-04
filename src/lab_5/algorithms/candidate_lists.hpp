/**
 * @file candidate_lists.hpp
 * @brief Candidate edges of an instance (from lab 4's CandidateMoveGenerator) in a flat layout:
 *  neighbour lists stored one after another and an n x n table answering "is (x, y) a candidate edge" in O(1).
 *  Built once per instance and shared by all runs on it.
 */

#pragma once
#include <vector>
#include "../../utils/neighbour_utils/candidate.hpp"

class CandidateLists {
public:
    CandidateLists(const CandidateMoveGenerator& generator, int nodes_num)
        : n(nodes_num), offsets(nodes_num + 1, 0), table(nodes_num * nodes_num, 0) {
        for (int x = 0; x < n; ++x) {
            // lists are symmetric and sorted ascending (the order lab 4's search walks them in)
            for (const int y : generator.get_candidate_neighbours(x)) {
                list.push_back(y);
                table[x * n + y] = 1;
            }
            offsets[x + 1] = static_cast<int>(list.size());
        }
    }

    bool is_candidate(int x, int y) const { return table[x * n + y]; }

    const int* begin(int x) const { return list.data() + offsets[x]; }
    const int* end(int x) const { return list.data() + offsets[x + 1]; }

private:
    int n;
    std::vector<int> offsets, list;  // neighbours of x: list[offsets[x], offsets[x + 1])
    std::vector<char> table;
};
