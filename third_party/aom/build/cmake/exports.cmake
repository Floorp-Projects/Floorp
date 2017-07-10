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
if (NOT AOM_BUILD_CMAKE_EXPORTS_CMAKE_)
set(AOM_BUILD_CMAKE_EXPORTS_CMAKE_ 1)

include("${AOM_ROOT}/build/cmake/exports_sources.cmake")

# Creates the custom target which handles generation of the symbol export lists.
function (setup_exports_target)
  if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
    set(symbol_file_ext "syms")
  elseif ("${AOM_TARGET_SYSTEM}" MATCHES "Windows\|MSYS" AND MSVC)
    set(symbol_file_ext "def")
  else ()
    set(symbol_file_ext "ver")
  endif ()

  set(aom_sym_file "${AOM_CONFIG_DIR}/libaom.${symbol_file_ext}")

  add_custom_target(generate_exports
                    COMMAND ${CMAKE_COMMAND}
                      -DAOM_ROOT="${AOM_ROOT}"
                      -DAOM_CONFIG_DIR="${AOM_CONFIG_DIR}"
                      -DAOM_TARGET_SYSTEM=${AOM_TARGET_SYSTEM}
                      -DAOM_SYM_FILE="${aom_sym_file}"
                      -DAOM_MSVC=${MSVC}
                      -DAOM_XCODE=${XCODE}
                      -DCONFIG_NAME=$<CONFIG>
                      -DCONFIG_AV1_DECODER=${CONFIG_AV1_DECODER}
                      -DCONFIG_AV1_ENCODER=${CONFIG_AV1_ENCODER}
                      -P "${AOM_ROOT}/build/cmake/generate_exports.cmake"
                    SOURCES ${AOM_EXPORTS_SOURCES}
                    DEPENDS ${AOM_EXPORTS_SOURCES})

  # Make libaom depend on the exports file, and set flags to pick it up when
  # creating the dylib.
  add_dependencies(aom generate_exports)

  if (APPLE)
    set_property(TARGET aom APPEND_STRING PROPERTY LINK_FLAGS
                 "-exported_symbols_list ${aom_sym_file}")
  elseif (WIN32)
    message(FATAL_ERROR "Windows DLL builds not supported yet.")
    if (NOT MSVC)
      set_property(TARGET aom APPEND_STRING PROPERTY LINK_FLAGS
                   "-Wl,--version-script ${aom_sym_file}")
    endif ()

    # TODO(tomfinegan): Sort out the import lib situation and flags for MSVC.

  else ()
    set_property(TARGET aom APPEND_STRING PROPERTY LINK_FLAGS
                 "-Wl,--version-script,${aom_sym_file}")
  endif ()
endfunction ()

endif ()  # AOM_BUILD_CMAKE_EXPORTS_CMAKE_
