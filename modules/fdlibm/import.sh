#!/bin/sh

set -e

BASE_URL=https://raw.githubusercontent.com/freebsd/freebsd/"${1}"/lib/msun/src

download_source() {
    REMOTE_FILENAME=$1
    LOCAL_FILENAME=$2
    while true; do
        curl -o "src/${LOCAL_FILENAME}" "${BASE_URL}/${REMOTE_FILENAME}" && break
    done
}

mkdir -p src

# headers
download_source math.h fdlibm.h
download_source math_private.h math_private.h

# Math.acos
download_source e_acos.c e_acos.cpp

# Math.acosh
download_source e_acosh.c e_acosh.cpp

# Math.asin
download_source e_asin.c e_asin.cpp

# Math.asinh
download_source s_asinh.c s_asinh.cpp

# Math.atan
download_source s_atan.c s_atan.cpp

# Math.atanh
download_source e_atanh.c e_atanh.cpp

# Math.atan2
download_source e_atan2.c e_atan2.cpp

# Math.cbrt
download_source s_cbrt.c s_cbrt.cpp

# Math.ceil
download_source s_ceil.c s_ceil.cpp
download_source s_ceilf.c s_ceilf.cpp

# Math.cos (not used due to poor performance)

# Math.cosh
download_source e_cosh.c e_cosh.cpp

# Math.exp
download_source e_exp.c e_exp.cpp

# Math.expm1
download_source s_expm1.c s_expm1.cpp

# Math.floor and Math.round
download_source s_floor.c s_floor.cpp

# Math.fround
download_source s_floorf.c s_floorf.cpp

# Math.hypot
download_source e_hypot.c e_hypot.cpp

# Math.log
download_source e_log.c e_log.cpp

# Math.log1p
download_source s_log1p.c s_log1p.cpp

# Math.log10
download_source e_log10.c e_log10.cpp
download_source k_log.h k_log.h

# Math.log2
download_source e_log2.c e_log2.cpp

# Math.pow (not used due to poor performance)

# Math.sin (not used due to poor performance)

# Math.sinh
download_source e_sinh.c e_sinh.cpp

# Math.sqrt (not used due to poor performance)

# Math.tan (not used due to poor performance)

# Math.tanh
download_source s_tanh.c s_tanh.cpp

# Math.trunc
download_source s_trunc.c s_trunc.cpp
download_source s_truncf.c s_truncf.cpp

# dependencies
download_source k_exp.c k_exp.cpp
download_source s_copysign.c s_copysign.cpp
download_source s_fabs.c s_fabs.cpp
download_source s_scalbn.c s_scalbn.cpp

# These are not not used in Math.* functions, but used internally.
download_source e_pow.c e_pow.cpp

download_source s_nearbyint.c s_nearbyint.cpp
download_source s_rint.c s_rint.cpp
download_source s_rintf.c s_rintf.cpp
