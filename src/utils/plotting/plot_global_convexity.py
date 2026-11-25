"""
Charts of the global convexity tests (lab 8).

Reads <lab_dir>/results/<instance>_local_optima.csv (written by src/lab_8/main.cpp) and saves
<lab_dir>/plots/global_convexity_<instance>.<format>: 6 charts per instance (3 versions of similarity x 2 measures),
objective on the x-axis, (average) similarity on the y-axis, with the Pearson correlation coefficient in the title.
The best of the 1000 local optima has an empty best1000 value, so it is left out of the "best of 1000" charts.
"""

from pathlib import Path
import argparse
import csv
import math

import matplotlib.pyplot as plt

SERIES_1 = '#2a78d6'
REFERENCE = '#6b6a64'
GRID_COLOR = '#e4e3dd'
TEXT_SECONDARY = '#3d3c37'

VERSIONS = [
    ('avg', 'Average similarity to the other local optima'),
    ('best1000', 'Similarity to the best of 1000 local optima'),
    ('reference', 'Similarity to the best ILS solution'),
]
MEASURES = [('edges', 'common edges'), ('nodes', 'common selected nodes')]


def pearson(x: list[float], y: list[float]) -> float:
    n = len(x)
    mx, my = sum(x) / n, sum(y) / n
    sxy = sum((a - mx) * (b - my) for a, b in zip(x, y))
    sxx = sum((a - mx) ** 2 for a in x)
    syy = sum((b - my) ** 2 for b in y)
    return sxy / math.sqrt(sxx * syy) if sxx and syy else float('nan')


def style_axis(ax) -> None:
    ax.grid(color=GRID_COLOR, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ('top', 'right'):
        ax.spines[side].set_visible(False)
    for side in ('left', 'bottom'):
        ax.spines[side].set_color(REFERENCE)
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=8)


def plot_instance(lab_dir: Path, instance: str, plot_format: str) -> Path:
    with open(lab_dir / 'results' / f'{instance}_local_optima.csv', newline='') as file:
        rows = list(csv.DictReader(file, delimiter=';'))

    fig, axes = plt.subplots(3, 2, figsize=(11, 12), constrained_layout=True)
    for row_axes, (version, version_title) in zip(axes, VERSIONS):
        for ax, (measure, measure_title) in zip(row_axes, MEASURES):
            column = f'{version}_{measure}'
            points = [(float(r['objective']), float(r[column])) for r in rows if r[column] != '']
            x, y = [p[0] for p in points], [p[1] for p in points]
            r = pearson(x, y)
            ax.scatter(x, y, s=10, color=SERIES_1, alpha=0.45, linewidths=0, zorder=3)
            # least-squares line, to show the direction of the correlation
            mx, my = sum(x) / len(x), sum(y) / len(y)
            slope = sum((a - mx) * (b - my) for a, b in zip(x, y)) / sum((a - mx) ** 2 for a in x)
            xs = [min(x), max(x)]
            ax.plot(xs, [my + slope * (v - mx) for v in xs], color=REFERENCE, linewidth=1.5, zorder=4)
            ax.set_title(f'{version_title},\n{measure_title}:  r = {r:.3f}  (n = {len(x)})', loc='left', fontsize=9)
            ax.set_xlabel('Objective (lower is better)', fontsize=8, color=TEXT_SECONDARY)
            ax.set_ylabel(('Average number of ' if version == 'avg' else 'Number of ') + measure_title, fontsize=8,
                          color=TEXT_SECONDARY)
            style_axis(ax)

    fig.suptitle(f'{instance}: similarity of 1000 local optima vs. their objective', fontsize=12, fontweight='bold')
    path = lab_dir / 'plots' / f'global_convexity_{instance}.{plot_format}'
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def main():
    # Example usage (from the repository root):
    # python src/utils/plotting/plot_global_convexity.py data/lab_8 --no-show
    parser = argparse.ArgumentParser(description='Plot the global convexity tests (lab 8).')
    parser.add_argument('lab_dir', type=str, help='Lab data directory, e.g. data/lab_8')
    parser.add_argument('--no-show', action='store_true', help='Only save the plots, do not display them')
    parser.add_argument('--format', dest='plot_format', type=str, default='png',
                        help='Format of the saved plots (png, pdf, svg)')
    args = parser.parse_args()
    lab_dir = Path(args.lab_dir)
    instances = sorted(p.name.split('_')[0] for p in (lab_dir / 'results').glob('*_local_optima.csv'))
    paths = [plot_instance(lab_dir, instance, args.plot_format) for instance in instances]
    for path in paths:
        print(f'saved {path}')
    if not args.no_show:
        for path in paths:
            plt.figure()
            plt.imshow(plt.imread(path))
            plt.axis('off')
        plt.show()


if __name__ == '__main__':
    main()
