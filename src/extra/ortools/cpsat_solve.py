"""
Exact model of the "select half of the nodes + Hamiltonian cycle" problem, solved with OR-Tools CP-SAT.

Model (directed arcs):
    AddCircuit over all arcs i -> j, plus a self-loop i -> i for every node:
    a node with its self-loop chosen is not in the cycle (skipped).
    sum selected_i = k
    min  sum d_ij arc_ij + sum c_i selected_i
No subtour cuts are needed - the circuit constraint handles them.
The best local search solution from lab 3 is given as a hint.

Usage (from the repository root):
    uv run --no-project --with "ortools==9.10.4067" python src/extra/ortools/cpsat_solve.py <instance> [time_limit_s] [workers]
Results go to data/extra/ortools/: run log, CP-SAT search log and the best tour found (<instance>_cpsat.txt).
"""
import sys
import time
from pathlib import Path

from ortools.sat.python import cp_model

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import ROOT, RunLog, best_local_search, load_instance, save_tour, tour_weight  # noqa: E402

instance = sys.argv[1]
time_limit = float(sys.argv[2]) if len(sys.argv) > 2 else 600.0
workers = int(sys.argv[3]) if len(sys.argv) > 3 else 8
out_dir = ROOT / 'data' / 'extra' / 'ortools'
log = RunLog(out_dir / f'run_{instance}.txt')

n, k, dist, cost = load_instance(instance)
ls_tour, ls_weight, ls_name = best_local_search(instance, dist, cost)

model = cp_model.CpModel()
arcs, arc_vars = [], {}
for i in range(n):
    for j in range(n):
        if i != j:
            arc_vars[i, j] = model.NewBoolVar(f'a_{i}_{j}')
            arcs.append((i, j, arc_vars[i, j]))
selected = [model.NewBoolVar(f's_{i}') for i in range(n)]
for i in range(n):
    arcs.append((i, i, selected[i].Not()))  # self-loop = node skipped
model.AddCircuit(arcs)
model.Add(sum(selected) == k)
model.Minimize(sum(dist[i][j] * v for (i, j), v in arc_vars.items()) + sum(cost[i] * selected[i] for i in range(n)))

# hint: best local search tour
ls_succ = {ls_tour[t]: ls_tour[(t + 1) % k] for t in range(k)}
for (i, j), v in arc_vars.items():
    model.AddHint(v, int(ls_succ.get(i) == j))
for i in range(n):
    model.AddHint(selected[i], int(i in ls_succ))
log(f'[{instance}] model: {len(arc_vars) + n} bool vars; hint {ls_name} = {ls_weight}; '
    f'{workers} workers, limit {time_limit:.0f}s')


class Progress(cp_model.CpSolverSolutionCallback):
    def __init__(self):
        super().__init__()
        self.start = time.time()

    def on_solution_callback(self):
        log(f'[{instance}] {time.time() - self.start:7.1f}s  solution {self.ObjectiveValue():.0f}  '
            f'bound {self.BestObjectiveBound():.0f}')


solver = cp_model.CpSolver()
solver.parameters.max_time_in_seconds = time_limit
solver.parameters.num_search_workers = workers
solver.parameters.log_search_progress = True
solver.parameters.log_to_stdout = False
search_log = open(out_dir / f'cpsat_{instance}.log', 'w', encoding='utf-8')
solver.log_callback = lambda line: search_log.write(line + '\n')
status = solver.Solve(model, Progress())
search_log.close()

status_name = solver.StatusName(status)
log(f'[{instance}] RESULT: {status_name}; objective {solver.ObjectiveValue():.0f}, '
    f'bound {solver.BestObjectiveBound():.0f}, time {solver.WallTime():.1f}s')
if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
    succ = {i: j for (i, j), v in arc_vars.items() if solver.Value(v)}
    tour = [next(iter(succ))]
    while len(tour) < len(succ):
        tour.append(succ[tour[-1]])
    weight = tour_weight(tour, dist, cost)
    assert len(tour) == k and weight == round(solver.ObjectiveValue())
    gap = (weight - solver.BestObjectiveBound()) / weight * 100
    log(f'[{instance}] tour {"OPTIMAL" if status == cp_model.OPTIMAL else "best found"}: {weight} '
        f'(gap to bound {gap:.2f}%), best local search {ls_weight}')
    save_tour(out_dir / f'{instance}_cpsat.txt', instance, tour, weight)
