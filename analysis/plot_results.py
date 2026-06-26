import argparse
import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import pandas as pd


def load_csv(path: Path) -> pd.DataFrame:
    return pd.read_csv(path)


def load_json(path: Path) -> pd.DataFrame:
    with open(path) as f:
        content = f.read()
    # Try normal parsing first
    try:
        data = json.loads(content.strip())
    except json.JSONDecodeError:
        # Find the last valid closing brace by scanning from the end
        # Count brace depth to find where the root object ends
        depth = 0
        end_pos = len(content)
        in_string = False
        escape = False
        for i in range(len(content) - 1, -1, -1):
            ch = content[i]
            if escape:
                escape = False
                continue
            if ch == "\\" and in_string:
                escape = True
                continue
            if ch == '"' :
                in_string = not in_string
                continue
            if in_string:
                continue
            if ch == "}":
                depth -= 1
                if depth == 0:
                    end_pos = i + 1
                    break
            elif ch == "{":
                depth += 1
        content = content[:end_pos]
        data = json.loads(content)
    if isinstance(data, dict) and "results" in data:
        return pd.DataFrame(data["results"])
    if isinstance(data, list):
        return pd.DataFrame(data)
    return pd.DataFrame([data])


def load_data(path: Path) -> pd.DataFrame:
    suffix = path.suffix.lower()
    if suffix == ".csv":
        return load_csv(path)
    if suffix == ".json":
        return load_json(path)
    print(f"Unsupported file format: {suffix}", file=sys.stderr)
    sys.exit(1)


def fmt_size(n: int) -> str:
    if n >= 1073741824:
        return f"{n / 1073741824:.1f}G"
    if n >= 1048576:
        return f"{n / 1048576:.0f}M"
    if n >= 1024:
        return f"{n / 1024:.0f}K"
    return str(n)


def setup_xaxis(ax, sizes):
    sizes = [float(s) for s in sizes]
    ax.set_xscale("log")
    ax.xaxis.set_major_locator(mticker.FixedLocator(sizes))
    ax.xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, p: fmt_size(int(x))))
    ax.tick_params(axis="x", labelsize=8, labelrotation=45)
    for label in ax.get_xticklabels():
        label.set_ha("right")


def convert_bandwidth(values: pd.Series, unit: str) -> pd.Series:
    if unit == "gbps":
        return values / 1000
    if unit == "mbs":
        return values / 8
    if unit == "gbs":
        return values / 8000
    return values


def bw_unit_label(unit: str) -> str:
    return {"mbps": "Bandwidth (Mbps)", "gbps": "Bandwidth (Gbps)", "mbs": "Bandwidth (MB/s)", "gbs": "Bandwidth (GB/s)"}[unit]


def plot_bandwidth(dfs: list[tuple[Path, pd.DataFrame]], output: Path, unit: str = "mbps") -> None:
    fig, ax = plt.subplots(figsize=(12, 6))

    all_sizes = []
    for label, df in dfs:
        sizes = df["message_size"]
        all_sizes.extend(sizes.tolist())
        mean_bw = convert_bandwidth(df["mean_bandwidth_mbps"], unit)
        min_bw = convert_bandwidth(df["min_bandwidth_mbps"], unit)
        max_bw = convert_bandwidth(df["max_bandwidth_mbps"], unit)
        ax.fill_between(sizes, min_bw, max_bw, alpha=0.1)
        ax.plot(sizes, mean_bw, marker="o", markersize=5, label=label)

    sizes = pd.Series(all_sizes)
    ax.set_xlabel("Message size")
    ax.set_ylabel(bw_unit_label(unit))
    ax.set_title("Bandwidth vs Message Size")
    ax.set_yscale("log")
    setup_xaxis(ax, sizes.drop_duplicates().sort_values().tolist())
    ax.legend()
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(output, dpi=150)
    plt.close(fig)
    print(f"Saved: {output}")


def plot_latency(dfs: list[tuple[Path, pd.DataFrame]], output: Path) -> None:
    fig, ax = plt.subplots(figsize=(12, 6))
    colors = plt.cm.tab10.colors

    all_sizes = []
    for i, (label, df) in enumerate(dfs):
        sizes = df["message_size"]
        all_sizes.extend(sizes.tolist())
        color = colors[i % len(colors)]
        for col, marker in [
            ("p50_latency_us", "s"),
            ("p95_latency_us", "^"),
            ("p99_latency_us", "d"),
            ("median_latency_us", "p"),
        ]:
            if col in df.columns:
                ax.plot(sizes, df[col], marker=marker, markersize=4, color=color, label=label)

    sizes = pd.Series(all_sizes)
    ax.set_xlabel("Message size")
    ax.set_ylabel("Latency (us)")
    ax.set_title("Latency Percentiles vs Message Size")
    ax.set_yscale("log")
    setup_xaxis(ax, sizes.drop_duplicates().sort_values().tolist())
    ax.legend()
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(output, dpi=150)
    plt.close(fig)
    print(f"Saved: {output}")


def plot_all(dfs: list[tuple[Path, pd.DataFrame]], out_dir: Path, unit: str = "mbps") -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    plot_bandwidth(dfs, out_dir / "bandwidth.png", unit)
    plot_latency(dfs, out_dir / "latency.png")

    # Combined single figure with subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10), sharex=True)
    colors = plt.cm.tab10.colors

    all_sizes = []
    for i, (label, df) in enumerate(dfs):
        sizes = df["message_size"]
        all_sizes.extend(sizes.tolist())
        color = colors[i % len(colors)]
        mean_bw = convert_bandwidth(df["mean_bandwidth_mbps"], unit)
        min_bw = convert_bandwidth(df["min_bandwidth_mbps"], unit)
        max_bw = convert_bandwidth(df["max_bandwidth_mbps"], unit)
        ax1.fill_between(sizes, min_bw, max_bw, alpha=0.1, color=color)
        ax1.plot(sizes, mean_bw, marker="o", markersize=5, color=color, label=label)

    sizes = pd.Series(all_sizes)
    ax1.set_ylabel(bw_unit_label(unit))
    ax1.set_title("Bandwidth vs Message Size")
    ax1.set_yscale("log")
    setup_xaxis(ax1, sizes.drop_duplicates().sort_values().tolist())
    ax1.legend()
    ax1.grid(True, which="both", alpha=0.3)

    for i, (label, df) in enumerate(dfs):
        sizes = df["message_size"]
        color = colors[i % len(colors)]
        for col, marker in [
            ("p50_latency_us", "s"),
            ("p95_latency_us", "^"),
            ("p99_latency_us", "d"),
            ("median_latency_us", "p"),
        ]:
            if col in df.columns:
                ax2.plot(sizes, df[col], marker=marker, markersize=4, color=color, label=label)

    ax2.set_xlabel("Message size")
    ax2.set_ylabel("Latency (us)")
    ax2.set_yscale("log")
    setup_xaxis(ax2, sizes.drop_duplicates().sort_values().tolist())
    ax2.legend()
    ax2.grid(True, which="both", alpha=0.3)

    fig.suptitle("Bandwidth Test Results", fontsize=14)
    fig.tight_layout()
    fig.savefig(out_dir / "combined.png", dpi=150)
    plt.close(fig)
    print(f"Saved: {out_dir / 'combined.png'}")


def main():
    parser = argparse.ArgumentParser(description="Plot bandwidth test results")
    parser.add_argument("inputs", nargs="+", type=Path, help="Input CSV or JSON file(s)")
    parser.add_argument("--output", "-o", type=Path, default=Path("output"), help="Output directory for plots")
    parser.add_argument("--bandwidth", "-b", action="store_true", help="Only plot bandwidth chart")
    parser.add_argument("--latency", "-l", action="store_true", help="Only plot latency chart")
    parser.add_argument("--unit", "-u", default="mbps", choices=["mbps", "gbps", "mbs", "gbs"],
                        help="Bandwidth unit: mbps (default), gbps, mbs (MB/s), gbs (GB/s)")
    args = parser.parse_args()

    dfs: list[tuple[Path, pd.DataFrame]] = []
    for input_path in args.inputs:
        if not input_path.exists():
            print(f"File not found: {input_path}", file=sys.stderr)
            sys.exit(1)
        df = load_data(input_path)
        label = input_path.name
        print(f"Loaded {len(df)} rows from {input_path}")
        dfs.append((label, df))

    if args.bandwidth and not args.latency:
        plot_bandwidth(dfs, args.output / "bandwidth.png", args.unit)
    elif args.latency and not args.bandwidth:
        plot_latency(dfs, args.output / "latency.png")
    else:
        plot_all(dfs, args.output, args.unit)


if __name__ == "__main__":
    main()
