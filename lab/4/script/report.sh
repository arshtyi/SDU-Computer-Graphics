#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

xmake f -m release -y
xmake
mkdir -p build report/image report/data
if xmake test > build/test.log 2>&1; then
    cat build/test.log
else
    cat build/test.log >&2
    exit 1
fi
for name in required warped quad pentagon fan3 ring; do
    xmake run CatmullClark --input "test/data/$name.obj" --levels 3 --output "build/results/$name"
    cp "build/results/$name/overview.png" "report/image/${name}_overview.png"
    cp "build/results/$name/stats.csv" "report/data/${name}_stats.csv"
    case "$name" in
        quad|pentagon|fan3|ring)
            cp "build/results/$name/level_0.png" "report/image/${name}_level0.png"
            cp "build/results/$name/level_3.png" "report/image/${name}_level3.png"
            ;;
    esac
done
cp build/results/required/level_1.png report/image/required_level1.png
