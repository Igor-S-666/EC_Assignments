"""
Exact ILP for the "select half of the nodes + Hamiltonian cycle" problem, solved with SCIP (PySCIPOpt).

Same model as src/extra/pulp/ilp_solve.py:
    min  sum d_ij x_ij + sum c_i y_i
    s.t. sum y_i = k
         sum_j x_ij = 2 y_i              for every node i
         sum_{e in E(S)} x_e <= |S| - 1  for every node set S with |S| < k (subtour elimination)
but the subtour constraints are added lazily inside a single branch and bound by a constraint handler,
instead of re-solving the whole problem after each round of cuts. The cut is valid for any |S| < k,
because in a tour through k nodes the edges inside a smaller set S form paths (at most |S| - 1 edges).
Fractional LP solutions are separated too (connected components of the support graph).
The best local search solution from lab 3 is used as a starting solution.

Usage (from the repository root):
    uv run --no-project --with "pyscipopt==5.1.1" python src/extra/scip/scip_solve.py <instance> [time_limit_s]
Results go to data/extra/scip/: run log, SCIP log and the best tour found (<instance>_scip.txt).
"""
import sys
from pathlib import Path

from pyscipopt import SCIP_RESULT, Conshdlr, Model, quicksum

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import ROOT, RunLog, best_local_search, load_instance, save_tour, tour_weight  # noqa: E402

instance = sys.argv[1]
time_limit = float(sys.argv[2]) if len(sys.argv) > 2 else 600.0
out_dir = ROOT / 'data' / 'extra' / 'scip'
log = RunLog(out_dir / f'run_{instance}.txt')

n, k, dist, cost = load_instance(instance)
ls_tour, ls_weight, ls_name = best_local_search(instance, dist, cost)
edges = [(i, j) for i in range(n) for j in range(i + 1, n)]

model = Model(f'select_tsp_{instance}')
x = {e: model.addVar(f'x_{e[0]}_{e[1]}', vtype='B', obj=dist[e[0]][e[1]]) for e in edges}
y = [model.addVar(f'y_{i}', vtype='B', obj=cost[i]) for i in range(n)]
model.addCons(quicksum(y) == k)
incident = {i: [] for i in range(n)}
for (i, j), var in x.items():
    incident[i].append(var)
    incident[j].append(var)
for i in range(n):
    model.addCons(quicksum(incident[i]) == 2 * y[i])


def components(values, threshold):
    """Connected components of the graph of edges with value > threshold."""
    adj = {}
    for (i, j), value in values.items():
        if value > threshold:
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


class SubtourElimination(Conshdlr):
    cuts = 0

    def edge_values(self, solution=None):
        return {e: self.model.getSolVal(solution, var) for e, var in x.items()}

    def add_cuts(self, solution=None, threshold=0.5, eps=1e-6):
        """Adds sum_{E(S)} x <= |S| - 1 for violated components S with |S| < k; returns whether any was added."""
        values = self.edge_values(solution)
        added = False
        for comp in components(values, threshold):
            if len(comp) >= k:
                continue
            S = set(comp)
            inside = [e for e in edges if e[0] in S and e[1] in S]
            if sum(values[e] for e in inside) > len(S) - 1 + eps:
                self.model.addCons(quicksum(x[e] for e in inside) <= len(S) - 1,
                                   name=f'subtour_{SubtourElimination.cuts}')
                SubtourElimination.cuts += 1
                added = True
        return added

    def has_subtour(self, solution):
        comps = components(self.edge_values(solution), 0.5)
        return not (len(comps) == 1 and len(comps[0]) == k)

    def conscheck(self, constraints, solution, checkintegrality, checklprows, printreason, completely):
        return {'result': SCIP_RESULT.INFEASIBLE if self.has_subtour(solution) else SCIP_RESULT.FEASIBLE}

    def consenfolp(self, constraints, nusefulconss, solinfeasible):
        return {'result': SCIP_RESULT.CONSADDED if self.add_cuts() else SCIP_RESULT.FEASIBLE}

    def consenfops(self, constraints, nusefulconss, solinfeasible, objinfeasible):
        return {'result': SCIP_RESULT.CONSADDED if self.add_cuts() else SCIP_RESULT.FEASIBLE}

    def conssepalp(self, constraints, nusefulconss):
        # fractional LP solution: components of edges with any positive value
        return {'result': SCIP_RESULT.CONSADDED if self.add_cuts(threshold=1e-6) else SCIP_RESULT.DIDNOTFIND}

    def conslock(self, constraint, locktype, nlockspos, nlocksneg):
        pass


model.includeConshdlr(SubtourElimination(), 'subtour', 'subtour elimination',
                      sepapriority=1, enfopriority=-1, chckpriority=-1, sepafreq=1, needscons=False)

# starting solution: best local search tour
start = model.createSol()
ls_edges = {tuple(sorted((ls_tour[t], ls_tour[(t + 1) % k]))) for t in range(k)}
for e, var in x.items():
    model.setSolVal(start, var, 1 if e in ls_edges else 0)
for i in range(n):
    model.setSolVal(start, y[i], 1 if i in set(ls_tour) else 0)
model.addSol(start)

model.setParam('limits/time', time_limit)
model.setLogfile(str(out_dir / f'scip_{instance}.log'))
model.hideOutput()
log(f'[{instance}] model: {len(x) + n} vars; start {ls_name} = {ls_weight}; limit {time_limit:.0f}s')
model.optimize()

status = model.getStatus()
primal, dual = model.getPrimalbound(), model.getDualbound()
log(f'[{instance}] RESULT: {status}; objective {primal:.0f}, bound {dual:.0f}, '
    f'time {model.getSolvingTime():.1f}s, nodes {model.getNNodes()}, subtour cuts {SubtourElimination.cuts}')
best = model.getBestSol()
values = {e: model.getSolVal(best, var) for e, var in x.items()}
comps = components(values, 0.5)
assert len(comps) == 1 and len(comps[0]) == k, 'best solution is not a single tour'
adj = {}
for (i, j), value in values.items():
    if value > 0.5:
        adj.setdefault(i, []).append(j)
        adj.setdefault(j, []).append(i)
tour, prev = [comps[0][0]], None
while len(tour) < k:
    nxt = next(w for w in adj[tour[-1]] if w != prev)
    prev = tour[-1]
    tour.append(nxt)
weight = tour_weight(tour, dist, cost)
assert weight == round(primal)
log(f'[{instance}] tour {"OPTIMAL" if status == "optimal" else "best found"}: {weight} '
    f'(gap to bound {(weight - dual) / weight * 100:.2f}%), best local search {ls_weight}')
save_tour(out_dir / f'{instance}_scip.txt', instance, tour, weight)
