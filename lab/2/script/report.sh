#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

image_dir=${1:-report/image}
xmake f -m release -y
xmake
mkdir -p "$image_dir"

xmake run Rasterizer -r 0 "$image_dir/rotate_0.png"
xmake run Rasterizer -r 20 "$image_dir/rotate_20.png"
xmake run Rasterizer -r -20 "$image_dir/rotate_minus20.png"
xmake run Rasterizer -a 30 1 1 1 "$image_dir/axis_30_1_1_1.png"
