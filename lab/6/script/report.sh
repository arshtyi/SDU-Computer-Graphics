#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

export_report() {
    for filename in config.json metrics.json history.csv; do
        cp "$1/$filename" "report/data/${2}_${filename}"
    done
    cp "$1/curves.png" "report/image/${2}_curves.png"
    uv run python - "$1/image" "report/image/${2}_comparison.png" <<'PY'
import sys
from pathlib import Path

from PIL import Image, ImageDraw

source, destination = map(Path, sys.argv[1:])
ground_truth = sorted(source.glob("*_gt.png"))
if not ground_truth:
    raise ValueError(f"No validation images in {source}")
tile, margin, label = 192, 12, 24
canvas = Image.new(
    "RGB", (3 * tile + 4 * margin, len(ground_truth) * (tile + label) + label), "white"
)
draw = ImageDraw.Draw(canvas)
for column, heading in enumerate(("Ground truth", "Prediction", "Absolute error x5")):
    draw.text((margin + column * (tile + margin), 6), heading, fill="black")
for row, path in enumerate(ground_truth):
    stem = path.name.removesuffix("_gt.png")
    for column, suffix in enumerate(("gt", "pred", "error")):
        with Image.open(source / f"{stem}_{suffix}.png") as image:
            resized = image.resize((tile, tile), Image.Resampling.NEAREST)
        canvas.paste(resized, (margin + column * (tile + margin), label + row * (tile + label)))
    draw.text((margin, label + row * (tile + label) + tile + 4), stem, fill="black")
canvas.save(destination)
PY
}

uv sync --locked
mkdir -p build report/data report/image
run_root=$(mktemp -d build/report-XXXXXX)
for scene in rabbit teapot; do
    uv run lab6 train --scene "$scene" --output "$run_root/$scene" \
        --steps 1200 --log-every 100 --size 64 --patch-size 8
    export_report "$run_root/$scene" "$scene"
    uv run lab6 train --scene "$scene" --output "$run_root/$scene-p4" \
        --steps 4000 --log-every 200 --size 64 --patch-size 4
    export_report "$run_root/$scene-p4" "$scene-p4"
done
