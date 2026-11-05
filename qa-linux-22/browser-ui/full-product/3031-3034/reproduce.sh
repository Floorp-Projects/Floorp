#!/bin/bash
set -euo pipefail
stage=/workspace/full-product-3031-3034
qa=/workspace/qa-linux-22/browser-ui/full-product/3031-3034
artifact=/workspace/qa-linux-22/browser-ui/full-product/shared/floorp-linux-x86_64-moz-artifact.tar.xz
export PATH="/workspace/.floorp-cloud/tools/node_modules/.bin:/workspace/.floorp-cloud/go/bin:$PATH"
export DENO_DIR=/workspace/.floorp-cloud/deno-cache
export DENO_TLS_CA_STORE=system
export MOZBUILD_STATE_PATH="$stage/mozbuild"
export REQUESTS_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt
export PIP_CERT=/etc/ssl/certs/ca-certificates.crt
export GOCACHE=/workspace/.floorp-cloud/go-cache
export GOMODCACHE=/workspace/.floorp-cloud/go-mod-cache
mkdir -p "$qa/logs"
printf '%s  %s\n' cb9b01283d5ce3167f4ba046ee7b39ae9d98a692e1beba73bd9f85f460157772 "$artifact" | sha256sum --check
cd "$stage/noraneko"
test "$(git rev-parse HEAD)" = 0cf1474cba7bc7c45693671dc1690323d85c911b
deno install --frozen --allow-scripts=npm:@swc/core > "$qa/logs/repro-install.log" 2>&1
NODE_ENV=production deno task feles-build build --phase before-mach > "$qa/logs/repro-before-mach.log" 2>&1
bash "$qa/prepare-runtime.sh" > "$qa/logs/repro-prepare-runtime.log" 2>&1
cd "$stage/runtime"
./mach configure > "$qa/logs/repro-configure.log" 2>&1
for patch in .github/patches/packaging/*.patch;do
 if git apply --reverse --check "$patch" >/dev/null 2>&1;then continue;fi
 git apply --check --ignore-space-change --ignore-whitespace "$patch"
 git apply --ignore-space-change --ignore-whitespace "$patch"
done
MOZ_ARTIFACT_FILE="$artifact" ./mach build > "$qa/logs/repro-mach-build.log" 2>&1
cd "$stage/noraneko"
CANON_PLATFORM=Linux-x86_64 NODE_ENV=production deno task feles-build build --phase after-mach > "$qa/logs/repro-after-mach.log" 2>&1
package_status=0
bash "$qa/finalize-and-package.sh" > "$qa/logs/repro-package.log" 2>&1 || package_status=$?
printf 'Strict package exit code: %s\n' "$package_status"
python3 "$qa/run-regression.py" 29722 > "$qa/logs/repro-returning.log" 2>&1
exit "$package_status"
