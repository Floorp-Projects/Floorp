#!/usr/bin/env bash
# This script builds NSS with gyp and ninja.
#
# This build system is still under development.  It does not yet support all
# the features or platforms that NSS supports.

set -e

cwd=$(cd $(dirname $0); pwd -P)
source "$cwd"/coreconf/nspr.sh
source "$cwd"/coreconf/sanitizers.sh

# Usage info
show_help()
{
    cat << EOF
Usage: ${0##*/} [-hcv] [-j <n>] [--nspr] [--gyp|-g] [--opt|-o] [-m32]
                [--test] [--fuzz] [--pprof] [--scan-build[=output]]
                [--asan] [--ubsan] [--msan] [--sancov[=edge|bb|func|...]]

This script builds NSS with gyp and ninja.

This build system is still under development.  It does not yet support all
the features or platforms that NSS supports.

NSS build tool options:

    -h               display this help and exit
    -c               clean before build
    -v               verbose build
    -j <n>           run at most <n> concurrent jobs
    --nspr           force a rebuild of NSPR
    --gyp|-g         force a rerun of gyp
    --opt|-o         do an opt build
    -m32             do a 32-bit build on a 64-bit system
    --test           ignore map files and export everything we have
    --fuzz           enable fuzzing mode. this always enables test builds
    --pprof          build with gperftool support
    --ct-verif       build with valgrind for ct-verif
    --scan-build     run the build with scan-build (scan-build has to be in the path)
                     --scan-build=/out/path sets the output path for scan-build
    --asan           do an asan build
    --ubsan          do an ubsan build
                     --ubsan=bool,shift,... sets specific UB sanitizers
    --msan           do an msan build
    --sancov         do sanitize coverage builds
                     --sancov=func sets coverage to function level for example
    --disable-tests  don't build tests and corresponding cmdline utils
EOF
}

run_verbose()
{
    if [ "$verbose" = 1 ]; then
        echo "$@"
        exec 3>&1
    else
        exec 3>/dev/null
    fi
    "$@" 1>&3 2>&3
    exec 3>&-
}

if [ -n "$CCC" ] && [ -z "$CXX" ]; then
    export CXX="$CCC"
fi

opt_build=0
build_64=0
clean=0
rebuild_gyp=0
rebuild_nspr=0
target=Debug
verbose=0
fuzz=0

gyp_params=(--depth="$cwd" --generator-output=".")
nspr_params=()
ninja_params=()

# try to guess sensible defaults
arch=$(python "$cwd"/coreconf/detect_host_arch.py)
if [ "$arch" = "x64" -o "$arch" = "aarch64" ]; then
    build_64=1
fi

# parse command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -c) clean=1 ;;
        --gyp|-g) rebuild_gyp=1 ;;
        --nspr) nspr_clean; rebuild_nspr=1 ;;
        -j) ninja_params+=(-j "$2"); shift ;;
        -v) ninja_params+=(-v); verbose=1 ;;
        --test) gyp_params+=(-Dtest_build=1) ;;
        --fuzz) fuzz=1 ;;
        --scan-build) enable_scanbuild  ;;
        --scan-build=?*) enable_scanbuild "${1#*=}" ;;
        --opt|-o) opt_build=1 ;;
        -m32|--m32) build_64=0 ;;
        --asan) enable_sanitizer asan ;;
        --msan) enable_sanitizer msan ;;
        --ubsan) enable_ubsan ;;
        --ubsan=?*) enable_ubsan "${1#*=}" ;;
        --sancov) enable_sancov ;;
        --sancov=?*) enable_sancov "${1#*=}" ;;
        --pprof) gyp_params+=(-Duse_pprof=1) ;;
        --ct-verif) gyp_params+=(-Dct_verif=1) ;;
        --disable-tests) gyp_params+=(-Ddisable_tests=1) ;;
        *) show_help; exit 2 ;;
    esac
    shift
done

if [ "$opt_build" = 1 ]; then
    target=Release
else
    target=Debug
fi
if [ "$build_64" = 1 ]; then
    nspr_params+=(--enable-64bit)
else
    gyp_params+=(-Dtarget_arch=ia32)
fi
if [ "$fuzz" = 1 ]; then
    source "$cwd"/coreconf/fuzz.sh
fi

# set paths
target_dir="$cwd"/out/$target
mkdir -p "$target_dir"
dist_dir="$cwd"/../dist
dist_dir=$(mkdir -p "$dist_dir"; cd "$dist_dir"; pwd -P)
gyp_params+=(-Dnss_dist_dir="$dist_dir")

# -c = clean first
if [ "$clean" = 1 ]; then
    nspr_clean
    rm -rf "$cwd"/out
    rm -rf "$dist_dir"
fi

# This saves a canonical representation of arguments that we are passing to gyp
# or the NSPR build so that we can work out if a rebuild is needed.
# Caveat: This can fail for arguments that are position-dependent.
# e.g., "-e 2 -f 1" and "-e 1 -f 2" canonicalize the same.
check_config()
{
    local newconf="$1".new oldconf="$1"
    shift
    mkdir -p $(dirname "$newconf")
    echo CC="$CC" >"$newconf"
    echo CCC="$CCC" >>"$newconf"
    for i in "$@"; do echo $i; done | sort >>"$newconf"

    # Note: The following diff fails if $oldconf isn't there as well, which
    # happens if we don't have a previous successful build.
    ! diff -q "$newconf" "$oldconf" >/dev/null 2>&1
}

gyp_config="$cwd"/out/gyp_config
nspr_config="$cwd"/out/$target/nspr_config

# If we don't have a build directory make sure that we rebuild.
if [ ! -d "$target_dir" ]; then
    rebuild_nspr=1
    rebuild_gyp=1
elif [ ! -d "$dist_dir"/$target ]; then
    rebuild_nspr=1
fi

if check_config "$nspr_config" "${nspr_params[@]}" \
                 nspr_cflags="$nspr_cflags" \
                 nspr_cxxflags="$nspr_cxxflags" \
                 nspr_ldflags="$nspr_ldflags"; then
    rebuild_nspr=1
fi

if check_config "$gyp_config" "${gyp_params[@]}"; then
    rebuild_gyp=1
fi

# save the chosen target
mkdir -p "$dist_dir"
echo $target > "$dist_dir"/latest

if [ "$rebuild_nspr" = 1 ]; then
    nspr_build "${nspr_params[@]}"
    mv -f "$nspr_config".new "$nspr_config"
fi
if [ "$rebuild_gyp" = 1 ]; then

    # These extra arguments aren't used in determining whether to rebuild.
    obj_dir="$dist_dir"/$target
    gyp_params+=(-Dnss_dist_obj_dir=$obj_dir)
    gyp_params+=(-Dnspr_lib_dir=$obj_dir/lib)
    gyp_params+=(-Dnspr_include_dir=$obj_dir/include/nspr)

    run_verbose run_scanbuild gyp -f ninja "${gyp_params[@]}" "$cwd"/nss.gyp

    mv -f "$gyp_config".new "$gyp_config"
fi

# Run ninja.
if hash ninja 2>/dev/null; then
    ninja=ninja
elif hash ninja-build 2>/dev/null; then
    ninja=ninja-build
else
    echo "Please install ninja" 1>&2
    exit 1
fi
run_scanbuild $ninja -C "$target_dir" "${ninja_params[@]}"
