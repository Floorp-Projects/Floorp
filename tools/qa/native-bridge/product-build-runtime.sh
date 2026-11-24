#!/usr/bin/env bash
# Release-equivalent (non-PGO) Runtime build at the locked commit, keeping the
# compile OBJDIR so the stock harness, manifests and fixtures come from it.
set -euo pipefail
RUNTIME="${1:?Runtime checkout at the locked commit}"
LOCK="${2:?floorp-runtime.lock.json}"
BUILD_DATE="${3:?MOZ_BUILD_DATE}"
commit=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["source"]["commit"])' "$LOCK")
tree=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["source"]["tree"])' "$LOCK")
cd "$RUNTIME"
[[ "$(git rev-parse HEAD)" == "$commit" && "$(git rev-parse 'HEAD^{tree}')" == "$tree" ]] || {
  echo "Runtime checkout does not match $LOCK" >&2; exit 1; }
git diff --quiet HEAD -- || { echo "Runtime checkout is not clean" >&2; exit 1; }
for patch in .github/patches/upstream/*.patch; do
  git apply --ignore-space-change --ignore-whitespace "$patch"
done
bash .github/workflows/scripts/setup-floorp.sh linux x86_64 false false "" "" "$BUILD_DATE"
if [[ -z "${ACTIONS_RESULTS_URL:-}" ]]; then
  sed -i "/^mk_add_options 'export SCCACHE_GHA_ENABLED=on'$/d" mozconfig
fi
grep -Eq '^[[:space:]]*ac_add_options[[:space:]]+--disable-tests' mozconfig && {
  echo "tests are disabled in the release mozconfig" >&2; exit 1; }
export MOZ_BUILD_DATE="$BUILD_DATE" LIBGL_ALWAYS_SOFTWARE=1
./mach configure
nice -n 10 ./mach build --jobs="$(nproc)"
./mach package
./mach package-tests
