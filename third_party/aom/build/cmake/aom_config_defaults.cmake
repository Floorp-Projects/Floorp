#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.

include("${AOM_ROOT}/build/cmake/util.cmake")

# This file sets default values for libaom configuration variables. All libaom
# config variables are added to the CMake variable cache via the macros provided
# in util.cmake.

#
# The variables in this section of the file are detected at configuration time,
# but can be overridden via the use of CONFIG_* and ENABLE_* values also defined
# in this file.
#

set_aom_detect_var(INLINE "" STRING "Sets INLINE value for current target.")

# CPUs.
set_aom_detect_var(ARCH_ARM 0 NUMBER "Enables ARM architecture.")
set_aom_detect_var(ARCH_MIPS 0 NUMBER "Enables MIPS architecture.")
set_aom_detect_var(ARCH_PPC 0 NUMBER "Enables PPC architecture.")
set_aom_detect_var(ARCH_X86 0 NUMBER "Enables X86 architecture.")
set_aom_detect_var(ARCH_X86_64 0 NUMBER "Enables X86_64 architecture.")

# ARM feature flags.
set_aom_detect_var(HAVE_NEON 0 NUMBER "Enables NEON intrinsics optimizations.")

# MIPS feature flags.
set_aom_detect_var(HAVE_DSPR2 0 NUMBER "Enables DSPR2 optimizations.")
set_aom_detect_var(HAVE_MIPS32 0 NUMBER "Enables MIPS32 optimizations.")
set_aom_detect_var(HAVE_MIPS64 0 NUMBER "Enables MIPS64 optimizations. ")
set_aom_detect_var(HAVE_MSA 0 NUMBER "Enables MSA optimizations.")

# PPC feature flags.
set_aom_detect_var(HAVE_VSX 0 NUMBER "Enables VSX optimizations.")

# x86/x86_64 feature flags.
set_aom_detect_var(HAVE_AVX 0 NUMBER "Enables AVX optimizations.")
set_aom_detect_var(HAVE_AVX2 0 NUMBER "Enables AVX2 optimizations.")
set_aom_detect_var(HAVE_MMX 0 NUMBER "Enables MMX optimizations. ")
set_aom_detect_var(HAVE_SSE 0 NUMBER "Enables SSE optimizations.")
set_aom_detect_var(HAVE_SSE2 0 NUMBER "Enables SSE2 optimizations.")
set_aom_detect_var(HAVE_SSE3 0 NUMBER "Enables SSE3 optimizations.")
set_aom_detect_var(HAVE_SSE4_1 0 NUMBER "Enables SSE 4.1 optimizations.")
set_aom_detect_var(HAVE_SSE4_2 0 NUMBER "Enables SSE 4.2 optimizations.")
set_aom_detect_var(HAVE_SSSE3 0 NUMBER "Enables SSSE3 optimizations.")

# Flags describing the build environment.
set_aom_detect_var(HAVE_FEXCEPT 0 NUMBER
                   "Internal flag, GNU fenv.h present for target.")
set_aom_detect_var(HAVE_PTHREAD_H 0 NUMBER
                   "Internal flag, target pthread support.")
set_aom_detect_var(HAVE_UNISTD_H 0 NUMBER
                   "Internal flag, unistd.h present for target.")
set_aom_detect_var(HAVE_WXWIDGETS 0 NUMBER "WxWidgets present.")

#
# Variables in this section can be set from the CMake command line or from
# within the CMake GUI. The variables control libaom features.
#

# Build configuration flags.
set_aom_config_var(AOM_RTCD_FLAGS "" STRING
                   "Arguments to pass to rtcd.pl. Separate with ';'")
set_aom_config_var(CONFIG_AV1_DECODER 1 NUMBER "Enable AV1 decoder.")
set_aom_config_var(CONFIG_AV1_ENCODER 1 NUMBER "Enable AV1 encoder.")
set_aom_config_var(CONFIG_BIG_ENDIAN 0 NUMBER "Internal flag.")
set_aom_config_var(CONFIG_GCC 0 NUMBER "Building with GCC (detect).")
set_aom_config_var(CONFIG_GCOV 0 NUMBER "Enable gcov support.")
set_aom_config_var(CONFIG_GPROF 0 NUMBER "Enable gprof support.")
set_aom_config_var(CONFIG_LIBYUV 1 NUMBER
                   "Enables libyuv scaling/conversion support.")

set_aom_config_var(CONFIG_MULTITHREAD 1 NUMBER "Multithread support.")
set_aom_config_var(CONFIG_OS_SUPPORT 0 NUMBER "Internal flag.")
set_aom_config_var(CONFIG_PIC 0 NUMBER "Build with PIC enabled.")
set_aom_config_var(CONFIG_RUNTIME_CPU_DETECT 1 NUMBER
                   "Runtime CPU detection support.")
set_aom_config_var(CONFIG_SHARED 0 NUMBER "Build shared libs.")
set_aom_config_var(CONFIG_STATIC 1 NUMBER "Build static libs.")
set_aom_config_var(CONFIG_WEBM_IO 1 NUMBER "Enables WebM support.")

# Debugging flags.
set_aom_config_var(CONFIG_BITSTREAM_DEBUG 0 NUMBER "Bitstream debugging flag.")
set_aom_config_var(CONFIG_DEBUG 0 NUMBER "Debug build flag.")
set_aom_config_var(CONFIG_MISMATCH_DEBUG 0 NUMBER "Mismatch debugging flag.")

# AV1 feature flags.
set_aom_config_var(CONFIG_ACCOUNTING 0 NUMBER "Enables bit accounting.")
set_aom_config_var(CONFIG_ANALYZER 0 NUMBER "Enables bit stream analyzer.")
set_aom_config_var(CONFIG_COEFFICIENT_RANGE_CHECKING 0 NUMBER
                   "Coefficient range check.")
set_aom_config_var(CONFIG_DENOISE 1 NUMBER
                   "Denoise/noise modeling support in encoder.")
set_aom_config_var(CONFIG_FILEOPTIONS 1 NUMBER
                   "Enables encoder config file support.")
set_aom_config_var(CONFIG_FIX_GF_LENGTH 1 NUMBER
                   "Fix the GF length if possible")
set_aom_config_var(CONFIG_INSPECTION 0 NUMBER "Enables bitstream inspection.")
set_aom_config_var(CONFIG_INTERNAL_STATS 0 NUMBER
                   "Enables internal encoder stats.")
set_aom_config_var(CONFIG_LOWBITDEPTH 0 NUMBER
                   "Enables 8-bit optimized pipeline.")
set_aom_config_var(CONFIG_MAX_DECODE_PROFILE 2 NUMBER
                   "Max profile to support decoding.")
set_aom_config_var(CONFIG_NORMAL_TILE_MODE 0 NUMBER
                   "Only enables normal tile mode.")
set_aom_config_var(
  CONFIG_REDUCED_ENCODER_BORDER 0 NUMBER
  "Enable reduced border extention for encoder. \
                    Disables superres and resize support."
  )
set_aom_config_var(CONFIG_SIZE_LIMIT 0 NUMBER "Limit max decode width/height.")
set_aom_config_var(CONFIG_SPATIAL_RESAMPLING 1 NUMBER "Spatial resampling.")
set_aom_config_var(DECODE_HEIGHT_LIMIT 0 NUMBER "Set limit for decode height.")
set_aom_config_var(DECODE_WIDTH_LIMIT 0 NUMBER "Set limit for decode width.")
set_aom_config_var(CONFIG_GLOBAL_MOTION_SEARCH 1 NUMBER
                   "Global motion search flag.")

# AV1 experiment flags.
set_aom_config_var(CONFIG_COLLECT_INTER_MODE_RD_STATS 1 NUMBER
                   "AV1 experiment flag.")
set_aom_config_var(CONFIG_COLLECT_RD_STATS 0 NUMBER "AV1 experiment flag.")
set_aom_config_var(CONFIG_DIST_8X8 0 NUMBER "AV1 experiment flag.")
set_aom_config_var(CONFIG_ENTROPY_STATS 0 NUMBER "AV1 experiment flag.")
set_aom_config_var(CONFIG_FP_MB_STATS 0 NUMBER "AV1 experiment flag.")
set_aom_config_var(CONFIG_INTER_STATS_ONLY 0 NUMBER "AV1 experiment flag.")
set_aom_config_var(CONFIG_RD_DEBUG 0 NUMBER "AV1 experiment flag.")
set_aom_config_var(CONFIG_2PASS_PARTITION_SEARCH_LVL 1 NUMBER
                   "AV1 experiment flag.")
set_aom_config_var(CONFIG_SHARP_SETTINGS 0 NUMBER
                   "Use sharper encoding settings")

#
# Variables in this section control optional features of the build system.
#
set_aom_option_var(ENABLE_CCACHE "Enable ccache support." OFF)
set_aom_option_var(ENABLE_DECODE_PERF_TESTS "Enables decoder performance tests"
                   OFF)
set_aom_option_var(ENABLE_DISTCC "Enable distcc support." OFF)
set_aom_option_var(ENABLE_DOCS
                   "Enable documentation generation (doxygen required)." ON)
set_aom_option_var(ENABLE_ENCODE_PERF_TESTS "Enables encoder performance tests"
                   OFF)
set_aom_option_var(ENABLE_EXAMPLES "Enables build of example code." ON)
set_aom_option_var(ENABLE_GOMA "Enable goma support." OFF)
set_aom_option_var(
  ENABLE_IDE_TEST_HOSTING
  "Enables running tests within IDEs like Visual Studio and Xcode." OFF)
set_aom_option_var(ENABLE_NASM "Use nasm instead of yasm for x86 assembly." OFF)
set_aom_option_var(ENABLE_TESTDATA "Enables unit test data download targets."
                   ON)
set_aom_option_var(ENABLE_TESTS "Enables unit tests." ON)
set_aom_option_var(ENABLE_TOOLS "Enable applications in tools sub directory."
                   ON)
set_aom_option_var(ENABLE_WERROR "Converts warnings to errors at compile time."
                   OFF)

# ARM assembly/intrinsics flags.
set_aom_option_var(ENABLE_NEON "Enables NEON optimizations on ARM targets." ON)

# MIPS assembly/intrinsics flags.
set_aom_option_var(ENABLE_DSPR2 "Enables DSPR2 optimizations on MIPS targets."
                   OFF)
set_aom_option_var(ENABLE_MSA "Enables MSA optimizations on MIPS targets." OFF)

# VSX intrinsics flags.
set_aom_option_var(ENABLE_VSX "Enables VSX optimizations on PowerPC targets."
                   ON)

# x86/x86_64 assembly/intrinsics flags.
set_aom_option_var(ENABLE_MMX
                   "Enables MMX optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE
                   "Enables SSE optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE2
                   "Enables SSE2 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE3
                   "Enables SSE3 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSSE3
                   "Enables SSSE3 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE4_1
                   "Enables SSE4_1 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE4_2
                   "Enables SSE4_2 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_AVX
                   "Enables AVX optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_AVX2
                   "Enables AVX2 optimizations on x86/x86_64 targets." ON)
