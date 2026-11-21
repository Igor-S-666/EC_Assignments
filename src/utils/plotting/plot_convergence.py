"""
Convergence of ILS and LNS (lab 7, additional experiment).

Reads <lab_dir>/results/convergence.csv (instance;method;run;iteration;time(microsec);objective - the initial
solution, every improvement and a final point of each run) and saves <lab_dir>/plots/convergence.<format>:
the average objective of the current solution over all runs, against the number of iterations (top row) and
against the running time (bottom row), one column per instance. Both x axes are logarithmic.

A run's objective is a step function (it only changes at improvements). The average at x is taken over all runs,
so each curve ends where the shortest run ends. The average MSLS objective (from the lab 6 statistics, if found)
is a grey reference line.
"""

from collections import defaultdict
from pathlib import Path
import argparse
import bisect
import csv

import matplotlib.pyplot as plt
import numpy as np

# categorical slots 1-4 of the reference palette (validated: light mode, 4 slots); ILS solid, LNS dashed as a
# secondary encoding (two of the hues are below 3:1 against the surface)
STYLES = {
    'ILS_double_bridge': ('ILS, double bridge', '#2a78d6', '-'),
    'ILS_segment_relocation': ('ILS, segment relocation', '#eb6834', '-'),
    'LNS_multi_segment_LS': ('LNS, 3-5 subpaths, LS', '#1baf7a', '--'),
    'LNS_multi_segment_noLS': ('LNS, 3-5 subpaths, no LS', '#eda100', '--'),
}
REFERENCE = '#6b6a64'
GRID_COLOR = '#e4e3dd'
TEXT_SECONDARY = '#3d3c37'
SURFACE = '#fcfcfb'


def read_runs(path: Path):
    """{(instance, method): [run points as (iteration, time_ms, objective) lists]}"""
    runs = defaultdict(list)
    with open(path, newline='') as f:
        for row in csv.DictReader(f, delimiter=';'):
            key = (row['instance'], row['method'], row['run'])
            runs[key].append((int(row['iteration']), int(row['time(microsec)']) / 1000, int(row['objective'])))
    grouped = defaultdict(list)
    for (instance, method, _), points in runs.items():
        grouped[(instance, method)].append(points)
    return grouped


def msls_averages(lab_dir: Path) -> dict[str, float]:
    path = lab_dir.parent / 'lab_6' / 'results' / 'statistics.csv'
    if not path.exists():
        return {}
    with open(path, newline='') as f:
        return {r['instance']: float(r['average']) for r in csv.DictReader(f, delimiter=';') if r['method'] == 'MSLS'}


def average_curve(runs, axis: int, grid: np.ndarray) -> np.ndarray:
    """Average over runs of the step function objective(x), x on `axis` (0 = iteration, 1 = time)."""
    values = []
    for points in runs:
        xs = [p[axis] for p in points]
        objectives = [p[2] for p in points]
        # value at x = objective of the last point with x_point <= x
        idx = [bisect.bisect_right(xs, x) - 1 for x in grid]
        values.append([objectives[max(i, 0)] for i in idx])
    return np.mean(values, axis=0)


def style_axis(ax) -> None:
    ax.set_facecolor(SURFACE)
    ax.grid(color=GRID_COLOR, linewidth=0.8, which='major')
    ax.set_axisbelow(True)
    for side in ('top', 'right'):
        ax.spines[side].set_visible(False)
    for side in ('left', 'bottom'):
        ax.spines[side].set_color(REFERENCE)
    ax.tick_params(colors=TEXT_SECONDARY)


def plot_convergence(lab_dir: Path, plot_format: str, csv_name: str = 'convergence.csv') -> Path:
    grouped = read_runs(lab_dir / 'results' / csv_name)
    instances = sorted({instance for instance, _ in grouped})
    methods = [m for m in STYLES if any((i, m) in grouped for i in instances)]
    msls = msls_averages(lab_dir)

    fig, axes = plt.subplots(2, len(instances), figsize=(6.4 * len(instances), 8.2), constrained_layout=True,
                             squeeze=False)
    fig.patch.set_facecolor(SURFACE)
    for col, instance in enumerate(instances):
        finals = []
        for row, (axis, xlabel) in enumerate([(0, 'Iterations of the main loop (log scale)'),
                                              (1, 'Running time [ms] (log scale)')]):
            ax = axes[row][col]
            for method in methods:
                runs = grouped[(instance, method)]
                end = min(points[-1][axis] for points in runs)  # all runs still running up to here
                start = 1 if axis == 0 else 0.5  # the initial local search alone takes about 0.5 ms
                grid = np.unique(np.geomspace(start, end, 400).round(0 if axis == 0 else 3))
                label, color, dash = STYLES[method]
                curve = average_curve(runs, axis, grid)
                ax.plot(grid, curve, color=color, linestyle=dash, linewidth=2, label=label)
                finals.append(curve[-1])
            if instance in msls:
                ax.axhline(msls[instance], color=REFERENCE, linewidth=1, linestyle=':', zorder=1)
                ax.annotate('MSLS average', xy=(1, msls[instance]), xycoords=('axes fraction', 'data'),
                            xytext=(-4, 4), textcoords='offset points', ha='right', fontsize=8, color=TEXT_SECONDARY)
            ax.set_xscale('log')
            # zoom on the interesting part: from slightly below the best final average to above the MSLS average
            low = min(finals)
            high = msls.get(instance, max(finals))
            ax.set_ylim(low - 0.15 * (high - low), high + 0.35 * (high - low))
            ax.set_xlabel(xlabel)
            ax.set_ylabel('Average objective (lower is better)')
            if row == 0:
                ax.set_title(instance, loc='left', fontweight='bold')
            style_axis(ax)

    handles, labels = axes[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='outside lower center', ncols=len(methods), frameon=False)
    fig.suptitle('Average objective of the current solution over 20 runs', fontsize=11)
    path = lab_dir / 'plots' / f'{Path(csv_name).stem}.{plot_format}'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150, facecolor=SURFACE)
    plt.close(fig)
    return path


def main():
    # Example usage (from the repository root):
    # python src/utils/plotting/plot_convergence.py data/lab_7 --no-show
    parser = argparse.ArgumentParser(description='Plot the convergence of ILS and LNS (lab 7).')
    parser.add_argument('lab_dir', type=str, help='Lab data directory, e.g. data/lab_7')
    parser.add_argument('--no-show', action='store_true', help='Only save the plot, do not display it')
    parser.add_argument('--format', dest='plot_format', type=str, default='png',
                        help='Format of the saved plot (png, pdf, svg)')
    parser.add_argument('--csv', type=str, default='convergence.csv',
                        help='File in <lab_dir>/results (e.g. convergence_10000.csv); the plot gets the same name')
    args = parser.parse_args()
    path = plot_convergence(Path(args.lab_dir), args.plot_format, args.csv)
    print(f'saved {path}')
    if not args.no_show:
        plt.figure()
        plt.imshow(plt.imread(path))
        plt.axis('off')
        plt.show()


if __name__ == '__main__':
    main()
