#!/usr/bin/env bash
# This file is used by build.sh to setup sanitizers.

sanitizer_flags=""
sanitizers=()

# This tracks what sanitizers are enabled so they don't get enabled twice.  This
# means that doing things that enable the same sanitizer twice (such as enabling
# both --asan and --fuzz) is order-dependent: only the first is used.
enable_sanitizer()
{
    local san="$1"
    for i in "${sanitizers[@]}"; do
        [ "$san" = "$i" ] && return
    done
    sanitizers+=("$san")

    if [ -z "$sanitizer_flags" ]; then
        gyp_params+=(-Dno_zdefs=1)
    fi

    local cflags=$(python $cwd/coreconf/sanitizers.py "$@")
    sanitizer_flags="$sanitizer_flags $cflags"
}

enable_sancov()
{
    local clang_version=$($CC --version | grep -oE '([0-9]{1,}\.)+[0-9]{1,}')
    if [[ ${clang_version:0:1} -lt 4 && ${clang_version:0:1} -eq 3 && ${clang_version:2:1} -lt 9 ]]; then
        echo "Need at least clang-3.9 (better 4.0) for sancov." 1>&2
        exit 1
    fi

    local sancov
    if [ -n "$1" ]; then
        sancov="$1"
    elif [ "${clang_version:0:3}" = "3.9" ]; then
        sancov=edge,indirect-calls,8bit-counters
    else
        sancov=trace-pc-guard,trace-cmp
    fi
    enable_sanitizer sancov "$sancov"
}

enable_ubsan()
{
    local ubsan
    if [ -n "$1" ]; then
        ubsan="$1"
    else
        ubsan=bool,signed-integer-overflow,shift,vptr
    fi
    enable_sanitizer ubsan "$ubsan"
}

# Not strictly a sanitizer, but the pattern fits
scanbuild=()
enable_scanbuild()
{
    [ "${#scanbuild[@]}" -gt 0 ] && return

    scanbuild=(scan-build)
    if [ -n "$1" ]; then
        scanbuild+=(-o "$1")
    fi
    # pass on CC and CCC to scanbuild
    if [ -n "$CC" ]; then
        scanbuild+=(--use-cc="$CC")
    fi
    if [ -n "$CCC" ]; then
        scanbuild+=(--use-c++="$CCC")
    fi
}

run_scanbuild()
{
    "${scanbuild[@]}" "$@"
}
