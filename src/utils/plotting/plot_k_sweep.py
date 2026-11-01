"""
Plots the quality / time trade-off of the candidate list size K (lab 4).
Reads <lab_dir>/results/k_sweep.csv and, for the baseline reference lines,
<lab_dir>/results/statistics.csv. One figure, one column per instance, with
two rows sharing the K axis: average objective (top) and average time (bottom).
Saved as <lab_dir>/plots/k_sweep.<format>.
"""

from pathlib import Path
import argparse
import csv

import matplotlib.pyplot as plt

SERIES_COLOR = '#2a78d6'
BASELINE_COLOR = '#6b6a64'
GRID_COLOR = '#e4e3dd'
BASELINE_METHOD = 'steepest_edge_random_baseline'


def read_rows(path: Path) -> list[dict[str, str]]:
    with open(path, newline='') as file:
        return list(csv.DictReader(file, delimiter=';'))


def style_axis(ax) -> None:
    ax.grid(axis='y', color=GRID_COLOR, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ('top', 'right'):
        ax.spines[side].set_visible(False)
    for side in ('left', 'bottom'):
        ax.spines[side].set_color(BASELINE_COLOR)
    ax.tick_params(colors='#3d3c37')


def plot_panel(ax, ks, values, baseline, ylabel, clip_from_k=None, fmt='{:,.0f}'):
    ax.plot(ks, values, color=SERIES_COLOR, linewidth=2, marker='o', markersize=5,
            label='candidate moves')
    if baseline is not None:
        ax.axhline(baseline, color=BASELINE_COLOR, linewidth=1.5, linestyle='--')
        # label right of the plot area, so it never overlaps the data
        ax.annotate(f'baseline\n{fmt.format(baseline)}',
                    xy=(1, baseline), xycoords=('axes fraction', 'data'),
                    xytext=(6, 0), textcoords='offset points',
                    ha='left', va='center', color='#3d3c37', fontsize=9)

    # Very small K values are far off the scale; zoom on the rest and mark them.
    if clip_from_k is not None:
        shown = [v for k, v in zip(ks, values) if k >= clip_from_k]
        if baseline is not None:
            shown.append(baseline)
        low, high = min(shown), max(shown)
        margin = (high - low) * 0.15
        ax.set_ylim(low - margin, high + margin)
        top = high + margin
        off_scale = [f'K={k}: {fmt.format(v)}' for k, v in zip(ks, values) if v > top]
        if off_scale:
            ax.text(0.99, 0.97, 'off scale ↑  ' + ',  '.join(off_scale),
                    transform=ax.transAxes, ha='right', va='top',
                    color='#3d3c37', fontsize=9)

    ax.set_ylabel(ylabel)
    style_axis(ax)


def plot_k_sweep(lab_dir: str, show: bool = True, plot_format: str = 'png') -> None:
    results_dir = Path(lab_dir, 'results')
    sweep = read_rows(results_dir / 'k_sweep.csv')

    baselines = {}
    statistics_path = results_dir / 'statistics.csv'
    if statistics_path.exists():
        for row in read_rows(statistics_path):
            if row['method'] == BASELINE_METHOD:
                baselines[row['instance']] = (float(row['average']),
                                              float(row['avg_time(microsec)']) / 1000)

    # Columns: instances. Rows: quality (top) and time (bottom), sharing the K axis.
    instances = sorted({row['instance'] for row in sweep})
    fig, axes = plt.subplots(2, len(instances), sharex=True, squeeze=False,
                             figsize=(6 * len(instances), 6.5), constrained_layout=True)

    for column, instance in enumerate(instances):
        rows = sorted((r for r in sweep if r['instance'] == instance), key=lambda r: int(r['k']))
        ks = [int(r['k']) for r in rows]
        averages = [float(r['average']) for r in rows]
        times = [float(r['avg_time(microsec)']) / 1000 for r in rows]
        base_avg, base_time = baselines.get(instance, (None, None))

        ax_quality, ax_time = axes[0, column], axes[1, column]
        plot_panel(ax_quality, ks, averages, base_avg,
                   'Average objective' if column == 0 else '', clip_from_k=3)
        plot_panel(ax_time, ks, times, base_time,
                   'Average time per run [ms]' if column == 0 else '', fmt='{:.2f} ms')
        ax_quality.set_title(instance, fontweight='bold')
        ax_time.set_ylim(bottom=0)
        ax_time.set_xlabel('Candidate list size K')
        ax_time.set_xticks(ks[::2] if len(ks) > 10 else ks)

    fig.suptitle('Candidate list size K: average objective (lower is better) and time, 200 runs each.\n'
                 'Dashed line: baseline steepest local search with the full neighbourhood.')

    plot_path = Path(lab_dir, 'plots', f'k_sweep.{plot_format}')
    plot_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(plot_path, dpi=150)
    if show:
        plt.show()


def main():
    # Example usage (from the repository root):
    # python src/utils/plotting/plot_k_sweep.py data/lab_4 --no-show
    parser = argparse.ArgumentParser(description='Plot quality and time against candidate list size K.')
    parser.add_argument('lab_dir', type=str, help='Lab data directory, e.g. data/lab_4')
    parser.add_argument('--no-show', action='store_true', help='Only save the plots, do not display them')
    parser.add_argument('--format', dest='plot_format', type=str, default='png',
                        help='Format of the saved plots (png, pdf, svg)')
    args = parser.parse_args()
    plot_k_sweep(args.lab_dir, show=not args.no_show, plot_format=args.plot_format)


if __name__ == '__main__':
    main()
