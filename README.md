# EC labs

Following repository contains code and reports for the course "Evolutionary Computation" at PUT.

## Problem

All labs solve the same problem, a variant of the TSP. Each node has a position in the plane and a cost. The task is to select exactly half of the nodes (rounded up) and form a Hamiltonian cycle through them, minimizing the total length of the cycle plus the total cost of the selected nodes. Distances are Euclidean, rounded to integers, and algorithms work only on the distance matrix. See [docs/problem_description.md](docs/problem_description.md) for the full description.

## Labs

| Lab | Topic | Report |
|---|---|---|
| 1 | Greedy heuristics (random, nearest neighbor ×2, greedy cycle) | [reports/lab_1](reports/lab_1/lab_1_report.md) |

## Repo structure

```
├───build                       compiled executables (not tracked)
├───data
│   ├───input                   problem instances (TSPA.csv, TSPB.csv)
│   └───lab_1                   one folder per lab
│       ├───output              best solution per instance and method 
│       ├───plots               visualizations of the best solutions
│       └───results             statistics of the objective function
├───docs                        problem description
├───reports
│   └───lab_1                   report (Markdown) and pseudocodes, one folder per lab
└───src
    ├───types.hpp               shared data types
    ├───algorithm_base.hpp      AlgorithmBase and StartNodeAlgorithm base classes
    ├───lab_1                   one folder per lab
    │   ├───main.cpp            experiment for the lab
    │   ├───requirements.md     lab assignment
    │   └───algorithms          algorithms implemented in the lab
    │        └────<algorithm>.hpp
    └───utils
        ├───checker             solution checker
        ├───generation          generators of synthetic instances (not used now)
        ├───parsing             instance parsing and distance matrix computation
        └───plotting            Python scripts for plotting results
```

The C++ code is header-only: every lab is built from a single `main.cpp`.

## Requirements

- C++17 compiler (e.g. g++ from MinGW-w64 / MSYS2)
- Python 3.12 with [uv](https://docs.astral.sh/uv/), for plotting only (`uv sync` installs matplotlib)

## Usage

All commands are run from the repository root.

Build and run (e.g. lab 1):

```
g++ -std=c++17 -O2 -Wall -Wextra src/lab_1/main.cpp -o build/lab_1.exe
./build/lab_1.exe
```

This runs all methods on all instances, checks every solution with the solution checker, and saves the best solutions to `data/lab_1/output/` and the statistics to `data/lab_1/results/statistics.csv`.

Plot results:

```
uv run src/utils/plotting/visualize_result.py --no-show data/lab_1/output/*.txt
uv run src/utils/plotting/compare_results.py data/lab_1 --no-show --methods random nearest_endpoint nearest_anypoint greedy_cycle
```

The first command saves one plot per output file. The second saves one comparison figure per instance (`comparison_<instance>.png`) with all methods and a common node cost scale. Add `--format pdf` (or `svg`) for vector plots, and drop `--no-show` to also display them. In PowerShell, `*.txt` is not expanded, so list the files explicitly.

## Adding a new lab

1. Create `src/lab_X/` with `main.cpp` and the algorithms in `src/lab_X/algorithms/`.
2. Derive algorithms from `AlgorithmBase` (implement `run(data)`), or from `StartNodeAlgorithm` if they start from a given node (implement `run(data, start_node)` and add `using StartNodeAlgorithm::run;`).
3. Save results to `data/lab_X/output/<instance>_<method>.txt`, so the plotting scripts work without changes.
4. Put the report in `reports/lab_X/`.
