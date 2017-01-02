#!/bin/bash
# This script builds NSS with gyp and ninja.
#
# This build system is still under development.  It does not yet support all
# the features or platforms that NSS supports.

set -e

source $(dirname $0)/coreconf/nspr.sh

# Usage info
show_help() {
cat << EOF

Usage: ${0##*/} [-hcgv] [-j <n>] [--test] [--fuzz] [--scan-build[=output]]
                [-m32] [--opt|-o] [--asan] [--ubsan] [--sancov[=edge|bb|func]]
                [--pprof] [--msan]

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
    --asan        do an asan build
    --ubsan       do an ubsan build
    --msan        do an msan build
    --sancov      do sanitize coverage builds
                  --sancov=func sets coverage to function level for example
    --pprof       build with gperftool support
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
verbose=0
fuzz=0

# parse parameters to store in config
params=$(echo "$*" | perl -pe 's/-c|-v|-g|-j [0-9]*|-h//g' | perl -pe 's/^\s*(.*?)\s*$/\1/')
params=$(echo "$params $CC $CCC" | tr " " "\n" | perl -pe '/^\s*$/d')
params=$(echo "${params[*]}" | sort)

cwd=$(cd $(dirname $0); pwd -P)
dist_dir="$cwd/../dist"

# try to guess sensible defaults
arch=$(python "$cwd/coreconf/detect_host_arch.py")
if [ "$arch" = "x64" -o "$arch" = "aarch64" ]; then
    build_64=1
fi

gyp_params=()
ninja_params=()
scanbuild=()

enable_fuzz()
{
    fuzz=1
    nspr_sanitizer asan
    nspr_sanitizer ubsan
    nspr_sanitizer sancov edge
    gyp_params+=(-Duse_asan=1)
    gyp_params+=(-Duse_ubsan=1)
    gyp_params+=(-Duse_sancov=edge)

    # Adding debug symbols even for opt builds.
    nspr_opt+=(--enable-debug-symbols)
}

# parse command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -c) clean=1 ;;
        -g) rebuild_gyp=1 ;;
        -j) ninja_params+=(-j "$2"); shift ;;
        -v) ninja_params+=(-v); verbose=1 ;;
        --test) gyp_params+=(-Dtest_build=1) ;;
        --fuzz) gyp_params+=(-Dtest_build=1 -Dfuzz=1); enable_fuzz ;;
        --scan-build) scanbuild=(scan-build) ;;
        --scan-build=?*) scanbuild=(scan-build -o "${1#*=}") ;;
        --opt|-o) opt_build=1 ;;
        -m32|--m32) build_64=0 ;;
        --asan) gyp_params+=(-Duse_asan=1); nspr_sanitizer asan ;;
        --ubsan) gyp_params+=(-Duse_ubsan=1); nspr_sanitizer ubsan ;;
        --sancov) gyp_params+=(-Duse_sancov=edge); nspr_sanitizer sancov edge ;;
        --sancov=?*) gyp_params+=(-Duse_sancov="${1#*=}"); nspr_sanitizer sancov "${1#*=}" ;;
        --pprof) gyp_params+=(-Duse_pprof=1) ;;
        --msan) gyp_params+=(-Duse_msan=1); nspr_sanitizer msan ;;
        *) show_help; exit ;;
    esac
    shift
done

if [ "$opt_build" = "1" ]; then
    target=Release
    nspr_opt+=(--disable-debug --enable-optimize)
else
    target=Debug
fi
if [ "$build_64" == "1" ]; then
    nspr_opt+=(--enable-64bit)
else
    gyp_params+=(-Dtarget_arch=ia32)
    nspr_opt+=(--enable-x32)
fi

# clone fuzzing stuff
if [ "$fuzz" = "1" ]; then
    [ $verbose = 0 ] && exec 3>/dev/null || exec 3>&1

    echo "[1/2] Cloning libFuzzer files ..."
    $cwd/fuzz/clone_libfuzzer.sh 1>&3 2>&3

    echo "[2/2] Cloning fuzzing corpus ..."
    $cwd/fuzz/clone_corpus.sh 1>&3 2>&3

    exec 3>&-
fi

# check if we have to rebuild gyp
if [ "$params" != "$(cat $cwd/out/config 2>/dev/null)" -o "$rebuild_gyp" == 1 -o "$clean" == 1 ]; then
    rebuild_gyp=1
    rm -rf "$cwd/../nspr/$target" # force NSPR to rebuild
fi

# set paths
target_dir="$cwd/out/$target"

# get the realpath of $dist_dir
dist_dir=$(mkdir -p $dist_dir; cd $dist_dir; pwd -P)

# get object directory
obj_dir="$dist_dir/$target"
gyp_params+=(-Dnss_dist_dir=$dist_dir)
gyp_params+=(-Dnss_dist_obj_dir=$obj_dir)
gyp_params+=(-Dnspr_lib_dir=$obj_dir/lib)
gyp_params+=(-Dnspr_include_dir=$obj_dir/include/nspr)

# -c = clean first
if [ "$clean" = 1 ]; then
    rm -rf "$cwd/out"
    rm -rf "$cwd/../nspr/$target"
    rm -rf "$dist_dir"
fi

# save the chosen target
mkdir -p $dist_dir
echo $target > $dist_dir/latest

# pass on CC and CCC
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
    build_nspr $verbose

    # Run gyp.
    [ $verbose = 1 ] && set -v -x
    "${scanbuild[@]}" gyp -f ninja "${gyp_params[@]}" --depth="$cwd" \
      --generator-output="." "$cwd/nss.gyp"
    [ $verbose = 1 ] && set +v +x

    # Store used parameters for next run.
    echo "$params" > "$cwd/out/config"
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
