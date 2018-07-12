##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
if ("${AOM_TARGET_CPU}" STREQUAL "arm64")
  set(ARCH_ARM 1)
  set(HAVE_NEON 1)
  set(RTCD_ARCH_ARM "yes")
  set(RTCD_HAVE_NEON "yes")
elseif ("${AOM_TARGET_CPU}" MATCHES "^armv7")
  set(ARCH_ARM 1)
  set(HAVE_NEON 1)
  set(HAVE_NEON_ASM 1)
  set(RTCD_ARCH_ARM "yes")
  set(RTCD_HAVE_NEON "yes")
  set(RTCD_HAVE_NEON_ASM "yes")
elseif ("${AOM_TARGET_CPU}" MATCHES "^mips")
  set(ARCH_MIPS 1)

  if ("${AOM_TARGET_CPU}" STREQUAL "mips32")
    set(HAVE_MIPS32 1)
  elseif ("${AOM_TARGET_CPU}" STREQUAL "mips64")
    set(HAVE_MIPS64 1)
  endif ()

  set(RTCD_ARCH_MIPS "yes")

  if (HAVE_DSPR2)
    set(RTCD_HAVE_DSPR2 "yes")
  endif ()

  if (HAVE_MSA)
    set(RTCD_HAVE_MSA "yes")
  endif ()
elseif ("${AOM_TARGET_CPU}" MATCHES "^x86")
  if ("${AOM_TARGET_CPU}" STREQUAL "x86")
    set(ARCH_X86 1)
    set(RTCD_ARCH_X86 "yes")
  elseif ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    set(ARCH_X86_64 1)
    set(RTCD_ARCH_X86_64 "yes")
  endif ()

  set(HAVE_MMX 1)
  set(HAVE_SSE 1)
  set(HAVE_SSE2 1)
  set(HAVE_SSE3 1)
  set(HAVE_SSSE3 1)
  set(HAVE_SSE4_1 1)
  set(HAVE_AVX 1)
  set(HAVE_AVX2 1)
  set(RTCD_HAVE_MMX "yes")
  set(RTCD_HAVE_SSE "yes")
  set(RTCD_HAVE_SSE2 "yes")
  set(RTCD_HAVE_SSE3 "yes")
  set(RTCD_HAVE_SSSE3 "yes")
  set(RTCD_HAVE_SSE4_1 "yes")
  set(RTCD_HAVE_AVX "yes")
  set(RTCD_HAVE_AVX2 "yes")
endif ()

foreach (config_var ${AOM_CONFIG_VARS})
  if (${${config_var}})
    set(RTCD_${config_var} yes)
  endif ()
endforeach ()
