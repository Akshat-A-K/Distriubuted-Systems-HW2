#!/usr/bin/env python3
"""Analyze Q8 Weather Analytics benchmark CSV results and generate plots & summaries."""

import argparse
import csv
import html
import math
from pathlib import Path
from typing import Dict, List, Optional

REQUIRED_COLUMNS = {
    "dataset",
    "records",
    "processes",
    "sequential_seconds",
    "wall_seconds",
    "algo_seconds",
    "setup_seconds",
    "scatter_seconds",
    "compute_seconds",
    "comm_seconds",
    "merge_seconds",
    "wall_speedup",
    "wall_efficiency",
    "algo_speedup",
    "algo_efficiency",
    "status",
}

DETAIL_COLUMNS = [
    "dataset",
    "records",
    "processes",
    "sequential_seconds",
    "wall_seconds",
    "algo_seconds",
    "setup_seconds",
    "scatter_seconds",
    "compute_seconds",
    "comm_seconds",
    "merge_seconds",
    "wall_speedup",
    "wall_efficiency",
    "algo_speedup",
    "algo_efficiency",
    "status",
]


def find_input(results_dir, requested):
    # type: (Path, Optional[str]) -> Path
    if requested:
        input_path = Path(requested)
        if not input_path.is_absolute():
            input_path = Path.cwd() / input_path
        if not input_path.is_file():
            raise FileNotFoundError("Input CSV not found: {}".format(input_path))
        return input_path

    candidates = sorted(
        path
        for path in results_dir.glob("*.csv")
        if path.name not in {"analysis_detail.csv", "analysis_summary.csv"}
    )
    if not candidates:
        raise FileNotFoundError("No benchmark CSV files found in {}".format(results_dir))
    return max(candidates, key=lambda path: path.stat().st_mtime)


def read_rows(input_path):
    # type: (Path) -> List[Dict[str, object]]
    with input_path.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)
        columns = set(reader.fieldnames or [])
        missing = REQUIRED_COLUMNS - columns
        if missing:
            raise ValueError("Missing CSV columns: {}".format(", ".join(sorted(missing))))

        rows = []
        for row_number, row in enumerate(reader, start=2):
            try:
                rows.append(
                    {
                        "dataset": row["dataset"],
                        "records": int(row["records"]),
                        "processes": int(row["processes"]),
                        "sequential_seconds": float(row["sequential_seconds"]),
                        "wall_seconds": float(row["wall_seconds"]),
                        "algo_seconds": float(row["algo_seconds"]),
                        "setup_seconds": float(row["setup_seconds"]),
                        "scatter_seconds": float(row["scatter_seconds"]),
                        "compute_seconds": float(row["compute_seconds"]),
                        "comm_seconds": float(row["comm_seconds"]),
                        "merge_seconds": float(row["merge_seconds"]),
                        "wall_speedup": float(row["wall_speedup"]),
                        "wall_efficiency": float(row["wall_efficiency"]),
                        "algo_speedup": float(row["algo_speedup"]),
                        "algo_efficiency": float(row["algo_efficiency"]),
                        "status": row["status"],
                    }
                )
            except (TypeError, ValueError) as error:
                raise ValueError("Invalid value on CSV row {}: {}".format(row_number, error))

    if not rows:
        raise ValueError("No benchmark rows found in {}".format(input_path))
    return rows


def write_csv(path, fieldnames, rows):
    # type: (Path, List[str], List[Dict[str, object]]) -> None
    with path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def analyze(rows):
    # type: (List[Dict[str, object]]) -> List[Dict[str, object]]
    groups = {}  # type: Dict[str, List[Dict[str, object]]]
    for row in rows:
        groups.setdefault(str(row["dataset"]), []).append(row)

    summaries = []
    for dataset, dataset_rows in groups.items():
        statuses = {row["status"] for row in dataset_rows}
        if statuses != {"PASS"}:
            raise ValueError("Correctness verification failed for {}: {}".format(dataset, statuses))

        best_wall_speedup = max(dataset_rows, key=lambda row: float(row["wall_speedup"]))
        best_algo_speedup = max(dataset_rows, key=lambda row: float(row["algo_speedup"]))
        best_efficiency = max(dataset_rows, key=lambda row: float(row["wall_efficiency"]))
        fastest_mpi = min(dataset_rows, key=lambda row: float(row["wall_seconds"]))
        first_row = dataset_rows[0]
        summaries.append(
            {
                "dataset": dataset,
                "records": first_row["records"],
                "sequential_seconds": first_row["sequential_seconds"],
                "fastest_mpi_processes": fastest_mpi["processes"],
                "fastest_mpi_seconds": fastest_mpi["wall_seconds"],
                "best_wall_speedup_processes": best_wall_speedup["processes"],
                "best_wall_speedup": best_wall_speedup["wall_speedup"],
                "best_algo_speedup_processes": best_algo_speedup["processes"],
                "best_algo_speedup": best_algo_speedup["algo_speedup"],
                "best_efficiency_processes": best_efficiency["processes"],
                "best_efficiency": best_efficiency["wall_efficiency"],
            }
        )
    return summaries


def write_report(path, input_path, rows, summaries):
    # type: (Path, Path, List[Dict[str, object]], List[Dict[str, object]]) -> None
    largest_dataset = max(summaries, key=lambda summary: int(summary["records"]))
    largest_rows = [row for row in rows if row["dataset"] == largest_dataset["dataset"]]
    largest_row = max(largest_rows, key=lambda row: int(row["processes"]))
    phase_names = {
        "setup_seconds": "setup and header broadcast",
        "scatter_seconds": "Scatterv records",
        "compute_seconds": "local metric aggregation",
        "comm_seconds": "gather/reduce partial metrics",
        "merge_seconds": "rank 0 final merge & sorting",
    }
    dominant_phase = max(phase_names, key=lambda phase: float(largest_row[phase]))
    with path.open("w", encoding="utf-8") as report:
        report.write("Input: {}\n".format(input_path.name))
        report.write("Benchmark rows: {}\n".format(len(rows)))
        report.write("Dataset cases: {}\n".format(len(summaries)))
        report.write("All test cases passed sequential correctness verification.\n\n")
        report.write("Small datasets: wall-clock results include process spawn, MPI initialization, and teardown, which dominate small computation.\n")
        report.write(
            "Largest dataset ({} records): at P={}, dominant phase was {} ({} seconds).\n\n".format(
                largest_dataset["records"], largest_row["processes"], phase_names[dominant_phase], largest_row[dominant_phase]
            )
        )
        report.write("Dataset | Records | Fastest MPI P | Wall seconds | Best wall speedup P | Best wall speedup | Best algorithm speedup P | Best algorithm speedup | Best wall efficiency P | Best wall efficiency\n")
        for summary in summaries:
            report.write(
                "{} | {} | {} | {} | {} | {} | {} | {} | {} | {}\n".format(
                    summary["dataset"], summary["records"],
                    summary["fastest_mpi_processes"], summary["fastest_mpi_seconds"],
                    summary["best_wall_speedup_processes"], summary["best_wall_speedup"],
                    summary["best_algo_speedup_processes"], summary["best_algo_speedup"],
                    summary["best_efficiency_processes"], summary["best_efficiency"],
                )
            )


def write_plots(results_dir, rows):
    # type: (Path, List[Dict[str, object]]) -> None
    dataset_names = list(dict.fromkeys(str(row["dataset"]) for row in rows))
    process_counts = sorted({int(row["processes"]) for row in rows})

    def plot_metric(metric, title, ylabel, filename):
        # type: (str, str, str, str) -> None
        width, height = 900, 540
        left, right, top, bottom = 85, 35, 55, 70
        plot_width = width - left - right
        plot_height = height - top - bottom
        all_values = [float(row[metric]) for row in rows]
        minimum = min(0.0, min(all_values))
        maximum = max(all_values)
        if math.isclose(minimum, maximum):
            maximum = minimum + 1.0

        def x_position(processes):
            # type: (int) -> float
            if len(process_counts) == 1:
                return left + plot_width / 2
            index = process_counts.index(processes)
            return left + index * plot_width / (len(process_counts) - 1)

        def y_position(value):
            # type: (float) -> float
            return top + (maximum - value) * plot_height / (maximum - minimum)

        colors = ["#1769aa", "#d1495b", "#2a9d8f", "#e09f3e", "#6c4f9d", "#0077b6", "#ef476f", "#06d6a0"]
        svg = [
            '<svg xmlns="http://www.w3.org/2000/svg" width="{0}" height="{1}" viewBox="0 0 {0} {1}">'.format(width, height),
            '<rect width="100%" height="100%" fill="#fbfaf7"/>',
            '<text x="{}" y="32" text-anchor="middle" font-family="Arial" font-size="22" font-weight="bold" fill="#202124">{}</text>'.format(width / 2, html.escape(title)),
        ]
        for tick in range(6):
            value = minimum + (maximum - minimum) * tick / 5
            y = y_position(value)
            svg.append('<line x1="{}" y1="{:.1f}" x2="{}" y2="{:.1f}" stroke="#d9d9d9"/>'.format(left, y, width - right, y))
            svg.append('<text x="{}" y="{:.1f}" text-anchor="end" font-family="Arial" font-size="12" fill="#555">{:.2f}</text>'.format(left - 12, y + 5, value))
        svg.append('<line x1="{0}" y1="{1}" x2="{0}" y2="{2}" stroke="#555"/>'.format(left, top, height - bottom))
        svg.append('<line x1="{0}" y1="{1}" x2="{2}" y2="{1}" stroke="#555"/>'.format(left, height - bottom, width - right))
        for processes in process_counts:
            x = x_position(processes)
            svg.append('<text x="{:.1f}" y="{}" text-anchor="middle" font-family="Arial" font-size="12" fill="#555">{}</text>'.format(x, height - bottom + 25, processes))
        svg.append('<text x="{}" y="{}" text-anchor="middle" font-family="Arial" font-size="14" fill="#333">MPI processes</text>'.format(width / 2, height - 18))
        svg.append('<text x="18" y="{0}" text-anchor="middle" transform="rotate(-90 18 {0})" font-family="Arial" font-size="14" fill="#333">{1}</text>'.format(height / 2, html.escape(ylabel)))

        for dataset in dataset_names:
            dataset_rows = [row for row in rows if row["dataset"] == dataset]
            points = [(x_position(int(row["processes"])), y_position(float(row[metric]))) for row in dataset_rows]
            point_text = " ".join("{:.1f},{:.1f}".format(x, y) for x, y in points)
            color = colors[dataset_names.index(dataset) % len(colors)]
            svg.append('<polyline points="{}" fill="none" stroke="{}" stroke-width="3"/>'.format(point_text, color))
            for x, y in points:
                svg.append('<circle cx="{:.1f}" cy="{:.1f}" r="5" fill="{}"/>'.format(x, y, color))

        legend_x, legend_y = width - 220, top + 10
        for index, dataset in enumerate(dataset_names):
            color = colors[index % len(colors)]
            y = legend_y + index * 20
            svg.append('<line x1="{0}" y1="{1}" x2="{2}" y2="{1}" stroke="{3}" stroke-width="3"/>'.format(legend_x, y, legend_x + 18, color))
            svg.append('<text x="{}" y="{}" font-family="Arial" font-size="11" fill="#333">{}</text>'.format(legend_x + 25, y + 5, html.escape(dataset)))
        svg.append("</svg>")
        (results_dir / filename).write_text("\n".join(svg), encoding="utf-8")

    plot_metric("wall_speedup", "Q8 MPI Wall Speedup", "Speedup", "speedup_plot.svg")
    plot_metric("algo_speedup", "Q8 MPI Algorithm Speedup", "Speedup", "algo_speedup_plot.svg")
    plot_metric("wall_efficiency", "Q8 MPI Wall Efficiency", "Efficiency", "efficiency_plot.svg")
    plot_metric("wall_seconds", "Q8 MPI Wall Runtime", "Time (seconds)", "mpi_runtime_plot.svg")


def main():
    # type: () -> None
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="?", help="Benchmark CSV; defaults to newest CSV in results")
    parser.add_argument(
        "--results-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "results",
        help="Directory for input and generated result files",
    )
    args = parser.parse_args()

    args.results_dir.mkdir(parents=True, exist_ok=True)
    input_path = find_input(args.results_dir, args.csv)
    rows = read_rows(input_path)
    summaries = analyze(rows)

    detail_columns = DETAIL_COLUMNS
    detail_rows = [{column: row[column] for column in detail_columns} for row in rows]
    write_csv(args.results_dir / "analysis_detail.csv", detail_columns, detail_rows)
    write_csv(args.results_dir / "analysis_summary.csv", list(summaries[0]), summaries)
    write_report(args.results_dir / "analysis_report.txt", input_path, rows, summaries)
    write_plots(args.results_dir, rows)

    print("Analyzed {} rows from {}".format(len(rows), input_path))
    print("Wrote results to {}".format(args.results_dir))


if __name__ == "__main__":
    main()
