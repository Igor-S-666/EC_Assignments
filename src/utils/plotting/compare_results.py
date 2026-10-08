"""
For each instance of a lab, plots best solutions of all methods in one figure,
with a common node weight scale.
Output files are expected to be named <instance>_<method>.txt
"""

from file_readers import output_file_reader
from visualize_result import plot_result
from pathlib import Path
import matplotlib.pyplot as plt
import argparse
import math

def compare_results(lab_dir: str, methods: list[str] | None = None, show: bool = True,
                    plot_format: str = 'png') -> None:
    results = {}
    for output_file in sorted(Path(lab_dir, 'output').glob('*.txt')):
        instance, method = output_file.stem.split('_', 1)
        results[(instance, method)] = output_file_reader(str(output_file), 'txt')

    instances = sorted({instance for instance, _ in results})
    if methods is None:
        methods = sorted({method for _, method in results})

    # near-square grid, e.g. 2x2 for 4 methods
    ncols = math.ceil(math.sqrt(len(methods)))
    nrows = math.ceil(len(methods) / ncols)

    for instance in instances:
        # all methods of an instance share the same input data, hence the same weight scale
        input_data = results[(instance, methods[0])][2]
        weights = [node[2] for node in input_data]
        vmin, vmax = min(weights), max(weights)

        # match the figure to the instance's width/height ratio, so there is no empty space between plots
        x_coords = [node[0] for node in input_data]
        y_coords = [node[1] for node in input_data]
        aspect = (max(x_coords) - min(x_coords)) / (max(y_coords) - min(y_coords))
        fig, axes = plt.subplots(nrows, ncols, squeeze=False,
                                 figsize=(6 * ncols + 1.5, 6 * nrows / aspect + 1.5), constrained_layout=True)
        for ax, method in zip(axes.flat, methods):
            node_path, value, input_data = results[(instance, method)]
            nodes = plot_result(ax, node_path, input_data, vmin, vmax)
            ax.set_title(f'{method}\ntotal_weight: {value}')
            ax.set_xticks([])
            ax.set_yticks([])
        for ax in axes.flat[len(methods):]:
            ax.axis('off')

        fig.suptitle(instance)
        fig.colorbar(nodes, ax=axes, label='Node weight', shrink=0.8)
        fig.legend(*axes.flat[0].get_legend_handles_labels(), loc='outside lower center', ncols=3)

        plot_path = Path(lab_dir, 'plots', f'comparison_{instance}.{plot_format}')
        plot_path.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(plot_path, dpi=150)
    if show:
        plt.show()

def main():
    # Example usage (from the repository root):
    # python src/utils/plotting/compare_results.py data/lab_1 --methods random nearest_endpoint nearest_anypoint greedy_cycle
    parser = argparse.ArgumentParser(description='Plot best solutions of all methods, one figure per instance of a lab.')
    parser.add_argument('lab_dir', type=str, help='Lab data directory, e.g. data/lab_1')
    parser.add_argument('--methods', type=str, nargs='+', default=None,
                        help='Methods in display order, all found by default')
    parser.add_argument('--no-show', action='store_true', help='Only save the plots, do not display them')
    parser.add_argument('--format', dest='plot_format', type=str, default='png',
                        help='Format of the saved plots (png, pdf, svg)')
    args = parser.parse_args()
    compare_results(args.lab_dir, args.methods, show=not args.no_show, plot_format=args.plot_format)

if __name__ == '__main__':
    main()
