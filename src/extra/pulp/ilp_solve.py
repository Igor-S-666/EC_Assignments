"""
Exact ILP for the "select half of the nodes + Hamiltonian cycle" problem, solved with PuLP + CBC.

Model (undirected edges):
    min  sum d_ij x_ij + sum c_i y_i
    s.t. sum y_i = k
         sum_j x_ij = 2 y_i              for every node i
         sum_{e in E(S)} x_e <= |S| - 1  for subtours S found so far (lazy, |S| < k)
Subtour cuts are added iteratively: solve, find cycles, cut them, solve again.
The best local search solution from lab 3 is used as a warm start.

Every fully solved iteration (status optimal) is a valid lower bound of the real optimum,
because it is the optimum of a relaxation (only some of the subtour constraints).

Usage (from the repository root; PuLP 3+ changed the API, so 2.9.0 is pinned):
    uv run --no-project --with "pulp==2.9.0" python src/extra/pulp/ilp_solve.py <instance> [time_limit_s] [--strong]
  --strong adds x_ij <= y_i, x_ij <= y_j (tighter LP relaxation, bigger model)
Results (run log, last CBC log, optimal tour if found) go to data/extra/pulp/.
"""
import math
import re
import sys
import time
from pathlib import Path

import pulp

root = Path(__file__).resolve().parents[3]
instance = sys.argv[1]
time_limit = float(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith('--') else 600.0
strong = '--strong' in sys.argv
tag = f'{instance}{"_strong" if strong else ""}'
out_dir = root / 'data' / 'extra' / 'pulp'
out_dir.mkdir(parents=True, exist_ok=True)
run_log = open(out_dir / f'run_{tag}.txt', 'w', encoding='utf-8')


def log(message):
    """Prints the message and keeps it in data/extra/pulp/run_<tag>.txt."""
    print(message, flush=True)
    run_log.write(message + '\n')
    run_log.flush()

# ---- data, identical to the C++ parser / preprocessing
nodes = [tuple(map(int, line.split(';'))) for line in
         (root / 'data' / 'input' / f'{instance}.csv').read_text().split()]
n = len(nodes)
k = (n + 1) // 2
cost = [c for _, _, c in nodes]
def dist(i, j):  # std::lround(std::sqrt(...)) for non-negative values
    (xi, yi, _), (xj, yj, _) = nodes[i], nodes[j]
    return math.floor(math.hypot(xi - xj, yi - yj) + 0.5)
edges = [(i, j) for i in range(n) for j in range(i + 1, n)]
d = {e: dist(*e) for e in edges}

# ---- warm start: best lab 3 solution
best_ls = None
for f in (root / 'data' / 'lab_3' / 'output').glob(f'{instance}_*.txt'):
    lines = f.read_text().splitlines()
    path = list(map(int, lines[1].removeprefix('Path:').split()))
    weight = int(lines[2].removeprefix('Total weight:'))
    if best_ls is None or weight < best_ls[1]:
        best_ls = (path, weight, f.stem)
ls_path, ls_weight, ls_name = best_ls
ls_edges = {tuple(sorted((ls_path[t], ls_path[(t + 1) % k]))) for t in range(k)}
assert sum(d[e] for e in ls_edges) + sum(cost[v] for v in ls_path) == ls_weight

# ---- model
t0 = time.time()
prob = pulp.LpProblem(f'select_tsp_{instance}', pulp.LpMinimize)
x = {e: pulp.LpVariable(f'x_{e[0]}_{e[1]}', cat='Binary') for e in edges}
y = [pulp.LpVariable(f'y_{i}', cat='Binary') for i in range(n)]
prob += pulp.lpSum(d[e] * x[e] for e in edges) + pulp.lpSum(cost[i] * y[i] for i in range(n))
prob += pulp.lpSum(y) == k
incident = {i: [] for i in range(n)}
for e in edges:
    incident[e[0]].append(x[e])
    incident[e[1]].append(x[e])
for i in range(n):
    prob += pulp.lpSum(incident[i]) == 2 * y[i]
if strong:
    for (i, j), var in x.items():
        prob += var <= y[i]
        prob += var <= y[j]
log(f'[{tag}] model: {len(x) + len(y)} vars, {len(prob.constraints)} constraints, '
      f'built in {time.time() - t0:.1f}s; warm start {ls_name} = {ls_weight}')


def components(chosen_edges):
    adj = {}
    for i, j in chosen_edges:
        adj.setdefault(i, []).append(j)
        adj.setdefault(j, []).append(i)
    seen, comps = set(), []
    for s in adj:
        if s in seen:
            continue
        stack, comp = [s], []
        seen.add(s)
        while stack:
            v = stack.pop()
            comp.append(v)
            for w in adj[v]:
                if w not in seen:
                    seen.add(w)
                    stack.append(w)
        comps.append(comp)
    return comps


start = time.time()
iteration, cuts = 0, 0
log_file = out_dir / f'cbc_{tag}.log'
result = None
while True:
    remaining = time_limit - (time.time() - start)
    if remaining <= 1:
        result = 'time limit reached between iterations'
        break
    iteration += 1
    for e in edges:
        x[e].setInitialValue(1 if e in ls_edges else 0)
    for i in range(n):
        y[i].setInitialValue(1 if i in set(ls_path) else 0)
    solver = pulp.PULP_CBC_CMD(msg=False, timeLimit=remaining, warmStart=True, logPath=str(log_file))
    it_start = time.time()
    prob.solve(solver)
    it_time = time.time() - it_start

    # best bound from the CBC log (PuLP does not expose it)
    cbc_log = log_file.read_text(errors='replace')
    bounds = re.findall(r'best possible ([-\d.e+]+)', cbc_log)
    bound = float(bounds[-1]) if bounds else float('nan')

    chosen = [e for e in edges if x[e].value() is not None and x[e].value() > 0.5]
    comps = components(chosen)
    obj = pulp.value(prob.objective)
    status = pulp.LpStatus[prob.status]
    proven = prob.sol_status == pulp.LpSolutionOptimal
    log(f'[{tag}] it {iteration:3d}: {it_time:6.1f}s  obj {obj:.0f}  bound {bound:.0f}  '
          f'status {status}/{"optimal" if proven else "not proven"}  '
          f'cycles {len(comps)} (sizes {sorted(map(len, comps))[:6]}{"..." if len(comps) > 6 else ""})')

    if len(comps) == 1 and len(comps[0]) == k:
        result = 'OPTIMAL' if proven else 'feasible tour, optimality NOT proven (time limit)'
        break
    if not proven and len(comps) > 1:
        # time limit hit with an infeasible (subtour) incumbent: still a valid lower bound, keep cutting
        pass
    for comp in comps:
        if len(comp) < k:
            S = set(comp)
            prob += pulp.lpSum(x[e] for e in edges if e[0] in S and e[1] in S) <= len(S) - 1
            cuts += 1

total = time.time() - start
log(f'[{tag}] RESULT: {result}; iterations {iteration}, subtour cuts {cuts}, total {total:.1f}s')
if result and result.startswith(('OPTIMAL', 'feasible')):
    obj = round(pulp.value(prob.objective))
    log(f'[{tag}] objective {obj} vs best local search {ls_weight} '
          f'({(ls_weight - obj) / obj * 100:+.2f}% LS above ILP)')
    # write the tour in order, in the lab output format
    adj = {}
    for i, j in chosen:
        adj.setdefault(i, []).append(j)
        adj.setdefault(j, []).append(i)
    tour, prev = [comps[0][0]], None
    while len(tour) < k:
        nxt = next(w for w in adj[tour[-1]] if w != prev)
        prev = tour[-1]
        tour.append(nxt)
    (out_dir / f'{tag}_pulp.txt').write_text(
        f'Input file: data/input/{instance}.csv\nPath: {" ".join(map(str, tour))} \nTotal weight: {obj}\n')
else:
    # every iteration ended with subtours, so the only feasible tour is the warm start
    log(f'[{tag}] no single tour found by the solver, best known tour is the warm start ({ls_weight}); nothing saved')
