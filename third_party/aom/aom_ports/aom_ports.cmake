#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_AOM_PORTS_AOM_PORTS_CMAKE_)
  return()
endif() # AOM_AOM_PORTS_AOM_PORTS_CMAKE_
set(AOM_AOM_PORTS_AOM_PORTS_CMAKE_ 1)

list(APPEND AOM_PORTS_INCLUDES
            "${AOM_ROOT}/aom_ports/aom_once.h"
            "${AOM_ROOT}/aom_ports/aom_timer.h"
            "${AOM_ROOT}/aom_ports/bitops.h"
            "${AOM_ROOT}/aom_ports/emmintrin_compat.h"
            "${AOM_ROOT}/aom_ports/mem.h"
            "${AOM_ROOT}/aom_ports/mem_ops.h"
            "${AOM_ROOT}/aom_ports/mem_ops_aligned.h"
            "${AOM_ROOT}/aom_ports/msvc.h"
            "${AOM_ROOT}/aom_ports/sanitizer.h"
            "${AOM_ROOT}/aom_ports/system_state.h")

list(APPEND AOM_PORTS_ASM_X86 "${AOM_ROOT}/aom_ports/emms.asm")

list(APPEND AOM_PORTS_INCLUDES_X86 "${AOM_ROOT}/aom_ports/x86_abi_support.asm")

list(APPEND AOM_PORTS_SOURCES_ARM "${AOM_ROOT}/aom_ports/arm.h"
            "${AOM_ROOT}/aom_ports/arm_cpudetect.c")

list(APPEND AOM_PORTS_SOURCES_PPC "${AOM_ROOT}/aom_ports/ppc.h"
            "${AOM_ROOT}/aom_ports/ppc_cpudetect.c")

# For arm and x86 targets:
#
# * Creates the aom_ports build target, adds the includes in aom_ports to the
#   target, and makes libaom depend on it.
#
# Otherwise:
#
# * Adds the includes in aom_ports to the libaom target.
#
# For all target platforms:
#
# * The libaom target must exist before this function is called.
function(setup_aom_ports_targets)
  if("${AOM_TARGET_CPU}" MATCHES "^x86")
    add_asm_library("aom_ports" "AOM_PORTS_ASM_X86" "aom")
    set(aom_ports_has_symbols 1)
  elseif("${AOM_TARGET_CPU}" MATCHES "arm")
    add_library(aom_ports OBJECT ${AOM_PORTS_SOURCES_ARM})
    set(aom_ports_has_symbols 1)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_ports>)
  elseif("${AOM_TARGET_CPU}" MATCHES "ppc")
    add_library(aom_ports OBJECT ${AOM_PORTS_SOURCES_PPC})
    set(aom_ports_has_symbols 1)
    target_sources(aom PRIVATE $<TARGET_OBJECTS:aom_ports>)
  endif()

  if(aom_ports_has_symbols)
    target_sources(aom_ports PRIVATE ${AOM_PORTS_INCLUDES})

    if("${AOM_TARGET_CPU}" STREQUAL "x86" OR "${AOM_TARGET_CPU}" STREQUAL
       "x86_64")
      target_sources(aom_ports PRIVATE ${AOM_PORTS_INCLUDES_X86})
    endif()

    set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
  else()
    target_sources(aom PRIVATE ${AOM_PORTS_INCLUDES})

    if("${AOM_TARGET_CPU}" STREQUAL "x86" OR "${AOM_TARGET_CPU}" STREQUAL
       "x86_64")
      target_sources(aom PRIVATE ${AOM_PORTS_INCLUDES_X86})
    endif()
  endif()
endfunction()
