
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


REPO_ROOT = Path(__file__).resolve().parents[3]

DATA_DIR = REPO_ROOT / "data" / "global_convexity"
PLOTS_DIR = DATA_DIR / "plots"

INSTANCES = ["TSPA", "TSPB"]

SIMILARITY_TYPES = {
    "average_similarity": "Average similarity to other local optima",
    "best_1000": "Similarity to the best local optimum",
    "best_method": "Similarity to the reference solution",
}

MEASURES = {
    "common_edges": "Number of common edges",
    "common_nodes": "Number of common nodes",
}

SIMILARITY_COLUMNS = {
    ("average_similarity", "common_edges"):
        "average_common_edges",

    ("average_similarity", "common_nodes"):
        "average_common_nodes",

    ("best_1000", "common_edges"):
        "best1000_common_edges",

    ("best_1000", "common_nodes"):
        "best1000_common_nodes",

    ("best_method", "common_edges"):
        "best_method_common_edges",

    ("best_method", "common_nodes"):
        "best_method_common_nodes",
}

REQUIRED_COLUMNS = {
    "objective",
    "average_common_edges",
    "average_common_nodes",
    "best1000_common_edges",
    "best1000_common_nodes",
    "best_method_common_edges",
    "best_method_common_nodes",
}


def load_instance_data(instance):

    path = DATA_DIR / f"{instance}_local_optima.csv"

    if not path.is_file():
        raise FileNotFoundError(
            f"Experiment result not found: {path}\n"
            "Run global_convexity.exe first."
        )

    print(f"Loading: {path}")

    df = pd.read_csv(path, sep=";")

    missing = REQUIRED_COLUMNS - set(df.columns)

    if missing:
        raise ValueError(
            f"Missing columns in {path}:\n"
            f"{sorted(missing)}\n"
            f"Available columns: {list(df.columns)}"
        )

    for column in REQUIRED_COLUMNS:
        df[column] = pd.to_numeric(
            df[column],
            errors="coerce"
        )

    if df["objective"].isna().any():
        raise ValueError(
            f"Invalid objective values in {path}"
        )

    return df

def calculate_pearson_correlation(x, y):

    valid = x.notna() & y.notna()

    x = x[valid]
    y = y[valid]

    if len(x) < 2:
        return float("nan")

    if x.nunique() < 2 or y.nunique() < 2:
        return float("nan")

    return x.corr(y, method="pearson")


def create_plot(
    df,
    instance,
    similarity_type,
    measure
):

    similarity_column = SIMILARITY_COLUMNS[
        (similarity_type, measure)
    ]

    x = df["objective"]
    y = df[similarity_column]

    # Remove missing values.
    # The best local optimum is excluded from best_1000
    # because its similarity to itself is not meaningful.
    valid = x.notna() & y.notna()

    x = x[valid]
    y = y[valid]

    correlation = calculate_pearson_correlation(x, y)

    plt.figure(figsize=(10, 7))

    plt.scatter(
        x,
        y,
        alpha=0.65,
        s=25,
        edgecolors="none"
    )

    plt.xlabel(
        "Objective value",
        fontsize=12
    )

    plt.ylabel(
        MEASURES[measure],
        fontsize=12
    )

    plt.title(
        f"{instance}: {SIMILARITY_TYPES[similarity_type]}\n"
        f"{MEASURES[measure]}",
        fontsize=13
    )

    plt.grid(
        True,
        alpha=0.3
    )

    if pd.notna(correlation):
        correlation_text = (
            f"Pearson correlation: {correlation:.4f}\n"
            f"Samples: {len(x)}"
        )
    else:
        correlation_text = (
            f"Pearson correlation: undefined\n"
            f"Samples: {len(x)}"
        )

    plt.text(
        0.03,
        0.97,
        correlation_text,
        transform=plt.gca().transAxes,
        verticalalignment="top",
        bbox={
            "boxstyle": "round",
            "facecolor": "white",
            "alpha": 0.85
        }
    )

    plt.tight_layout()

    filename = (
        f"{instance}_{similarity_type}_{measure}.png"
    )

    output_path = PLOTS_DIR / filename

    plt.savefig(
        output_path,
        dpi=300,
        bbox_inches="tight"
    )

    plt.close()

    print(
        f"  Saved: {filename} "
        f"(r={correlation:.4f}, n={len(x)})"
    )

    return {
        "instance": instance,
        "similarity_type": similarity_type,
        "measure": measure,
        "sample_size": len(x),
        "pearson_r": correlation,
    }


def main():

    print("       GLOBAL CONVEXITY ANALYSIS")

    print(f"\nRepository root: {REPO_ROOT}")
    print(f"Data directory: {DATA_DIR}")

    if not DATA_DIR.is_dir():
        raise FileNotFoundError(
            f"Data directory does not exist: {DATA_DIR}\n"
            "Run global_convexity.exe first."
        )

    PLOTS_DIR.mkdir(
        parents=True,
        exist_ok=True
    )

    correlation_results = []

    # Process both TSP instances.
    for instance in INSTANCES:
        print(f"Processing {instance}")

        df = load_instance_data(instance)

        print(f"Loaded {len(df)} local optima.")

        # 3 similarity types x 2 measures = 6 plots.
        for similarity_type in SIMILARITY_TYPES:

            for measure in MEASURES:

                result = create_plot(
                    df=df,
                    instance=instance,
                    similarity_type=similarity_type,
                    measure=measure
                )

                correlation_results.append(result)


    correlations_df = pd.DataFrame(
        correlation_results
    )

    correlations_path = (
        PLOTS_DIR / "pearson_correlations.csv"
    )

    correlations_df.to_csv(
        correlations_path,
        sep=";",
        index=False,
        float_format="%.6f"
    )


    print("CORRELATIONS")

    print(
        correlations_df.to_string(
            index=False,
            float_format=lambda value: f"{value:.4f}"
        )
    )

    print("Analysis completed successfully!")

    print(f"\nPlots saved to: {PLOTS_DIR}")
    print(f"Correlations saved to: {correlations_path}")
    print(f"Total plots generated: {len(correlation_results)}")


if __name__ == "__main__":
    main()
