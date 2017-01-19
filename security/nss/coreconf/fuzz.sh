#!/usr/bin/env bash
# This file is used by build.sh to setup fuzzing.

gyp_params+=(-Dtest_build=1 -Dfuzz=1)
enable_sanitizer asan
enable_ubsan
enable_sancov

# Add debug symbols even for opt builds.
nspr_params+=(--enable-debug-symbols)

echo "fuzz [1/2] Cloning libFuzzer files ..."
run_verbose "$cwd"/fuzz/clone_libfuzzer.sh

echo "fuzz [2/2] Cloning fuzzing corpus ..."
run_verbose  "$cwd"/fuzz/clone_corpus.sh
