/**
 * @file local_steepest_fast.hpp
 * @brief This file contains the implementation of faster version of the steepest descent local search algorithm,
 *  which uses previous iterations delta cost to avoid unnecessary cost calculations.
 */

#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "../../utils/neighbour_utils/types.hpp"
#include "../../utils/neighbour_utils/neighbour_exchanges.hpp"
#include "candidate_lists.hpp"
#include "local_search_base.hpp"

// Neighbourhood is fixed to the best one from lab 3: intra edge exchange (2-opt) + inter node exchange,
// optionally restricted to candidate moves (moves introducing at least one candidate edge, as in lab 4).
class LocalSearchSteepestFast : public FixedLocalSearchBase {
public:
    using FixedLocalSearchBase::FixedLocalSearchBase;
    using FixedLocalSearchBase::run;

    // candidate lists of the instance the next runs are on; nullptr (default) -> full neighbourhood
    void set_candidates(const CandidateLists* candidate_lists) { candidates = candidate_lists; }

    algorithm_result run(const algorithm_data& data,
                         std::string initial_solution_method,
                         int initial_solution_start_node) override {
        const algorithm_result initial = initial_solution(data, initial_solution_method, initial_solution_start_node);
        return Search(data, initial, lm_buffer, candidates).run();
    }

private:
    // Stored by node ids (positions change after every 2-opt). 16 bytes instead of 24 with ints, which makes
    // passes over LM noticeably faster; node ids (< 200) easily fit in short.
    //  type 0 (2-opt):         remove edges a->b and c->d, add (a, c) and (b, d)
    //  type 1 (node exchange): node b (between a and c) leaves, unselected node d enters
    struct move_t {
        int delta;
        short type, a, b, c, d;
    };
    static move_t make_move(int delta, int type, int a, int b, int c, int d) {
        return {delta, short(type), short(a), short(b), short(c), short(d)};
    }

    // memory of LM, kept between runs so it's allocated (and paged in) only once
    std::vector<move_t> lm_buffer;
    const CandidateLists* candidates = nullptr;

    // Result of checking a saved move against the current solution.
    enum : int { REMOVE = 0, SKIP = 2 };  // otherwise +1 / -1: apply, edges in saved / reversed direction

    // number of best applicable moves kept in the front part of LM (see Search), approximately:
    // the threshold is picked from a sample of every SAMPLE_STRIDE-th applicable move
    static constexpr std::size_t FRONT_SIZE = 64;
    static constexpr std::size_t SAMPLE_STRIDE = 16;

    /**
     * One run of the search.
     *
     * The list of improving moves (LM) is a plain vector that is never sorted: most moves become inapplicable
     * before they'd be reached. It's split by a delta threshold, lm[0, front) holds the moves with
     * delta <= threshold and the rest are worse. So if the front has an applicable move, its best one is the
     * best applicable move of the whole LM - the one browsing a sorted LM would stop at - and each iteration
     * scans only the front. The whole LM is scanned (and pruned) only when the front runs out of applicable moves.
     * Duplicates are harmless: after the first copy is applied its edges are gone and the other one gets removed.
     */
    class Search {
    public:
        Search(const algorithm_data& data, const algorithm_result& initial, std::vector<move_t>& lm_buffer,
               const CandidateLists* candidates)
            : dist(data.distance_matrix), cost(data.cost_matrix), candidates(candidates),
              state(initial.path, data.nodes_num), path(state.path), m(state.size()),
              total_weight(initial.total_weight), pos(data.nodes_num, -1), upos(data.nodes_num, -1), lm(lm_buffer) {
            for (int i = 1; i <= m; ++i) pos[path[i]] = i;
            for (int k = 0; k < static_cast<int>(state.unselected.size()); ++k) upos[state.unselected[k]] = k;
            lm.clear();  // keeps the capacity from previous runs
            lm.reserve(m * m);
        }

        algorithm_result run() {
            // evaluate all moves applicable to the initial solution
            for (int i = 1; i <= m; ++i) {
                if (candidates) {
                    for (const int* it = candidates->begin(path[i]); it != candidates->end(path[i]); ++it) {
                        if (pos[*it] >= 0 && path[i] < *it) {
                            add_candidate_two_opts(path[i], *it, true);
                            add_candidate_two_opts(path[i], *it, false);
                        }
                    }
                    add_candidate_exchanges(path[i]);
                } else {
                    add_edge_moves(path[i], path[i + 1], i + 1);
                    add_node_moves(path[i]);
                }
            }

            while (true) {
                const auto [best, status] = best_in_front();
                if (status == REMOVE) {
                    if (!rebuild_front()) break;  // no applicable improving move -> local optimum
                    continue;
                }
                const move_t mv = lm[best];
                remove_from_front(best);
                total_weight += mv.delta;
                if (mv.type == 0) {
                    apply_two_opt(mv, status);
                } else {
                    apply_exchange(mv);
                }
            }
            return {state.cycle(), total_weight};
        }

    private:
        // intra moves keep the node set -> distances; node exchanges -> cost_matrix (includes the node weights)
        const distance_matrix_t& dist;
        const distance_matrix_t& cost;
        const CandidateLists* candidates;  // nullptr -> full neighbourhood
        local_search_state state;
        std::vector<int>& path;
        const int m;
        int total_weight;
        // pos[node] = position in path (1..m) or -1, upos[node] = index in state.unselected or -1
        std::vector<int> pos, upos;

        std::vector<move_t>& lm;
        std::size_t front = 0;
        int threshold = std::numeric_limits<int>::min();
        std::vector<int> applicable;  // buffer of rebuild_front

        // padding (path[0] == path[m], path[m + 1] == path[1]) makes these modulo-free
        int next(int x) const { return path[pos[x] + 1]; }
        int prev(int x) const { return path[pos[x] - 1]; }

        // +1: edge a->b exists, -1: exists as b->a, 0: edge no longer exists
        int edge_status(int a, int b) const {
            if (pos[a] < 0 || pos[b] < 0) return 0;
            if (next(a) == b) return 1;
            if (next(b) == a) return -1;
            return 0;
        }

        int check(const move_t& mv) const {
            const int a = mv.a, b = mv.b, c = mv.c, d = mv.d;
            if (mv.type == 0) {
                const int s1 = edge_status(a, b), s2 = edge_status(c, d);
                if (s1 == 0 || s2 == 0) return REMOVE;
                return s1 == s2 ? s1 : SKIP;
            }
            // node exchange doesn't depend on direction: b still between a and c, d still unselected
            if (pos[b] < 0 || upos[d] < 0) return REMOVE;
            const int p = prev(b), n = next(b);
            return (p == a && n == c) || (p == c && n == a) ? 1 : REMOVE;
        }

        void add(const move_t& mv) {
            lm.push_back(mv);
            if (mv.delta <= threshold) std::swap(lm[front++], lm.back());
        }

        void remove_from_front(std::size_t i) {
            lm[i] = lm[--front];
            lm[front] = lm.back();
            lm.pop_back();
        }

        // Pairs the current edge a->b with the edges (path[i], path[i + 1]), i = first..m. For edges a->b and
        // c->d both reconnections are evaluated. The second one, (a, d) + (b, c), is the move with relatively
        // inverted edges: it's saved as a->b, d->c, so it's skipped until some reversal flips one of these edges.
        void add_edge_moves(int a, int b, int first = 1) {
            const int* da = dist[a].data();
            const int* db = dist[b].data();
            for (int i = first; i <= m; ++i) {
                const int c = path[i], d = path[i + 1];
                if (c == a || c == b || d == a) continue;  // the same or adjacent edges
                const int removed = da[b] + dist[c][d];
                if (const int delta = da[c] + db[d] - removed; delta < 0) add(make_move(delta, 0, a, b, c, d));
                if (const int delta = da[d] + db[c] - removed; delta < 0) add(make_move(delta, 0, a, b, d, c));
            }
        }

        void add_exchange(int v, int u) {
            const int p = prev(v), n = next(v);
            const int delta = cost[p][u] + cost[u][n] - cost[p][v] - cost[v][n];
            if (delta < 0) add(make_move(delta, 1, p, v, n, u));
        }

        // Replace selected node v with every unselected node. cost[x][n] = dist[n][x] + weight[n], and the
        // weight[n] terms cancel out, so only the rows of p and n are read.
        void add_node_moves(int v) {
            const int p = prev(v), n = next(v);
            const int* cp = cost[p].data();
            const int* dn = dist[n].data();
            const int removed = cp[v] + dn[v];
            for (int u : state.unselected) {
                if (const int delta = cp[u] + dn[u] - removed; delta < 0) add(make_move(delta, 1, p, v, n, u));
            }
        }

        // ---- candidate mode: only moves introducing at least one candidate edge ----

        // 2-opt removing the edges a->b and c->d (as saved, see move_t) and adding (a, c), (b, d)
        void add_two_opt(int a, int b, int c, int d) {
            if (a == c || a == d || b == c || b == d) return;  // removed edges share a node
            const int delta = dist[a][c] + dist[b][d] - dist[a][b] - dist[c][d];
            if (delta < 0) add(make_move(delta, 0, a, b, c, d));
        }

        // The 2-opts adding the candidate edge (x, y), x and y selected, that remove x's next edge (x_next) or
        // its previous one, together with y's next or previous edge. One of the two is applicable now; the other
        // removes y's edge in the opposite direction, so it's the relatively inverted move (skipped until some
        // reversal flips one of these edges).
        void add_candidate_two_opts(int x, int y, bool x_next) {
            const int ny = next(y), py = prev(y);
            if (x_next) {
                const int nx = next(x);
                add_two_opt(x, nx, y, ny);  // remove x->nx, y->ny; add (x, y), (nx, ny)
                add_two_opt(x, nx, y, py);  // remove x->nx, py->y; add (x, y), (nx, py)
            } else {
                const int px = prev(x);
                add_two_opt(px, x, py, y);  // remove px->x, py->y; add (px, py), (x, y)
                add_two_opt(px, x, ny, y);  // remove px->x, y->ny; add (px, ny), (x, y)
            }
        }

        // Exchanges of the selected node s for an unselected node that is a candidate neighbour of s's
        // predecessor or successor (so it introduces a candidate edge). Rows of p and n are fixed, as in
        // add_node_moves.
        void add_candidate_exchanges(int s) {
            const int p = prev(s), n = next(s);
            const int* cp = cost[p].data();
            const int* dn = dist[n].data();
            const int removed = cp[s] + dn[s];
            for (const int* it = candidates->begin(p); it != candidates->end(p); ++it) {
                const int w = *it;
                if (upos[w] < 0) continue;
                if (const int delta = cp[w] + dn[w] - removed; delta < 0) add(make_move(delta, 1, p, s, n, w));
            }
            for (const int* it = candidates->begin(n); it != candidates->end(n); ++it) {
                const int w = *it;
                if (upos[w] < 0 || candidates->is_candidate(p, w)) continue;  // done in the first loop
                if (const int delta = cp[w] + dn[w] - removed; delta < 0) add(make_move(delta, 1, p, s, n, w));
            }
        }

        void add_if_improving(int delta, int a, int b, int c, int d) {
            if (delta < 0 && a != c && a != d && b != c && b != d) add(make_move(delta, 0, a, b, c, d));
        }

        // Candidate 2-opts removing the new edge a->b: those adding a candidate edge (a, y) (a's next edge
        // removed) or (b, y) (b's previous edge removed), each with y's next or previous edge - the second
        // of them being the relatively inverted move, as in add_candidate_two_opts. Rows of a and b are fixed.
        void add_candidate_two_opts_removing(int a, int b) {
            const int* da = dist[a].data();
            const int* db = dist[b].data();
            const int ab = da[b];
            for (const int* it = candidates->begin(a); it != candidates->end(a); ++it) {
                const int y = *it;
                if (pos[y] < 0) continue;
                const int ny = next(y), py = prev(y);
                add_if_improving(da[y] + db[ny] - ab - dist[y][ny], a, b, y, ny);  // add (a, y), (b, ny)
                add_if_improving(da[y] + db[py] - ab - dist[py][y], a, b, y, py);  // add (a, y), (b, py)
            }
            for (const int* it = candidates->begin(b); it != candidates->end(b); ++it) {
                const int y = *it;
                if (pos[y] < 0) continue;
                const int ny = next(y), py = prev(y);
                add_if_improving(da[py] + db[y] - ab - dist[py][y], a, b, py, y);  // add (a, py), (b, y)
                add_if_improving(da[ny] + db[y] - ab - dist[y][ny], a, b, ny, y);  // add (a, ny), (b, y)
            }
        }

        // New candidate moves after a move created the edges `new_edges` (pairs a->b in the current direction),
        // which gave the nodes `touched` (their endpoints) new neighbours. A candidate 2-opt removing a new edge
        // adds a candidate edge at one of its endpoints, so only those are generated - moves removing old edges
        // are already in LM. Every exchange of a touched node removes a new edge.
        void add_candidate_moves_around(std::initializer_list<std::pair<int, int>> new_edges,
                                        std::initializer_list<int> touched) {
            for (const auto& [a, b] : new_edges) add_candidate_two_opts_removing(a, b);
            for (const int t : touched) add_candidate_exchanges(t);
        }

        // {index, check status} of the best applicable move of the front (status REMOVE if there is none),
        // dropping removed moves on the way
        std::pair<std::size_t, int> best_in_front() {
            std::size_t best = 0;
            int status = REMOVE;
            for (std::size_t i = 0; i < front;) {
                const int s = check(lm[i]);
                if (s == REMOVE) { remove_from_front(i); continue; }
                if (s != SKIP && (status == REMOVE || lm[i].delta < lm[best].delta ||
                                  (lm[i].delta == lm[best].delta && scan_order(lm[i], s) < scan_order(lm[best], status)))) {
                    best = i;
                    status = s;
                }
                ++i;
            }
            return {best, status};
        }

        // Path positions {i, j}, i < j, of the removed edges (i, i + 1) and (j, j + 1) of an applicable 2-opt.
        std::pair<int, int> two_opt_positions(const move_t& mv, int status) const {
            // both edges reversed: the same move read along the current direction is d->c, b->a
            int i = pos[status > 0 ? mv.a : mv.d], j = pos[status > 0 ? mv.c : mv.b];
            if (i > j) std::swap(i, j);
            return {i, j};
        }

        // Order in which the plain steepest search (lab 3) scans an applicable move: 2-opt by (i, j), then
        // exchanges by (position, index in unselected). Ties are broken by it, so both searches take the
        // same moves and end in exactly the same local optima.
        int scan_order(const move_t& mv, int status) const {
            if (mv.type == 0) {
                const auto [i, j] = two_opt_positions(mv, status);
                return i * (m + 2) + j;
            }
            return (m + 2) * (m + 2) + pos[mv.b] * static_cast<int>(state.unselected.size()) + upos[mv.d];
        }

        // Compacts the whole LM, then moves about FRONT_SIZE best applicable moves (and everything at least
        // as good, skipped moves included) to the front. Returns false if LM has no applicable move.
        // The threshold is a delta of an applicable move, so the front always gets at least one.
        bool rebuild_front() {
            applicable.clear();
            std::size_t kept = 0, applicable_count = 0;
            for (const move_t& mv : lm) {
                const int s = check(mv);
                if (s == REMOVE) continue;
                if (s != SKIP && applicable_count++ % SAMPLE_STRIDE == 0) applicable.push_back(mv.delta);
                lm[kept++] = mv;
            }
            lm.resize(kept);
            if (applicable.empty()) return false;

            const std::size_t k = std::min(applicable.size(), FRONT_SIZE / SAMPLE_STRIDE) - 1;
            std::nth_element(applicable.begin(), applicable.begin() + k, applicable.end());
            threshold = applicable[k];
            front = std::partition(lm.begin(), lm.end(), [this](const move_t& mv) { return mv.delta <= threshold; })
                    - lm.begin();
            return true;
        }

        void apply_two_opt(const move_t& mv, int status) {
            const auto [i, j] = two_opt_positions(mv, status);
            intra_edge_swap(state, i, j);
            for (int k = i + 1; k <= j; ++k) pos[path[k]] = k;

            // new edges are (i, i + 1) and (j, j + 1); these 4 nodes have new neighbours
            if (candidates) {
                add_candidate_moves_around({{path[i], path[i + 1]}, {path[j], path[j + 1]}},
                                           {path[i], path[i + 1], path[j], path[j + 1]});
                return;
            }
            add_edge_moves(path[i], path[i + 1]);
            add_edge_moves(path[j], path[j + 1]);
            for (int k : {i, i + 1, j, j + 1}) add_node_moves(path[k]);
        }

        void apply_exchange(const move_t& mv) {
            const int v = mv.b, u = mv.d, i = pos[v], k = upos[u];
            inter_node_swap(state, i, k);
            pos[u] = i, upos[u] = -1;
            pos[v] = -1, upos[v] = k;

            // saved neighbours of v, oriented along the current direction (new edges are p->u, u->n)
            int p = mv.a, n = mv.c;
            if (prev(u) != p) std::swap(p, n);
            if (candidates) {
                add_candidate_moves_around({{p, u}, {u, n}}, {p, u, n});
                // v is unselected now: exchanges putting it next to its selected candidate neighbours
                for (const int* it = candidates->begin(v); it != candidates->end(v); ++it) {
                    if (pos[*it] >= 0) {
                        add_exchange(next(*it), v);
                        add_exchange(prev(*it), v);
                    }
                }
                return;
            }
            add_edge_moves(p, u);
            add_edge_moves(u, n);
            for (int x : {p, u, n}) add_node_moves(x);
            // v is unselected now, so it can replace any selected node
            for (int q = 1; q <= m; ++q) add_exchange(path[q], v);
        }
    };
};
