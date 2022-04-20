#!/usr/bin/env bash
#
# Runs all benchmarks. Controlled by the following environment variables:
#
# FEATURES={} - cargo features to pass to all benchmarks (e.g. core_arch,sleef-sys,ispc)
# NORUN={1}   - only builds the benchmarks

set -ex

if [[ ${NORUN} != 1 ]]; then
    # Most benchmarks require hyperfine; require it upfront.
    hash hyperfine 2>/dev/null || { echo >&2 "hyperfine is not in PATH."; exit 1; }
fi


# If the ispc benchmark feature is enabled, ispc must be in the path of the
# benchmarks. 
if echo "$FEATURES" | grep -q "ispc"; then
    hash ispc 2>/dev/null || { echo >&2 "ispc is not in PATH."; exit 1; }
fi

# An example with a benchmark.sh is a benchmark:
for dir in examples/*/
do
    dir=${dir%*/}
    cd ${dir%*/}
    if [ -f "benchmark.sh" ]; then
        ./benchmark.sh
    fi
    cd -
done

