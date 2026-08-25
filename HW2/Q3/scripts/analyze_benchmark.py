#!/usr/bin/env python3
"""Analyze Q3 Bitonic Sort benchmark CSV results and generate plots & summaries."""

import argparse
import csv
import html
import math
from pathlib import Path

REQUIRED_COLUMNS = {
    "dataset",
    "elements",
    "processes",
    "sequential_seconds",
    "wall_seconds",
    "algo_seconds",
    "setup_seconds",
    "scatter_seconds",
    "initsort_seconds",
    "stagecomm_seconds",
    "stagecomp_seconds",
    "gather_seconds",
    "compute_seconds",
    "comm_seconds",
    "wall_speedup",
    "wall_efficiency",
    "algo_speedup",
    "algo_efficiency",
    "status",
}

DETAIL_COLUMNS = [
    "dataset",
    "elements",
    "processes",
    "sequential_seconds",
    "wall_seconds",
    "algo_seconds",
    "setup_seconds",
    "scatter_seconds",
    "initsort_seconds",
    "stagecomm_seconds",
    "stagecomp_seconds",
    "gather_seconds",
    "compute_seconds",
    "comm_seconds",
    "wall_speedup",
    "wall_efficiency",
    "algo_speedup",
    "algo_efficiency",
    "status",
]


def find_input(results_dir: Path, requested: str | None) -> Path:
    if requested:
        input_path = Path(requested)
        if not input_path.is_absolute():
            input_path = Path.cwd() / input_path
        if not input_path.is_file():
            raise FileNotFoundError(f"Input CSV not found: {input_path}")
        return input_path

    candidates = sorted(
        path
        for path in results_dir.glob("*.csv")
        if path.name not in {"analysis_detail.csv", "analysis_summary.csv"}
    )
    if not candidates:
        raise FileNotFoundError(f"No benchmark CSV files found in {results_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def read_rows(input_path: Path) -> list[dict[str, object]]:
    with input_path.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)
        columns = set(reader.fieldnames or [])
        missing = REQUIRED_COLUMNS - columns
        if missing:
            raise ValueError(f"Missing CSV columns: {', '.join(sorted(missing))}")

        rows = []
        for row_number, row in enumerate(reader, start=2):
            try:
                rows.append(
                    {
                        "dataset": row["dataset"],
                        "elements": int(row["elements"]),
                        "processes": int(row["processes"]),
                        "sequential_seconds": float(row["sequential_seconds"]),
                        "wall_seconds": float(row["wall_seconds"]),
                        "algo_seconds": float(row["algo_seconds"]),
                        "setup_seconds": float(row["setup_seconds"]),
                        "scatter_seconds": float(row["scatter_seconds"]),
                        "initsort_seconds": float(row["initsort_seconds"]),
                        "stagecomm_seconds": float(row["stagecomm_seconds"]),
                        "stagecomp_seconds": float(row["stagecomp_seconds"]),
                        "gather_seconds": float(row["gather_seconds"]),
                        "compute_seconds": float(row["compute_seconds"]),
                        "comm_seconds": float(row["comm_seconds"]),
                        "wall_speedup": float(row["wall_speedup"]),
                        "wall_efficiency": float(row["wall_efficiency"]),
                        "algo_speedup": float(row["algo_speedup"]),
                        "algo_efficiency": float(row["algo_efficiency"]),
                        "status": row["status"],
                    }
                )
            except (TypeError, ValueError) as error:
                raise ValueError(f"Invalid value on CSV row {row_number}: {error}") from error

    if not rows:
        raise ValueError(f"No benchmark rows found in {input_path}")
    return rows


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def analyze(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[str, list[dict[str, object]]] = {}
    for row in rows:
        groups.setdefault(str(row["dataset"]), []).append(row)

    summaries = []
    for dataset, dataset_rows in groups.items():
        statuses = {row["status"] for row in dataset_rows}
        if statuses != {"PASS"}:
            raise ValueError(f"Correctness verification failed for {dataset}: {statuses}")

        best_wall_speedup = max(dataset_rows, key=lambda row: float(row["wall_speedup"]))
        best_algo_speedup = max(dataset_rows, key=lambda row: float(row["algo_speedup"]))
        best_efficiency = max(dataset_rows, key=lambda row: float(row["wall_efficiency"]))
        fastest_mpi = min(dataset_rows, key=lambda row: float(row["wall_seconds"]))
        first_row = dataset_rows[0]
        summaries.append(
            {
                "dataset": dataset,
                "elements": first_row["elements"],
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


def write_report(path: Path, input_path: Path, rows: list[dict[str, object]], summaries: list[dict[str, object]]) -> None:
    largest_dataset = max(summaries, key=lambda summary: int(summary["elements"]))
    largest_rows = [row for row in rows if row["dataset"] == largest_dataset["dataset"]]
    largest_row = max(largest_rows, key=lambda row: int(row["processes"]))
    phase_names = {
        "setup_seconds": "setup and broadcast",
        "scatter_seconds": "Scatter",
        "initsort_seconds": "Initial local sort",
        "stagecomm_seconds": "Stage communication (Sendrecv)",
        "stagecomp_seconds": "Stage compare-exchange & local sorts",
        "gather_seconds": "Gather",
    }
    dominant_phase = max(phase_names, key=lambda phase: float(largest_row[phase]))
    with path.open("w", encoding="utf-8") as report:
        report.write(f"Input: {input_path.name}\n")
        report.write(f"Benchmark rows: {len(rows)}\n")
        report.write(f"Dataset cases: {len(summaries)}\n")
        report.write("All test cases passed sequential correctness verification.\n\n")
        report.write("Small datasets: wall-clock results include process spawn, MPI initialization, and teardown, which dominate the sort computation. Algorithm-only timings remove launcher overhead.\n")
        report.write(f"Largest dataset ({largest_dataset['elements']} elements): at P={largest_row['processes']}, dominant phase was {phase_names[dominant_phase]} ({largest_row[dominant_phase]} seconds).\n\n")
        report.write("Dataset | Elements | Fastest MPI P | Wall seconds | Best wall speedup P | Best wall speedup | Best algorithm speedup P | Best algorithm speedup | Best wall efficiency P | Best wall efficiency\n")
        for summary in summaries:
            report.write(
                f"{summary['dataset']} | {summary['elements']} | "
                f"{summary['fastest_mpi_processes']} | {summary['fastest_mpi_seconds']} | "
                f"{summary['best_wall_speedup_processes']} | {summary['best_wall_speedup']} | "
                f"{summary['best_algo_speedup_processes']} | {summary['best_algo_speedup']} | "
                f"{summary['best_efficiency_processes']} | {summary['best_efficiency']}\n"
            )


def write_plots(results_dir: Path, rows: list[dict[str, object]]) -> None:
    dataset_names = list(dict.fromkeys(str(row["dataset"]) for row in rows))
    process_counts = sorted({int(row["processes"]) for row in rows})

    def plot_metric(metric: str, title: str, ylabel: str, filename: str) -> None:
        width, height = 900, 540
        left, right, top, bottom = 85, 35, 55, 70
        plot_width = width - left - right
        plot_height = height - top - bottom
        all_values = [float(row[metric]) for row in rows]
        minimum = min(0.0, min(all_values))
        maximum = max(all_values)
        if math.isclose(minimum, maximum):
            maximum = minimum + 1.0

        def x_position(processes: int) -> float:
            if len(process_counts) == 1:
                return left + plot_width / 2
            index = process_counts.index(processes)
            return left + index * plot_width / (len(process_counts) - 1)

        def y_position(value: float) -> float:
            return top + (maximum - value) * plot_height / (maximum - minimum)

        colors = ["#1769aa", "#d1495b", "#2a9d8f", "#e09f3e", "#6c4f9d", "#0077b6", "#ef476f", "#06d6a0"]
        svg = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
            '<rect width="100%" height="100%" fill="#fbfaf7"/>',
            f'<text x="{width / 2}" y="32" text-anchor="middle" font-family="Arial" font-size="22" font-weight="bold" fill="#202124">{html.escape(title)}</text>',
        ]
        for tick in range(6):
            value = minimum + (maximum - minimum) * tick / 5
            y = y_position(value)
            svg.append(f'<line x1="{left}" y1="{y:.1f}" x2="{width - right}" y2="{y:.1f}" stroke="#d9d9d9"/>')
            svg.append(f'<text x="{left - 12}" y="{y + 5:.1f}" text-anchor="end" font-family="Arial" font-size="12" fill="#555">{value:.2f}</text>')
        svg.append(f'<line x1="{left}" y1="{top}" x2="{left}" y2="{height - bottom}" stroke="#555"/>')
        svg.append(f'<line x1="{left}" y1="{height - bottom}" x2="{width - right}" y2="{height - bottom}" stroke="#555"/>')
        for processes in process_counts:
            x = x_position(processes)
            svg.append(f'<text x="{x:.1f}" y="{height - bottom + 25}" text-anchor="middle" font-family="Arial" font-size="12" fill="#555">{processes}</text>')
        svg.append(f'<text x="{width / 2}" y="{height - 18}" text-anchor="middle" font-family="Arial" font-size="14" fill="#333">MPI processes</text>')
        svg.append(f'<text x="18" y="{height / 2}" text-anchor="middle" transform="rotate(-90 18 {height / 2})" font-family="Arial" font-size="14" fill="#333">{html.escape(ylabel)}</text>')

        for dataset in dataset_names:
            dataset_rows = [row for row in rows if row["dataset"] == dataset]
            points = [(x_position(int(row["processes"])), y_position(float(row[metric]))) for row in dataset_rows]
            point_text = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
            color = colors[dataset_names.index(dataset) % len(colors)]
            svg.append(f'<polyline points="{point_text}" fill="none" stroke="{color}" stroke-width="3"/>')
            for x, y in points:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="5" fill="{color}"/>')

        legend_x, legend_y = width - 220, top + 10
        for index, dataset in enumerate(dataset_names):
            color = colors[index % len(colors)]
            y = legend_y + index * 20
            svg.append(f'<line x1="{legend_x}" y1="{y}" x2="{legend_x + 18}" y2="{y}" stroke="{color}" stroke-width="3"/>')
            svg.append(f'<text x="{legend_x + 25}" y="{y + 5}" font-family="Arial" font-size="11" fill="#333">{html.escape(dataset)}</text>')
        svg.append("</svg>")
        (results_dir / filename).write_text("\n".join(svg), encoding="utf-8")

    plot_metric("wall_speedup", "Q3 MPI Wall Speedup", "Speedup", "speedup_plot.svg")
    plot_metric("algo_speedup", "Q3 MPI Algorithm Speedup", "Speedup", "algo_speedup_plot.svg")
    plot_metric("wall_efficiency", "Q3 MPI Wall Efficiency", "Efficiency", "efficiency_plot.svg")
    plot_metric("wall_seconds", "Q3 MPI Wall Runtime", "Time (seconds)", "mpi_runtime_plot.svg")


def main() -> None:
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

    print(f"Analyzed {len(rows)} rows from {input_path}")
    print(f"Wrote results to {args.results_dir}")


if __name__ == "__main__":
    main()
