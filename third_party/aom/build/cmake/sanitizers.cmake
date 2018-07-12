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
if (NOT AOM_BUILD_CMAKE_SANITIZERS_CMAKE_)
set(AOM_BUILD_CMAKE_SANITIZERS_CMAKE_ 1)

if (MSVC OR NOT SANITIZE)
  return ()
endif ()

include("${AOM_ROOT}/build/cmake/compiler_flags.cmake")

string(TOLOWER ${SANITIZE} SANITIZE)

# Require the sanitizer requested.
require_linker_flag("-fsanitize=${SANITIZE}")
require_compiler_flag("-fsanitize=${SANITIZE}" YES)

# Make callstacks accurate.
require_compiler_flag("-fno-omit-frame-pointer -fno-optimize-sibling-calls" YES)

endif()  # AOM_BUILD_CMAKE_SANITIZERS_CMAKE_
