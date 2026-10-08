from file_readers import output_file_reader
from pathlib import Path
import matplotlib.pyplot as plt
import argparse

def plot_result(ax, node_path: list[int], input_data: list[tuple[int, int, int]],
                vmin: int | None = None, vmax: int | None = None):
    """
    Draws nodes (colored by weight) and the closed path on the given axes.
    Returns the scatter, so it can be used for a colorbar.
    """
    weights = [node[2] for node in input_data]
    if vmin is None:
        vmin = min(weights)
    if vmax is None:
        vmax = max(weights)

    # unselected nodes are faded, so the cycle stands out
    selected = set(node_path)
    unselected_nodes = [node for i, node in enumerate(input_data) if i not in selected]
    ax.scatter([node[0] for node in unselected_nodes], [node[1] for node in unselected_nodes],
               c=[node[2] for node in unselected_nodes], cmap='plasma', vmin=vmin, vmax=vmax,
               s=12, alpha=0.3, linewidths=0, label='Unselected nodes', zorder=2)
    selected_nodes = [input_data[node] for node in node_path]
    nodes = ax.scatter([node[0] for node in selected_nodes], [node[1] for node in selected_nodes],
                       c=[node[2] for node in selected_nodes], cmap='plasma', vmin=vmin, vmax=vmax,
                       s=30, edgecolors='white', linewidths=0.5, label='Selected nodes', zorder=3)

    # Plot the path, returning to the first node to close the cycle
    cycle = node_path + node_path[:1]
    path_x = [input_data[node][0] for node in cycle]
    path_y = [input_data[node][1] for node in cycle]
    ax.plot(path_x, path_y, color='black', lw=1, label='Path', zorder=1)
    ax.set_aspect('equal')
    return nodes

def visualize_result(output_file_path: str, output_file_type: str, show: bool = True,
                     plot_format: str = 'png') -> None:
    """
    Visualizes the result from the output file.

    Args:
        output_file_path (str): The path to the output file.
        output_file_type (str): The type of the output file.
        show (bool): Whether to display the plot (it is always saved).
        plot_format (str): Format of the saved plot (e.g. png, or pdf/svg for sharp plots in a report).
    """
    node_path, value, input_data = output_file_reader(output_file_path, output_file_type)

    fig, ax = plt.subplots(figsize=(8, 5), constrained_layout=True)
    nodes = plot_result(ax, node_path, input_data)
    fig.colorbar(nodes, ax=ax, label='Node weight')

    ax.set_title(f'Path Visualization (total_weight: {value})')
    ax.set_xlabel('X Coordinate')
    ax.set_ylabel('Y Coordinate')
    fig.legend(*ax.get_legend_handles_labels(), loc='outside lower center', ncols=3)

    # data/lab_X/output/<name>.txt -> data/lab_X/plots/<name>.<plot_format>
    output_path = Path(output_file_path)
    plot_path = output_path.parent.parent / 'plots' / f'{output_path.stem}.{plot_format}'
    plot_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(plot_path, dpi=150)
    if show:
        plt.show()
    plt.close(fig)

def main():
    # Example usage (from the repository root):
    # python src/utils/plotting/visualize_result.py data/lab_1/output/TSPA_greedy_cycle.txt
    parser = argparse.ArgumentParser(description='Visualize the result from the output file.')
    parser.add_argument('output_file_paths', type=str, nargs='+', help='Path(s) to the output file(s)')
    parser.add_argument('--type', dest='output_file_type', type=str,
                        default = 'txt',
                        help='Type of the output file (e.g., txt)')
    parser.add_argument('--no-show', action='store_true', help='Only save the plot, do not display it')
    parser.add_argument('--format', dest='plot_format', type=str, default='png',
                        help='Format of the saved plot (png, pdf, svg)')
    args = parser.parse_args()
    for output_file_path in args.output_file_paths:
        visualize_result(output_file_path, args.output_file_type, show=not args.no_show,
                         plot_format=args.plot_format)

if __name__ == '__main__':
    main()
