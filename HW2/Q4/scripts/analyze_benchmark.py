#!/usr/bin/env python3
"""Analyze Q4 triangle-counting benchmark CSV results."""

import argparse
import csv
import html
import math
from pathlib import Path

REQUIRED_COLUMNS = {
    "graph",
    "vertices",
    "edges",
    "processes",
    "sequential_seconds",
    "mpi_seconds",
    "speedup",
    "efficiency",
    "triangles",
}
DETAIL_COLUMNS = [
    "graph",
    "vertices",
    "edges",
    "processes",
    "sequential_seconds",
    "mpi_seconds",
    "speedup",
    "efficiency",
    "triangles",
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
                        "graph": row["graph"],
                        "vertices": int(row["vertices"]),
                        "edges": int(row["edges"]),
                        "processes": int(row["processes"]),
                        "sequential_seconds": float(row["sequential_seconds"]),
                        "mpi_seconds": float(row["mpi_seconds"]),
                        "speedup": float(row["speedup"]),
                        "efficiency": float(row["efficiency"]),
                        "triangles": int(row["triangles"]),
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
        groups.setdefault(str(row["graph"]), []).append(row)

    summaries = []
    for graph, graph_rows in groups.items():
        triangle_counts = {row["triangles"] for row in graph_rows}
        if len(triangle_counts) != 1:
            raise ValueError(f"Triangle count mismatch for {graph}: {sorted(triangle_counts)}")

        best_speedup = max(graph_rows, key=lambda row: float(row["speedup"]))
        best_efficiency = max(graph_rows, key=lambda row: float(row["efficiency"]))
        fastest_mpi = min(graph_rows, key=lambda row: float(row["mpi_seconds"]))
        first_row = graph_rows[0]
        summaries.append(
            {
                "graph": graph,
                "vertices": first_row["vertices"],
                "edges": first_row["edges"],
                "triangles": first_row["triangles"],
                "sequential_seconds": first_row["sequential_seconds"],
                "fastest_mpi_processes": fastest_mpi["processes"],
                "fastest_mpi_seconds": fastest_mpi["mpi_seconds"],
                "best_speedup_processes": best_speedup["processes"],
                "best_speedup": best_speedup["speedup"],
                "best_efficiency_processes": best_efficiency["processes"],
                "best_efficiency": best_efficiency["efficiency"],
            }
        )
    return summaries


def write_report(path: Path, input_path: Path, rows: list[dict[str, object]], summaries: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8") as report:
        report.write(f"Input: {input_path.name}\n")
        report.write(f"Benchmark rows: {len(rows)}\n")
        report.write(f"Graph cases: {len(summaries)}\n")
        report.write("Triangle counts are consistent across all process counts.\n\n")
        report.write("Graph | Triangles | Fastest MPI P | Fastest MPI seconds | Best speedup P | Best speedup | Best efficiency P | Best efficiency\n")
        for summary in summaries:
            report.write(
                f"{summary['graph']} | {summary['triangles']} | "
                f"{summary['fastest_mpi_processes']} | {summary['fastest_mpi_seconds']} | "
                f"{summary['best_speedup_processes']} | {summary['best_speedup']} | "
                f"{summary['best_efficiency_processes']} | {summary['best_efficiency']}\n"
            )


def write_plots(results_dir: Path, rows: list[dict[str, object]]) -> None:
    graph_names = list(dict.fromkeys(str(row["graph"]) for row in rows))
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

        colors = ["#1769aa", "#d1495b", "#2a9d8f", "#e09f3e", "#6c4f9d"]
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

        for graph in graph_names:
            graph_rows = [row for row in rows if row["graph"] == graph]
            points = [(x_position(int(row["processes"])), y_position(float(row[metric]))) for row in graph_rows]
            point_text = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
            color = colors[graph_names.index(graph) % len(colors)]
            svg.append(f'<polyline points="{point_text}" fill="none" stroke="{color}" stroke-width="3"/>')
            for x, y in points:
                svg.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="5" fill="{color}"/>')

        legend_x, legend_y = width - 220, top + 10
        for index, graph in enumerate(graph_names):
            color = colors[index % len(colors)]
            y = legend_y + index * 22
            svg.append(f'<line x1="{legend_x}" y1="{y}" x2="{legend_x + 18}" y2="{y}" stroke="{color}" stroke-width="3"/>')
            svg.append(f'<text x="{legend_x + 25}" y="{y + 5}" font-family="Arial" font-size="12" fill="#333">{html.escape(graph)}</text>')
        svg.append("</svg>")
        (results_dir / filename).write_text("\n".join(svg), encoding="utf-8")

    plot_metric("speedup", "Q4 MPI Speedup", "Speedup", "speedup_plot.svg")
    plot_metric("efficiency", "Q4 MPI Efficiency", "Efficiency", "efficiency_plot.svg")
    plot_metric("mpi_seconds", "Q4 MPI Runtime", "Time (seconds)", "mpi_runtime_plot.svg")


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
