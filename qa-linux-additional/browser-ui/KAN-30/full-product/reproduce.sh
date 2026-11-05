#!/bin/bash
set -euo pipefail
# Inputs are existing verified Runtime159 source/artifact and genuine generated XPT.
# Keep sufficient free space for an independent object directory and package staging.
qa=/workspace/qa-linux-additional/browser-ui/KAN-30/full-product
stage=/workspace/kan30-full
export PATH="/workspace/.floorp-cloud/tools/node_modules/.bin:$PATH"
export DENO_DIR=/workspace/.floorp-cloud/deno-cache DENO_TLS_CA_STORE=system
mkdir -p "$stage/runtime" "$stage/mozbuild/node"
cp "$qa/mozconfig" "$stage/mozconfig"
cd "$stage/noraneko"
test "$(git rev-parse HEAD)" = 1b8f3503df9e8cae6b08118ecbe2d85e2d5158e7
deno install --frozen --allow-scripts=npm:@swc/core
NODE_ENV=production deno task feles-build build --phase before-mach
unshare -Ur -m "$qa/build-package.sh"
unshare -Ur -m "$qa/finalize-package.sh"
python3 "$qa/audit.py"
