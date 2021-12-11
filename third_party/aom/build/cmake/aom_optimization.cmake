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
if(AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_
set(AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_ 1)

include("${AOM_ROOT}/build/cmake/util.cmake")

# Translate $flag to one which MSVC understands, and write the new flag to the
# variable named by $translated_flag (or unset it, when MSVC needs no flag).
function(get_msvc_intrinsic_flag flag translated_flag)
  if("${flag}" STREQUAL "-mavx")
    set(${translated_flag} "/arch:AVX" PARENT_SCOPE)
  elseif("${flag}" STREQUAL "-mavx2")
    set(${translated_flag} "/arch:AVX2" PARENT_SCOPE)
  else()

    # MSVC does not need flags for intrinsics flavors other than AVX/AVX2.
    unset(${translated_flag} PARENT_SCOPE)
  endif()
endfunction()

# Adds an object library target. Terminates generation if $flag is not supported
# by the current compiler. $flag is the intrinsics flag required by the current
# compiler, and is added to the compile flags for all sources in $sources.
# $opt_name is used to name the target. $target_to_update is made dependent upon
# the created target.
#
# Note: the libaom target is always updated because OBJECT libraries have rules
# that disallow the direct addition of .o files to them as dependencies. Static
# libraries do not have this limitation.
function(add_intrinsics_object_library flag opt_name target_to_update sources
         dependent_target)
  if("${${sources}}" STREQUAL "")
    return()
  endif()
  set(target_name ${target_to_update}_${opt_name}_intrinsics)
  add_library(${target_name} OBJECT ${${sources}})

  if(MSVC)
    get_msvc_intrinsic_flag(${flag} "flag")
  endif()

  if(flag)
    separate_arguments(flag)
    target_compile_options(${target_name} PUBLIC ${flag})
  endif()

  target_sources(${dependent_target} PRIVATE $<TARGET_OBJECTS:${target_name}>)

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${target_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction()

# Adds sources in list named by $sources to $target and adds $flag to the
# compile flags for each source file.
function(add_intrinsics_source_to_target flag target sources)
  target_sources(${target} PRIVATE ${${sources}})
  if(MSVC)
    get_msvc_intrinsic_flag(${flag} "flag")
  endif()
  if(flag)
    foreach(source ${${sources}})
      set_property(SOURCE ${source} APPEND PROPERTY COMPILE_FLAGS ${flag})
    endforeach()
  endif()
endfunction()

# Writes object format for the current target to the var named by $out_format,
# or terminates the build when the object format for the current target is
# unknown.
function(get_asm_obj_format out_format)
  if("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho64")
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      set(objformat "elf64")
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS" OR "${AOM_TARGET_SYSTEM}"
           STREQUAL "Windows")
      set(objformat "win64")
    else()
      message(FATAL_ERROR "Unknown obj format: ${AOM_TARGET_SYSTEM}")
    endif()
  elseif("${AOM_TARGET_CPU}" STREQUAL "x86")
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho32")
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      set(objformat "elf32")
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS" OR "${AOM_TARGET_SYSTEM}"
           STREQUAL "Windows")
      set(objformat "win32")
    else()
      message(FATAL_ERROR "Unknown obj format: ${AOM_TARGET_SYSTEM}")
    endif()
  else()
    message(FATAL_ERROR
              "Unknown obj format: ${AOM_TARGET_CPU}-${AOM_TARGET_SYSTEM}")
  endif()

  set(${out_format} ${objformat} PARENT_SCOPE)
endfunction()

# Adds library target named $lib_name for ASM files in variable named by
# $asm_sources. Builds an output directory path from $lib_name. Links $lib_name
# into $dependent_target. Generates a dummy C file with a dummy function to
# ensure that all cmake generators can determine the linker language, and that
# build tools don't complain that an object exposes no symbols.
function(add_asm_library lib_name asm_sources dependent_target)
  if("${${asm_sources}}" STREQUAL "")
    return()
  endif()
  set(asm_lib_obj_dir "${AOM_CONFIG_DIR}/asm_objects/${lib_name}")
  if(NOT EXISTS "${asm_lib_obj_dir}")
    file(MAKE_DIRECTORY "${asm_lib_obj_dir}")
  endif()

  # TODO(tomfinegan): If cmake ever allows addition of .o files to OBJECT lib
  # targets, make this OBJECT instead of STATIC to hide the target from
  # consumers of the AOM cmake build.
  add_library(${lib_name} STATIC ${${asm_sources}})

  foreach(asm_source ${${asm_sources}})
    get_filename_component(asm_source_name "${asm_source}" NAME)
    set(asm_object "${asm_lib_obj_dir}/${asm_source_name}.o")
    add_custom_command(OUTPUT "${asm_object}"
                       COMMAND ${AS_EXECUTABLE} ARGS ${AOM_AS_FLAGS}
                               -I${AOM_ROOT}/ -I${AOM_CONFIG_DIR}/ -o
                               "${asm_object}" "${asm_source}"
                       DEPENDS "${asm_source}"
                       COMMENT "Building ASM object ${asm_object}"
                       WORKING_DIRECTORY "${AOM_CONFIG_DIR}" VERBATIM)
    target_sources(aom PRIVATE "${asm_object}")
  endforeach()

  # The above created a target containing only ASM sources. Cmake needs help
  # here to determine the linker language. Add a dummy C file to force the
  # linker language to C. We don't bother with setting the LINKER_LANGUAGE
  # property on the library target because not all generators obey it (looking
  # at you, xcode generator).
  add_dummy_source_file_to_target("${lib_name}" "c")

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${lib_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction()

# Terminates generation if nasm found in PATH does not meet requirements.
# Currently checks only for presence of required object formats and support for
# the -Ox argument (multipass optimization).
function(test_nasm)
  execute_process(COMMAND ${AS_EXECUTABLE} -hf OUTPUT_VARIABLE nasm_helptext)

  if(NOT "${nasm_helptext}" MATCHES "-Ox")
    message(FATAL_ERROR
              "Unsupported nasm: multipass optimization not supported.")
  endif()

  if("${AOM_TARGET_CPU}" STREQUAL "x86")
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      if(NOT "${nasm_helptext}" MATCHES "macho32")
        message(FATAL_ERROR
                  "Unsupported nasm: macho32 object format not supported.")
      endif()
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      if(NOT "${nasm_helptext}" MATCHES "elf32")
        message(FATAL_ERROR
                  "Unsupported nasm: elf32 object format not supported.")
      endif()
    endif()
  else()
    if("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      if(NOT "${nasm_helptext}" MATCHES "macho64")
        message(FATAL_ERROR
                  "Unsupported nasm: macho64 object format not supported.")
      endif()
    elseif("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      if(NOT "${nasm_helptext}" MATCHES "elf64")
        message(FATAL_ERROR
                  "Unsupported nasm: elf64 object format not supported.")
      endif()
    endif()
  endif()
endfunction()

# Adds build command for generation of rtcd C source files using
# build/cmake/rtcd.pl. $config is the input perl file, $output is the output C
# include file, $source is the C source file, and $symbol is used for the symbol
# argument passed to rtcd.pl.
function(add_rtcd_build_step config output source symbol)
  add_custom_command(OUTPUT ${output}
                     COMMAND ${PERL_EXECUTABLE} ARGS
                             "${AOM_ROOT}/build/cmake/rtcd.pl"
                             --arch=${AOM_TARGET_CPU}
                             --sym=${symbol} ${AOM_RTCD_FLAGS}
                             --config=${AOM_CONFIG_DIR}/config/aom_config.h
                             ${config} > ${output}
                     DEPENDS ${config}
                     COMMENT "Generating ${output}"
                     WORKING_DIRECTORY ${AOM_CONFIG_DIR} VERBATIM)
  set_property(SOURCE ${source} PROPERTY OBJECT_DEPENDS ${output})
  set_property(SOURCE ${output} PROPERTY GENERATED)
endfunction()
