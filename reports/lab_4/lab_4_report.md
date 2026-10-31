# Evolutionary Computation – Lab 4

**Authors:** Julia Mirońska, Igor Szymczak

## Problem Description

We are given three columns of integers, with one row for each node. The first two columns contain the \(x\) and \(y\) coordinates of the node positions in a plane. The third column contains node costs. The goal is to select exactly 50% of the nodes (rounding up if the number of nodes is odd) and form a Hamiltonian cycle through the selected nodes, minimizing the sum of the cycle length and the costs of the selected nodes.

The objective function is:

\[
f(P)=\sum_{i=0}^{k-1}d(p_i,p_{(i+1)\bmod k})+\sum_{i=0}^{k-1}c_{p_i}
\]

where \(k=\lceil n/2\rceil\), and \(d(i,j)\) is the Euclidean distance rounded to the nearest integer.

Both TSPA and TSPB contain 200 nodes, so each solution contains 100 nodes.

## Algorithms

### Baseline: steepest local search

The baseline algorithm is the steepest local search with two-edge exchange and a random starting solution, as used in Lab 3.

At each iteration, it evaluates the available intra-route and inter-route moves and applies the move with the smallest negative objective delta. The process ends when no improving move remains.

### Candidate-based steepest local search

The purpose of Lab 4 is to reduce the computational cost of steepest local search by restricting the neighbourhood to a subset of promising moves.

Instead of evaluating every possible move, the algorithm uses candidate lists. These lists contain nearby nodes for each node in the instance. Only moves involving pairs of nodes connected by candidate edges are considered.

The candidate list size is \(k=10\). Candidate lists are symmetrized: an edge is included if either endpoint selected the other endpoint as a candidate.

The candidate lists are constructed using the cost matrix:

\[
cost(i,j)=d(i,j)+c_j
\]

Thus, the attractiveness of a candidate depends on both the distance to it and its node cost.

### Pseudocode: candidate list construction

**Input:** Cost matrix \(C\), number of nodes \(n\), candidate list size \(K\).

**Output:** Symmetric candidate adjacency lists.

```text
PROCEDURE BuildCandidateLists(C, n, K)

    candidates[0 ... n-1] ← empty lists

    FOR each node i:
        neighbours ← all nodes j where j ≠ i

        SORT neighbours by increasing C[i][j]

        FOR each of the first min(K, n-1) neighbours j:
            ADD j to candidates[i]

    FOR each node i:
        FOR each node j in candidates[i]:
            ADD i to candidates[j]

    FOR each node i:
        SORT candidates[i]
        REMOVE duplicate entries

    RETURN candidates
```

The symmetrization step ensures that a candidate edge is available from both endpoints, even if it was initially selected by only one of them.

### Pseudocode: candidate move generation (`candidate.hpp`)

The generator uses the current solution to distinguish selected and unselected nodes. It then examines candidate edges and creates two types of moves:

- **Intra-route moves:** two-edge exchanges between selected nodes.
- **Inter-route moves:** replacement of a selected node with an unselected candidate node.

```text
PROCEDURE GenerateCandidateMoves(state, candidates)

    moves ← empty list

    position[node] ← -1 for every node
    unselected_index[node] ← -1 for every node

    FOR each selected node at position p:
        position[node] ← p

    FOR each unselected node at index u:
        unselected_index[node] ← u

    FOR each selected node i:
        FOR each candidate node j in candidates[i]:

            IF j is selected:
                IF edge (i, j) defines a valid two-edge exchange:
                    ADD intra-route move (i, j) to moves

            ELSE:
                ADD inter-route replacement move (i, j) to moves

    RETURN moves
```

The generator uses lookup arrays to determine whether a node is selected and to retrieve its position or unselected-node index efficiently. This avoids repeatedly searching through the current solution.

The generated moves are evaluated using the same delta calculations and move-application procedures as in the baseline algorithm.

### Pseudocode: candidate-based part of steepest local search

Only the candidate-based modification is shown below. The remaining steepest-search procedure is unchanged: the best improving move is selected and applied.

```text
PROCEDURE CandidateSteepestSearch(state, current_value)

    REPEAT:
        moves ← GenerateCandidateMoves(state, candidates)

        best_delta ← 0
        best_move ← NONE

        FOR each move in moves:
            delta ← CalculateDelta(move, state)

            IF delta < best_delta:
                best_delta ← delta
                best_move ← move

        IF best_move is NONE:
            BREAK

        ApplyMove(best_move, state)
        current_value ← current_value + best_delta

    RETURN state
```

The main difference from the baseline is the generation of a restricted neighbourhood. The candidate-based algorithm evaluates fewer moves, which reduces the time spent searching for an improvement. However, because some moves are omitted, it may reach a different local optimum.

## Computational Experiment

The Lab 4 results are compared with all methods from Lab 3. The Lab 3 methods include constructive heuristics, 2-regret heuristics, greedy local search and steepest local search.

The Lab 4 baseline is the steepest local search with two-edge exchange and a random starting solution. The candidate-based version uses the same starting-solution and search strategy, but restricts the neighbourhood using candidate lists.

### Objective function values

The following table contains the average, minimum and maximum objective values reported in Lab 3 and Lab 4.

| Method | TSPA avg | TSPA min | TSPA max | TSPB avg | TSPB min | TSPB max |
|---|---:|---:|---:|---:|---:|---:|
| Random | 263132.65 | 232767 | 290020 | 206682.99 | 185482 | 232014 |
| Nearest neighbor (end) | 87476.63 | 84362 | 90075 | 59397.33 | 56007 | 63166 |
| Nearest neighbor (any position) | 74346.96 | 72328 | 77022 | 59475.50 | 52915 | 66164 |
| Greedy cycle | 73797.39 | 72590 | 75281 | 59709.47 | 53016 | 63736 |
| 2-regret nearest neighbor | 122860.66 | 111527 | 133181 | 74763.39 | 69032 | 80767 |
| 2-regret greedy cycle | 121007.17 | 112654 | 130924 | 73403.35 | 69455 | 78142 |
| Weighted 2-regret nearest neighbor | 75561.20 | 72419 | 78504 | 55698.34 | 50485 | 61406 |
| Weighted 2-regret greedy cycle | 73348.72 | 72011 | 75532 | 57895.54 | 52731 | 63370 |
| Greedy LS, nodes, random start | 87875.52 | 80852 | 96854 | 63592.77 | 58487 | 70640 |
| Greedy LS, nodes, greedy cycle start | 73478.34 | 72415 | 74736 | 59331.08 | 52707 | 63607 |
| Greedy LS, edges, random start | 76342.58 | 74146 | 78532 | 52869.81 | 50667 | 55260 |
| Greedy LS, edges, greedy cycle start | 73164.54 | 72088 | 74565 | 58728.54 | 51715 | 63178 |
| Steepest LS, nodes, random start | 90293.09 | 83000 | 98690 | 65919.13 | 59827 | 74954 |
| Steepest LS, nodes, greedy cycle start | 73468.17 | 72415 | 74736 | 59322.68 | 52588 | 63607 |
| **Steepest LS, edges, random start (Lab 4 baseline)** | **76191.55** | **73440** | **79726** | **52575.82** | **50091** | **54983** |
| Steepest LS, edges, greedy cycle start | 73167.85 | 72088 | 74544 | 58736.80 | 52523 | 63178 |
| **Steepest LS, edges, random start, candidates** | **76235.09** | **73758** | **79511** | **52553.58** | **50382** | **55096** |

The Lab 4 candidate-based method has a slightly higher average objective value on TSPA than the baseline, by 43.54 (approximately 0.057%). On TSPB, its average is lower by 22.24 (approximately 0.042%).

The best individual baseline solutions are better on both instances. The candidate method nevertheless achieves a lower maximum value on TSPA, while its maximum on TSPB is slightly higher.

### Running time

The following table compares the average running times of all methods. Times are in microseconds (\(\mu s\)).

For Lab 1 and Lab 2 methods, the previous report recorded only average times. Minimum and maximum values are therefore unavailable. The Lab 3 local-search methods and Lab 4 methods include minimum and maximum times.

| Method | TSPA avg | TSPA min | TSPA max | TSPB avg | TSPB min | TSPB max |
|---|---:|---:|---:|---:|---:|---:|
| Random | 1 | -- | -- | 0 | -- | -- |
| Nearest neighbor (end) | 36 | -- | -- | 25 | -- | -- |
| Nearest neighbor (any position) | 612 | -- | -- | 576 | -- | -- |
| Greedy cycle | 525 | -- | -- | 520 | -- | -- |
| 2-regret nearest neighbor | 1034 | -- | -- | 1113 | -- | -- |
| 2-regret greedy cycle | 873 | -- | -- | 890 | -- | -- |
| Weighted 2-regret nearest neighbor | 940 | -- | -- | 960 | -- | -- |
| Weighted 2-regret greedy cycle | 917 | -- | -- | 913 | -- | -- |
| Greedy LS, nodes, random start | 725 | 469 | 1528 | 704 | 390 | 1337 |
| Greedy LS, nodes, greedy cycle start | 799 | 670 | 1549 | 826 | 659 | 1391 |
| Greedy LS, edges, random start | 467 | 344 | 813 | 478 | 342 | 1011 |
| Greedy LS, edges, greedy cycle start | 814 | 684 | 1101 | 904 | 741 | 1555 |
| Steepest LS, nodes, random start | 2769 | 2099 | 3842 | 2836 | 2139 | 6516 |
| Steepest LS, nodes, greedy cycle start | 591 | 512 | 996 | 648 | 523 | 1153 |
| **Steepest LS, edges, random start (Lab 4 baseline)** | **149031** | **118209** | **278849** | **154787** | **124338** | **294255** |
| Steepest LS, edges, greedy cycle start | 574 | 498 | 1119 | 647 | 562 | 1043 |
| **Steepest LS, edges, random start, candidates** | **53326** | **41562** | **92493** | **48372** | **35444** | **146692** |

The candidate-based version is substantially faster than the Lab 4 baseline:

- **TSPA:** approximately 2.80 times faster, reducing average runtime by 64.2%.
- **TSPB:** approximately 3.20 times faster, reducing average runtime by 68.7%.


## Best Solutions

### TSPA

| Method | Best objective value |
|---|---:|
| Steepest LS, edges, random start (Lab 4 baseline) | 73440 |
| Steepest LS, edges, random start, candidates | 73758 |

**Baseline solution:**

```text
168 63 149 60 31 48 80 152 108 49 160 83 123 100 90 132 76 91 157 71
51 16 67 144 140 53 179 52 177 99 131 36 143 33 9 54 8 85 93 50
3 169 1 164 186 136 184 178 29 101 147 37 47 130 159 170 153 193 78 88
107 57 182 138 151 42 89 113 106 150 174 176 72 134 84 58 158 192 181 114
135 73 45 185 35 180 28 87 141 189 2 171 187 30 161 97 127 69 104 129
```

Total weight: **73440**

**Candidate-based solution:**

```text
113 89 42 150 151 57 64 107 88 78 193 153 170 146 159 130 47 147 101 29
178 184 136 164 9 54 33 131 99 36 177 179 52 67 144 53 140 41 135 73
45 185 102 28 180 156 35 16 51 139 112 4 76 132 71 157 91 80 48 7
31 108 60 116 149 63 168 49 152 123 83 160 97 95 127 69 171 2 189 187
30 141 87 98 114 192 158 58 84 134 72 176 174 85 1 169 3 50 93 106
```

Total weight: **73758**

### TSPB

| Method | Best objective value |
|---|---:|
| Steepest LS, edges, random start (Lab 4 baseline) | 50091 |
| Steepest LS, edges, random start, candidates | 50382 |

**Baseline solution:**

```text
76 162 84 66 67 31 181 152 34 24 27 3 184 46 72 120 156 89 48 96
198 121 42 147 116 37 146 13 149 28 145 65 158 26 125 177 170 115 180 6
29 132 197 175 134 45 157 60 40 71 155 97 154 140 47 38 21 176 30 183
133 136 172 160 69 151 98 167 73 59 85 51 82 87 179 99 171 74 20 2
14 153 90 129 127 80 192 54 78 10 148 114 196 105 94 81 70 190 57 185
```

Total weight: **50091**

**Candidate-based solution:**

```text
78 127 129 90 118 110 2 14 20 74 153 182 80 192 54 162 66 67 31 11
24 34 152 181 179 87 82 51 85 59 73 167 151 122 173 184 3 135 76 46
72 117 120 133 136 172 160 69 183 30 21 38 47 140 154 97 155 71 40 60
157 45 134 175 36 197 132 29 6 180 115 170 26 125 177 138 158 145 28 149
116 147 121 198 58 96 48 89 57 190 70 187 81 94 105 61 196 114 148 10
```

Total weight: **50382**

## Solution Checker

Every generated solution was verified using the solution checker: `src/utils/checker/solution_checker.hpp`.

The checker recalculates the objective function using the distance matrix and node costs. It also verifies that each solution contains exactly \(\lceil n/2\rceil\) nodes, that all node indices are valid, and that no node occurs more than once. The objective value calculated by the checker is compared with the value reported by the algorithm.

All generated solutions (200 per instance and method) passed these checks. The best solutions can additionally be verified using the checker provided on ekursy.

## Conclusions

- **Candidate lists substantially reduce runtime.** Compared with the full-neighbourhood Lab 4 baseline, the candidate method is approximately 2.80 times faster on TSPA and 3.20 times faster on TSPB.

- **The objective quality remains close to the baseline.** The average objective changes by only +0.057% on TSPA and -0.042% on TSPB. This indicates that restricting the neighbourhood can preserve solution quality while reducing computation time.

- **The best individual baseline solutions remain better.** The baseline obtains 73440 on TSPA and 50091 on TSPB, while the candidate method obtains 73758 and 50382, respectively.

- **The candidate method does not necessarily find the same local optimum.** Since it evaluates only a subset of possible moves, it can stop even when an improving move exists outside the candidate neighbourhood.

- **The candidate approach offers a useful speed-quality trade-off.** Its main advantage is reduced runtime, while the difference in average objective values is very small.

## Source Code

The implementation is available in the repository:

[EC_Assignments – Lab 4](https://github.com/Igor-S-666/EC_Assignments/tree/main/src/lab_4)

The candidate-list construction and candidate move generation are implemented in `candidate.hpp`. The steepest local search implementation uses the generated candidate moves to restrict the neighbourhood evaluated at each iteration.
