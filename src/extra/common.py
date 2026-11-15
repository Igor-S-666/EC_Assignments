"""
Shared helpers for the exact-solver experiments in src/extra/<solver>/.
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load_instance(instance: str):
    """Returns (n, k, dist, cost); dist[i][j] matches the C++ preprocessing (std::lround of euclidean)."""
    nodes = [tuple(map(int, line.split(';'))) for line in
             (ROOT / 'data' / 'input' / f'{instance}.csv').read_text().split()]
    n = len(nodes)
    k = (n + 1) // 2
    cost = [c for _, _, c in nodes]
    dist = [[math.floor(math.hypot(xi - xj, yi - yj) + 0.5) for xj, yj, _ in nodes] for xi, yi, _ in nodes]
    return n, k, dist, cost


def tour_weight(tour, dist, cost) -> int:
    """Objective of a closed tour, calculated from scratch."""
    return sum(dist[tour[t]][tour[(t + 1) % len(tour)]] + cost[tour[t]] for t in range(len(tour)))


def best_local_search(instance: str, dist, cost):
    """Best lab 3 local search solution of the instance: (tour, weight, method), used as warm start."""
    best = None
    for f in (ROOT / 'data' / 'lab_3' / 'output').glob(f'{instance}_*.txt'):
        lines = f.read_text().splitlines()
        tour = list(map(int, lines[1].removeprefix('Path:').split()))
        weight = int(lines[2].removeprefix('Total weight:'))
        if best is None or weight < best[1]:
            best = (tour, weight, f.stem.split('_', 1)[1])
    assert best is not None, 'no lab 3 results, run src/lab_3/main.cpp first'
    assert tour_weight(best[0], dist, cost) == best[1]
    return best


def save_tour(path: Path, instance: str, tour, weight: int) -> None:
    """Saves a tour in the same format as the lab outputs (readable by src/utils/plotting)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f'Input file: data/input/{instance}.csv\n'
                    f'Path: {" ".join(map(str, tour))} \n'
                    f'Total weight: {weight}\n')


class RunLog:
    """Prints messages and keeps them in a file."""

    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.file = open(path, 'w', encoding='utf-8')

    def __call__(self, message: str) -> None:
        print(message, flush=True)
        self.file.write(message + '\n')
        self.file.flush()
