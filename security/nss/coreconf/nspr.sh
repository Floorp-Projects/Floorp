#!/usr/bin/env bash
# This script builds NSPR for NSS.
#
# This build system is still under development.  It does not yet support all
# the features or platforms that the regular NSPR build supports.

# variables
nspr_opt=()
nspr_cflags=
nspr_cxxflags=
nspr_ldflags=

# Try to avoid bmake on OS X and BSD systems
if hash gmake 2>/dev/null; then
    make() { command gmake "$@"; }
fi

nspr_sanitizer()
{
    nspr_cflags="$nspr_cflags $(python $cwd/coreconf/sanitizers.py $1 $2)"
    nspr_cxxflags="$nspr_cxxflags $(python $cwd/coreconf/sanitizers.py $1 $2)"
    nspr_ldflags="$nspr_ldflags $(python $cwd/coreconf/sanitizers.py $1 $2)"
}

verbose()
{
    CFLAGS=$nspr_cflags CXXFLAGS=$nspr_cxxflags LDFLAGS=$nspr_ldflags \
      CC=$CC CXX=$CCC ../configure "${nspr_opt[@]}" --prefix="$obj_dir"
    make -C "$cwd/../nspr/$target"
    make -C "$cwd/../nspr/$target" install
}

silent()
{
    echo "[1/3] configure NSPR ..."
    CFLAGS=$nspr_cflags CXXFLAGS=$nspr_cxxflags LDFLAGS=$nspr_ldflags \
      CC=$CC CXX=$CCC ../configure "${nspr_opt[@]}" --prefix="$obj_dir" 1> /dev/null
    echo "[2/3] make NSPR ..."
    make -C "$cwd/../nspr/$target" 1> /dev/null
    echo "[3/3] install NSPR ..."
    make -C "$cwd/../nspr/$target" install 1> /dev/null
}

build_nspr()
{
    mkdir -p "$cwd/../nspr/$target"
    cd "$cwd/../nspr/$target"
    if [ "$1" == 1 ]; then
        verbose
    else
        silent
    fi
}
