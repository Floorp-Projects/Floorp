#!/bin/bash
# This script builds NSS with gyp and ninja.
#
# This build system is still under development.  It does not yet support all
# the features or platforms that NSS supports.

set -e

# Usage info
show_help() {
cat << EOF

Usage: ${0##*/} [-hcgv] [-j <n>] [--test] [--fuzz] [--scan-build[=output]]
                [-m32] [--opt|-o]

This script builds NSS with gyp and ninja.

This build system is still under development.  It does not yet support all
the features or platforms that NSS supports.

NSS build tool options:

    -h            display this help and exit
    -c            clean before build
    -g            force a rebuild of gyp (and NSPR, because why not)
    -j <n>        run at most <n> concurrent jobs
    -v            verbose build
    -m32          do a 32-bit build on a 64-bit system
    --test        ignore map files and export everything we have
    --fuzz        enable fuzzing mode. this always enables test builds
    --scan-build  run the build with scan-build (scan-build has to be in the path)
                  --scan-build=/out/path sets the output path for scan-build
    --opt|-o      do an opt build
EOF
}

if [ -n "$CCC" ] && [ -z "$CXX" ]; then
    export CXX="$CCC"
fi

opt_build=0
build_64=0
clean=0
rebuild_gyp=0
target=Debug

# try to guess sensible defaults
arch=$(uname -m)
if [ "$arch" = "x86_64" -o "$arch" = "aarch64" ]; then
    build_64=1
fi

gyp_params=()
ninja_params=()
scanbuild=()
nspr_env=()

# parse command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -c) clean=1 ;;
        -g) rebuild_gyp=1 ;;
        -j) ninja_params+=(-j "$2"); shift ;;
        -v) ninja_params+=(-v) ;;
        --test) gyp_params+=(-Dtest_build=1) ;;
        --fuzz) gyp_params+=(-Dtest_build=1 -Dfuzz=1) ;;
        --scan-build) scanbuild=(scan-build) ;;
        --scan-build=?*) scanbuild=(scan-build -o "${1#*=}") ;;
        --opt|-o) opt_build=1; nspr_env+=(BUILD_OPT=1) ;;
        -m32|--m32) build_64=0 ;;
        *) show_help; exit ;;
    esac
    shift
done

# set paths
cwd=$(cd $(dirname $0); pwd -P)
obj_dir=$(USE_64=$build_64 make -s -C "$cwd" platform)
dist_dir="$cwd/../dist/$obj_dir"

# -c = clean first
if [ "$clean" = 1 ]; then
    rm -rf "$cwd/out"
    rm -rf "$cwd/../nspr/$obj_dir"
fi

if [ "$opt_build" = "1" ]; then
    target=Release
else
    target=Debug
fi
if [ "$build_64" == "1" ]; then
    target="${target}_x64"
    nspr_env+=(USE_64=1)
else
    gyp_params+=(-Dtarget_arch=ia32)
fi
target_dir="$cwd/out/$target"

# figure out the scan-build string
if [ "${#scanbuild[@]}" -gt 0 ]; then
    if [ -n "$CC" ]; then
       scanbuild+=(--use-cc="$CC")
    fi
    if [ -n "$CCC" ]; then
       scanbuild+=(--use-c++="$CCC")
    fi
 fi

# These steps can take a while, so don't overdo them.
# Force a redo with -g.
if [ "$rebuild_gyp" = 1 -o ! -d "$target_dir" ]; then
    # Build NSPR.
    make "${nspr_env[@]}" -C "$cwd" NSS_GYP=1 install_nspr

    # Run gyp.
    PKG_CONFIG_PATH="$cwd/../nspr/$obj_dir/config" \
        "${scanbuild[@]}" gyp -f ninja "${gyp_params[@]}" --depth="$cwd" \
          --generator-output="." "$cwd/nss.gyp"
fi

# Run ninja.
if which ninja >/dev/null 2>&1; then
    ninja=(ninja)
elif which ninja-build >/dev/null 2>&1; then
    ninja=(ninja-build)
else
    echo "Please install ninja" 1>&2
    exit 1
fi
"${scanbuild[@]}" $ninja -C "$target_dir" "${ninja_params[@]}"
