"""
Plots of the time-limited experiments (lab 6: MSLS / ILS, lab 7: LNS).

Reads <lab_dir>/results/runs.csv and saves <lab_dir>/plots/objective_runs.<format>: objective of every run
per method (dots) with the average (line), one panel per instance, MSLS (if present) as the grey reference.
"""

from collections import defaultdict
from pathlib import Path
import argparse
import csv
import re

import matplotlib.pyplot as plt

SERIES_1 = '#2a78d6'    # ILS
REFERENCE = '#6b6a64'   # MSLS
GRID_COLOR = '#e4e3dd'
TEXT_SECONDARY = '#3d3c37'


def read_rows(path: Path) -> list[dict[str, str]]:
    with open(path, newline='') as file:
        return list(csv.DictReader(file, delimiter=';'))


def short_name(method: str) -> str:
    # ILS_segment_relocation_strength_4 -> segment relocation
    if method.startswith('ILS_'):
        return method[len('ILS_'):].rsplit('_strength_', 1)[0].replace('_', ' ')
    # LNS_weighted_multi_segment_LS_rcl3 -> weighted multi segment, LS, rand. repair
    if method.startswith('LNS_'):
        name = method[len('LNS_'):]
        repair = ''
        if re.search(r'_rcl\d+$', name):
            name, repair = name.rsplit('_rcl', 1)[0], ', rand. repair'
        destroy, ls = name.rsplit('_', 1)
        destroy = destroy.replace('single_segment', 'single_subpath').replace('multi_segment', '3-5_subpaths')
        return f"{destroy.replace('_', ' ')}, {'LS' if ls == 'LS' else 'no LS'}{repair}"
    return method


def style_axis(ax, grid_axis: str) -> None:
    ax.grid(axis=grid_axis, color=GRID_COLOR, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ('top', 'right'):
        ax.spines[side].set_visible(False)
    for side in ('left', 'bottom'):
        ax.spines[side].set_color(REFERENCE)
    ax.tick_params(colors=TEXT_SECONDARY)


def plot_objectives(lab_dir: Path, plot_format: str, methods: list[str] | None = None) -> Path:
    rows = read_rows(lab_dir / 'results' / 'runs.csv')
    values: dict[str, dict[str, list[int]]] = defaultdict(lambda: defaultdict(list))
    for row in rows:
        if methods is None or row['method'] in methods:
            values[row['instance']][row['method']].append(int(row['objective']))
    instances = list(values)

    method_count = max(len(values[instance]) for instance in instances)
    fig, axes = plt.subplots(1, len(instances), figsize=(6.2 * len(instances), max(4.6, 0.32 * method_count + 1.2)),
                             constrained_layout=True)
    if len(instances) == 1:
        axes = [axes]
    for ax, instance in zip(axes, instances):
        methods = list(values[instance])  # order of main.cpp: MSLS first
        for y, method in enumerate(methods):
            runs = values[instance][method]
            color = REFERENCE if method == 'MSLS' else SERIES_1
            average = sum(runs) / len(runs)
            ax.scatter(runs, [y] * len(runs), s=26, color=color, alpha=0.55, linewidths=0, zorder=3)
            ax.plot([average, average], [y - 0.32, y + 0.32], color=color, linewidth=2.5,
                    solid_capstyle='round', zorder=4)
            # selective label: the average only, right of the rightmost run
            ax.annotate(f'{average:,.0f}', xy=(max(runs), y), xytext=(6, 0), textcoords='offset points',
                        va='center', fontsize=8, color=TEXT_SECONDARY)
        ax.set_yticks(range(len(methods)), [short_name(m) for m in methods])
        ax.invert_yaxis()
        ax.set_xlabel('Objective (lower is better)')
        ax.set_title(instance, loc='left', fontweight='bold')
        ax.margins(x=0.12)
        style_axis(ax, 'x')

    runs_per_method = len(next(iter(values[instances[0]].values())))
    fig.suptitle(f'Objective of each of the {runs_per_method} runs (dots) and the average (line)', fontsize=11)
    path = lab_dir / 'plots' / f'objective_runs.{plot_format}'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def main():
    # Example usage (from the repository root):
    # python src/utils/plotting/plot_ils.py data/lab_6 --no-show
    parser = argparse.ArgumentParser(description='Plot the per-run results of a time-limited experiment (lab 6, lab 7).')
    parser.add_argument('lab_dir', type=str, help='Lab data directory, e.g. data/lab_6')
    parser.add_argument('--no-show', action='store_true', help='Only save the plots, do not display them')
    parser.add_argument('--format', dest='plot_format', type=str, default='png',
                        help='Format of the saved plots (png, pdf, svg)')
    parser.add_argument('--methods', type=str, nargs='+', default=None,
                        help='Only these methods (default: all in runs.csv), shown in the order of runs.csv')
    args = parser.parse_args()
    lab_dir = Path(args.lab_dir)
    paths = [plot_objectives(lab_dir, args.plot_format, args.methods)]
    for path in paths:
        print(f'saved {path}')
    if not args.no_show:
        for path in paths:
            img = plt.imread(path)
            plt.figure()
            plt.imshow(img)
            plt.axis('off')
        plt.show()


if __name__ == '__main__':
    main()
