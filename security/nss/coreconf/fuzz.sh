#!/usr/bin/env bash
# This file is used by build.sh to setup fuzzing.

set +e

# Default to clang if CC is not set.
if [ -z "$CC" ]; then
    if ! command -v clang &> /dev/null 2>&1; then
        echo "Fuzzing requires clang!"
        exit 1
    fi
    export CC=clang
    export CCC=clang++
    export CXX=clang++
fi

gyp_params+=(-Dtest_build=1 -Dstatic_libs=1 -Dfuzz=1 -Dsign_libs=0)

# Add debug symbols even for opt builds.
nspr_params+=(--enable-debug-symbols)

if [ "$fuzz_oss" = 1 ]; then
  gyp_params+=(-Dno_zdefs=1 -Dfuzz_oss=1)
else
  enable_sanitizer asan
  # Ubsan only builds on x64 for the moment.
  if [ "$target_arch" = "x64" ]; then
    enable_ubsan
  fi
  enable_sancov
fi

if [ "$fuzz_tls" = 1 ]; then
  gyp_params+=(-Dfuzz_tls=1)
fi

if [ ! -f "/usr/lib/libFuzzingEngine.a" ]; then
  echo "Cloning libFuzzer files ..."
  run_verbose "$cwd"/fuzz/config/clone_libfuzzer.sh
fi
