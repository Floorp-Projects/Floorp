#!/usr/bin/env bash
# This script builds NSPR for NSS.
#
# This build system is still under development.  It does not yet support all
# the features or platforms that the regular NSPR build supports.

# variables
nspr_cflags=
nspr_cxxflags=
nspr_ldflags=

# Try to avoid bmake on OS X and BSD systems
if hash gmake 2>/dev/null; then
    make() { command gmake "$@"; }
fi

nspr_set_flags()
{
    nspr_cflags="$CFLAGS $@"
    nspr_cxxflags="$CXXFLAGS $@"
    nspr_ldflags="$LDFLAGS $@"
}

nspr_build()
{
    local nspr_dir="$cwd"/../nspr/$target
    mkdir -p "$nspr_dir"

    # These NSPR options are directory-specific, so they don't need to be
    # included in nspr_opt and changing them doesn't force a rebuild of NSPR.
    extra_params=(--prefix="$dist_dir"/$target)
    if [ "$opt_build" = 1 ]; then
        extra_params+=(--disable-debug --enable-optimize)
    fi
    if [ "$target_arch" = "x64" ]; then
        extra_params+=(--enable-64bit)
    fi

    echo "NSPR [1/5] configure ..."
    pushd "$nspr_dir" >/dev/null
    CFLAGS="$nspr_cflags" CXXFLAGS="$nspr_cxxflags" \
          LDFLAGS="$nspr_ldflags" CC="$CC" CXX="$CCC" \
          run_verbose ../configure "${extra_params[@]}" "$@"
    popd >/dev/null
    echo "NSPR [2/5] make ..."
    run_verbose make -C "$nspr_dir"

    if [ "$build_nspr_tests" = 1 ]; then
      echo "NSPR [3/5] build tests ..."
      run_verbose make -C "$nspr_dir/pr/tests"
    else
        echo "NSPR [3/5] NOT building tests"
    fi

    if [[ "$build_nspr_tests" = 1 && "$run_nspr_tests" = 1 ]]; then
      echo "NSPR [4/5] run tests ..."
      run_verbose make -C "$nspr_dir/pr/tests" runtests
    else
        echo "NSPR [4/5] NOT running tests"
    fi

    echo "NSPR [5/5] install ..."
    run_verbose make -C "$nspr_dir" install
}

nspr_clean()
{
    rm -rf "$cwd"/../nspr/$target
}

set_nspr_path()
{
    local include=$(echo "$1" | cut -d: -f1)
    local lib=$(echo "$1" | cut -d: -f2)
    gyp_params+=(-Dnspr_include_dir="$include")
    gyp_params+=(-Dnspr_lib_dir="$lib")
}
