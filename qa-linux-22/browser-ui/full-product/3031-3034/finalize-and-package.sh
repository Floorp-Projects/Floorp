#!/bin/bash
set -euo pipefail
stage=/workspace/full-product-3031-3034
cd "$stage/noraneko"
dist="$stage/obj-artifact-build-output/dist"
target="$dist/bin"
env LD_LIBRARY_PATH="$stage/tools/sysroot/usr/lib/x86_64-linux-gnu" "$stage/tools/sysroot/usr/bin/rsync" -aL "$target/" "$dist/tmp__bin"
rm -rf "$target"
mv "$dist/tmp__bin" "$target"
for patch in tools/patches/*.patch;do
 test -e "$patch" || continue
 case "$patch" in *.windows.patch|*.darwin.patch) continue;;esac
 if git apply --reverse --directory "$target" --unsafe-paths --check "$patch" >/dev/null 2>&1;then
  echo "Already applied by Runtime: $patch"
 else
  git apply --directory "$target" --unsafe-paths --check "$patch"
  git apply --directory "$target" --unsafe-paths "$patch"
  echo "Applied frontend patch: $patch"
 fi
done
mkdir -p "$target/browser"
cp _dist/buildid2 "$target/browser/buildid2"
cd "$stage/runtime"
export PATH="/workspace/.floorp-cloud/tools/node_modules/.bin:/workspace/.floorp-cloud/go/bin:$PATH"
export MOZBUILD_STATE_PATH="$stage/mozbuild"
export REQUESTS_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt
export PIP_CERT=/etc/ssl/certs/ca-certificates.crt
./mach package
