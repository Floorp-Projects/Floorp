#!/bin/bash
set -euo pipefail
stage=/workspace/kan30-full
qa=/workspace/qa-linux-additional/browser-ui/KAN-30/full-product
mount --bind /workspace/full-product-3031-3034/runtime "$stage/runtime"
mount -o remount,bind,ro "$stage/runtime"
mount --bind /workspace/full-product-3031-3034/mozbuild/node "$stage/mozbuild/node"
mount -o remount,bind,ro "$stage/mozbuild/node"
export PATH="/workspace/.floorp-cloud/tools/node_modules/.bin:/workspace/.floorp-cloud/go/bin:$PATH"
export MOZBUILD_STATE_PATH="$stage/mozbuild" MOZCONFIG="$stage/mozconfig" GIT_OPTIONAL_LOCKS=0
python3 "$qa/flatten-product.py"
cd "$stage/noraneko"
target="$stage/obj-artifact-build-output/dist/bin"
for patch in tools/patches/*.patch;do
 case "$patch" in *.windows.patch|*.darwin.patch) continue;;esac
 if ! git apply --reverse --directory "$target" --unsafe-paths --check "$patch" >/dev/null 2>&1;then git apply --directory "$target" --unsafe-paths --check "$patch";git apply --directory "$target" --unsafe-paths "$patch";fi
done
cp _dist/buildid2 "$target/browser/buildid2"
cd "$stage/runtime"
./mach python --virtualenv build "$qa/compare-xpt.py" > "$qa/logs/xpt-comparison.log" 2>&1
mkdir -p "$stage/obj-artifact-build-output/dist/xpt_artifacts"
cp /workspace/qa-linux-22/browser-ui/full-product/xpt-generation/generated/xpt/*.xpt "$stage/obj-artifact-build-output/dist/xpt_artifacts/"
git show HEAD:toolkit/mozapps/installer/packager.mk | cmp - toolkit/mozapps/installer/packager.mk
./mach package > "$qa/logs/strict-package.log" 2>&1
