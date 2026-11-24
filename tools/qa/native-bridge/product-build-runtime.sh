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
GITHUB_WORKSPACE="$PWD" bash .github/workflows/scripts/setup-floorp.sh linux x86_64 false false "" "" "$BUILD_DATE"
if [[ -z "${ACTIONS_RESULTS_URL:-}" ]]; then
  sed -i "/^mk_add_options 'export SCCACHE_GHA_ENABLED=on'$/d" mozconfig
fi
grep -Eq '^[[:space:]]*ac_add_options[[:space:]]+--disable-tests' mozconfig && {
  echo "tests are disabled in the release mozconfig" >&2; exit 1; }
export MOZ_BUILD_DATE="$BUILD_DATE" LIBGL_ALWAYS_SOFTWARE=1
./mach configure
nice -n 10 ./mach build --jobs="$(nproc)"
./mach package
# Stock package-tests also stages jsreftest and cppunittest suites this
# Runtime does not build; stage and archive only what the cohort consumes.
# One make run, as package-tests does: each separate run repeats
# install-test-files and make-stage-dir, and the repeat fails on symlinked
# support-file directories and wipes the earlier stage.
make -C obj-x86_64-pc-linux-gnu -j1 -s stage-config stage-mach stage-extensions stage-mochitest
name=$(basename obj-x86_64-pc-linux-gnu/dist/floorp-*.linux-x86_64.tar.xz .tar.xz)
for kind in common mochitest condprof; do
  ./mach python -m mozbuild.action.test_archive "$kind" \
    "$PWD/obj-x86_64-pc-linux-gnu/dist/$name.$kind.tests.tar.zst"
done
deleted=$(git ls-files --deleted)
[[ -z "$deleted" ]] || { echo "staging removed tracked source files: $deleted" >&2; exit 1; }
