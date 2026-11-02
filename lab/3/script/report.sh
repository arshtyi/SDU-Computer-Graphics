#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

image_dir=${1:-report/image}
xmake f -m release -y
xmake
mkdir -p "$image_dir"

for algorithm in casteljau bernstein both; do
    xmake run BezierCurve --input test/data/cubic.in --algorithm "$algorithm" \
        --output "$image_dir/cubic_$algorithm.png"
done
for name in s_curve six_points boundary; do
    xmake run BezierCurve --input "test/data/$name.in" --algorithm both \
        --output "$image_dir/${name}_both.png"
done
