# Copyright 2025-, European Centre for Medium Range Weather Forecasts.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import re
import sys
from collections import defaultdict

import cartopy.crs as ccrs
import cartopy.feature as cfeature
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

# Regex for matching filename pattern
FILENAME_PATTERN = re.compile(r"ee_plugin_proc-(\d+)_step-(\d+)\.log")

# Regex for matching line content pattern
LINE_PATTERN = re.compile(
    r"\[EE Plume Plugin\] >>> event: (.*?); step: (.*?); param: (.*?); levtype: (.*?); levelist: (.*?); polygons: (\[.*\])"
)


def parse_line(line):
    """Parses a single line and returns a dictionary of extracted values."""
    match = LINE_PATTERN.match(line.strip())
    if not match:
        return None

    keys = ["event", "step", "param", "levtype", "levelist", "polygons"]
    return dict(zip(keys, match.groups()))


def decode_polygons(polygon_str):
    """Decode polygon string to list of (lat, lon) tuples."""
    try:
        polygon_str = polygon_str.strip()[2:-2]  # Remove surrounding [()]
        polygons = polygon_str.split("),(")
        coords = []
        for polygon in polygons:
            points = polygon.split(",")
            coords.append(list(zip(map(float, points[::2]), map(float, points[1::2]))))
        return coords
    except Exception as e:
        print(f"Failed to parse polygon: {polygon_str} ({e})")
        return []


def process_folder(folder_path, output_dir=".", verbose=False, dpi=600, gif=False):
    """
    Process a folder of log files from the Plume EE plugin
    and generate extreme event polygon plots and optional GIF.

    This function:
    - Finds all files matching the pattern `ee_plugin_proc-<N>_step-<M>.log`
    - Groups them by time step and extracts event data
    - Plots polygons per event on a Cartopy map and saves as PNG
    - Optionally creates a GIF animation of all plots

    Args:
        folder_path (str): Path to the input folder containing log files.
        output_dir (str): Directory to save output plots and GIF (default is current dir).
        verbose (bool): If True, prints progress messages.
        gif (bool): If True, generates a GIF from the saved plots.
    """
    files_by_step = defaultdict(list)

    # Step 1: Filter and group files by step
    for file in os.listdir(folder_path):
        match = FILENAME_PATTERN.fullmatch(file)
        if match:
            proc, step = int(match.group(1)), int(match.group(2))
            if 0 <= proc and 0 <= step:
                files_by_step[step].append(os.path.join(folder_path, file))

    # Step 2: Process files by step in ascending order
    for step in sorted(files_by_step.keys()):
        if verbose:
            print(f"\n=== Processing Step = {step} ===\n")

        # Collect parsed lines grouped by event
        events = defaultdict(list)
        for filepath in files_by_step[step]:
            if verbose:
                print(f"\n--- Reading file: {os.path.basename(filepath)} ---")
            with open(filepath, "r") as f:
                for line_num, line in enumerate(f, start=1):
                    parsed = parse_line(line)
                    if parsed:
                        if verbose:
                            print(f"\nLine {line_num}:")
                            for key, value in parsed.items():
                                print(f"  {key.capitalize()}: {value}")
                        polygon_coords = decode_polygons(parsed["polygons"])
                        if polygon_coords:
                            events[parsed["event"]].extend(polygon_coords)
                    elif verbose:
                        print(f"\nLine {line_num}: [Unrecognized format]")

        # Prepare map
        _, ax = plt.subplots(figsize=(12, 8), subplot_kw={"projection": ccrs.PlateCarree()})
        ax.set_global()
        ax.coastlines()
        ax.add_feature(cfeature.BORDERS)
        ax.add_feature(cfeature.LAND, facecolor="lightgray")
        ax.add_feature(cfeature.OCEAN, facecolor="lightblue")

        # Add lon/lat gridlines
        gl = ax.gridlines(draw_labels=True, linewidth=0.5, color="gray", alpha=0.5, linestyle="--")
        gl.top_labels = False
        gl.right_labels = False
        gl.xlabel_style = {"size": 8}
        gl.ylabel_style = {"size": 8}

        # Start plotting
        event_colors = {}
        legend_patches = []

        for i, (event_name, polygons) in enumerate(events.items()):
            color = f"C{i % 10}"  # use matplotlib's default color cycle
            event_colors[event_name] = color
            legend_patches.append(mpatches.Patch(color=color, label=event_name))

            for polygon in polygons:
                lats, lons = zip(*polygon)
                ax.plot(lons, lats, "-", color=color, transform=ccrs.PlateCarree())

        # Add title and legend
        ax.set_title(f"Events detected at step {step}", fontsize=14)
        ax.legend(
            handles=legend_patches,
            title="Event Types",
            loc="lower center",
            bbox_to_anchor=(0.5, -0.1),
            ncol=3,
            frameon=True,
        )

        # Save plot
        output_filename = os.path.join(output_dir, f"events_step-{step:04d}.png")
        plt.tight_layout()
        plt.savefig(output_filename, bbox_inches="tight", dpi=dpi)
        plt.close()
        if verbose:
            print(f"Saved plot to {output_filename}")

    # Step 3: Make a GIF out of the plots
    if gif:
        gif_filename = os.path.join(output_dir, "events_summary.gif")
        image_files = sorted(
            [
                f
                for f in os.listdir(output_dir)
                if f.startswith("events_step-") and f.endswith(".png")
            ]
        )

        if image_files:
            from PIL import Image

            if verbose:
                print(f"\nGenerating GIF from {len(image_files)} images...")

            frames = []
            for img_name in image_files:
                img_path = os.path.join(output_dir, img_name)
                frames.append(Image.open(img_path).convert("RGB"))

            # Save GIF
            frames[0].save(
                gif_filename,
                format="GIF",
                save_all=True,
                append_images=frames[1:],
                duration=1000,  # 1000ms per frame (1s)
                loop=0,
            )

            if verbose:
                print(f"GIF saved to {gif_filename}")
        else:
            if verbose:
                print("No step images found to create GIF.")

    if verbose:
        print("\n=== Done ===")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process log files and plot ee polygons on a map.")
    parser.add_argument("folder_path", help="Path to the folder containing .log files")
    parser.add_argument(
        "--output-dir",
        "-o",
        type=str,
        default=".",
        help="Directory where plots and GIF will be saved (default: current directory)",
    )
    parser.add_argument("--verbose", "-v", action="store_true", help="Enable verbose output")
    parser.add_argument("--dpi", type=int, default=600, help="DPI for saved images (default: 600)")
    parser.add_argument("--gif", action="store_true", help="Generate GIF from all step images")

    args = parser.parse_args()

    if not os.path.isdir(args.folder_path):
        print(f"Error: '{args.folder_path}' is not a valid directory.")
        sys.exit(1)

    if not os.path.isdir(args.output_dir):
        print(f"Error: '{args.output_dir}' is not a valid directory.")
        sys.exit(1)

    process_folder(
        args.folder_path,
        output_dir=args.output_dir,
        verbose=args.verbose,
        dpi=args.dpi,
        gif=args.gif,
    )
