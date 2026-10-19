#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

xmake f -m release -y
xmake
mkdir -p build report/image report/data

printf 'shader,triangles,covered_pixels\n' > report/data/render_stats.csv
for shader in normal phong texture bump bump-phong displacement-fragment displacement; do
    xmake run Rasterizer "report/image/spot_$shader.png" "$shader" \
        --stats "build/$shader.csv" --mask "build/${shader}_mask.png"
    awk -F, 'NR == 2 { print $1 "," $2 "," $5 }' "build/$shader.csv" >> report/data/render_stats.csv
done
cp build/phong_mask.png report/image/spot_phong_mask.png
cp build/displacement_mask.png report/image/spot_displacement_mask.png
for shader in normal texture bump bump-phong displacement-fragment; do
    cmp build/phong_mask.png "build/${shader}_mask.png"
done
if cmp -s build/phong_mask.png build/displacement_mask.png; then
    printf '%s\n' 'Geometric displacement did not change the coverage mask.' >&2
    exit 1
fi

for model in rock cube; do
    case "$model" in
        rock) texture=models/rock/rock.png; scale=2.5 ;;
        cube) texture=models/cube/wall1.tif; scale=1.8 ;;
    esac
    for shader in normal texture; do
        xmake run Rasterizer "report/image/${model}_$shader.png" "$shader" \
            --model "models/$model/$model.obj" --texture "$texture" --fit --angle 35 --scale "$scale"
    done
done

if command -v sips >/dev/null 2>&1; then
    sips -z 32 32 models/spot/spot_texture.png --out report/image/texture_32.png >/dev/null
elif command -v magick >/dev/null 2>&1; then
    magick models/spot/spot_texture.png -resize 32x32! report/image/texture_32.png
else
    printf '%s\n' 'Need sips (macOS) or ImageMagick to resize/crop report images.' >&2
    exit 1
fi
for filter in nearest bilinear; do
    xmake run Rasterizer "report/image/small_$filter.png" texture --filter "$filter" --texture report/image/texture_32.png
    if command -v sips >/dev/null 2>&1; then
        sips --cropToHeightWidth 240 240 --cropOffset 190 220 "report/image/small_$filter.png" \
            --out "report/image/small_${filter}_crop.png" >/dev/null
    else
        magick "report/image/small_$filter.png" -crop 240x240+220+190 +repage "report/image/small_${filter}_crop.png"
    fi
done
typst compile --root . report/main.typ report/main.pdf
