
import os
import pandas as pd
import matplotlib.pyplot as plt


def plot_global_convexity(
    data_dir="data/global_convexity",
    output_dir="data/global_convexity/plots"
):
    os.makedirs(output_dir, exist_ok=True)

    # The six similarity measures written by the C++ program.
    plot_definitions = [
        (
            "average_common_edges",
            "Average similarity",
            "Common edges"
        ),
        (
            "average_common_nodes",
            "Average similarity",
            "Common nodes"
        ),
        (
            "best1000_common_edges",
            "Similarity to best of 1000",
            "Common edges"
        ),
        (
            "best1000_common_nodes",
            "Similarity to best of 1000",
            "Common nodes"
        ),
        (
            "best_method_common_edges",
            "Similarity to reference solution",
            "Common edges"
        ),
        (
            "best_method_common_nodes",
            "Similarity to reference solution",
            "Common nodes"
        )
    ]

    instances = ["TSPA", "TSPB"]

    for instance in instances:
        filename = os.path.join(
            data_dir,
            f"{instance}_local_optima.csv"
        )

        if not os.path.isfile(filename):
            print(f"File not found: {filename}")
            continue

        # Read the local optima data.
        df = pd.read_csv(filename, sep=";")

        # Check that the expected columns exist.
        required_columns = [
            "objective"
        ] + [
            definition[0] for definition in plot_definitions
        ]

        missing_columns = [
            column for column in required_columns
            if column not in df.columns
        ]

        if missing_columns:
            print(f"Missing columns in {filename}:")
            print(missing_columns)
            print("Available columns:")
            print(df.columns.tolist())
            continue

        # Create a figure with six subplots (3 rows x 2 columns).
        fig, axes = plt.subplots(
            3,
            2,
            figsize=(14, 15)
        )

        fig.suptitle(
            f"Global Convexity - {instance}",
            fontsize=18,
            fontweight="bold"
        )

        for i, (column, similarity_title, measure_title) in enumerate(
            plot_definitions
        ):
            ax = axes[i // 2, i % 2]

            # Remove missing values.
            # This also excludes the best local optimum from the
            # best-of-1000 similarity plots, as intended.
            plot_data = df[
                ["objective", column]
            ].dropna()

            x = plot_data["objective"]
            y = plot_data[column]

            # Scatter plot.
            ax.scatter(
                x,
                y,
                alpha=0.6,
                s=20
            )

            # Pearson correlation.
            correlation = x.corr(y)

            ax.set_title(
                f"{similarity_title}\n{measure_title}",
                fontsize=11
            )

            ax.set_xlabel("Objective value")
            ax.set_ylabel("Similarity")

            ax.grid(
                True,
                alpha=0.3
            )

            # Display Pearson correlation on the plot.
            if pd.notna(correlation):
                correlation_text = f"Pearson r = {correlation:.4f}"
            else:
                correlation_text = "Pearson r = undefined"

            ax.text(
                0.05,
                0.95,
                correlation_text,
                transform=ax.transAxes,
                verticalalignment="top",
                bbox=dict(
                    boxstyle="round",
                    facecolor="white",
                    alpha=0.8
                )
            )

        plt.tight_layout(
            rect=[0, 0, 1, 0.96]
        )

        # Save the figure.
        output_file = os.path.join(
            output_dir,
            f"{instance}_global_convexity.png"
        )

        plt.savefig(
            output_file,
            dpi=300,
            bbox_inches="tight"
        )

        plt.show()
        plt.close(fig)

        print(f"Saved: {output_file}")


if __name__ == "__main__":
    plot_global_convexity()
