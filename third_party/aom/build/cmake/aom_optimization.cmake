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
if (NOT AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_)
set(AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_ 1)

include("${AOM_ROOT}/build/cmake/util.cmake")

# Translate $flag to one which MSVC understands, and write the new flag to the
# variable named by $translated_flag (or unset it, when MSVC needs no flag).
function (get_msvc_intrinsic_flag flag translated_flag)
  if ("${flag}" STREQUAL "-mavx")
    set(${translated_flag} "/arch:AVX" PARENT_SCOPE)
  elseif ("${flag}" STREQUAL "-mavx2")
    set(${translated_flag} "/arch:AVX2" PARENT_SCOPE)
  else ()
    # MSVC does not need flags for intrinsics flavors other than AVX/AVX2.
    unset(${translated_flag} PARENT_SCOPE)
  endif ()
endfunction ()

# Adds an object library target. Terminates generation if $flag is not supported
# by the current compiler. $flag is the intrinsics flag required by the current
# compiler, and is added to the compile flags for all sources in $sources.
# $opt_name is used to name the target. $target_to_update is made
# dependent upon the created target.
#
# Note: the libaom target is always updated because OBJECT libraries have rules
# that disallow the direct addition of .o files to them as dependencies. Static
# libraries do not have this limitation.
function (add_intrinsics_object_library flag opt_name target_to_update sources
          dependent_target)
  set(target_name ${target_to_update}_${opt_name}_intrinsics)
  add_library(${target_name} OBJECT ${${sources}})

  if (MSVC)
    get_msvc_intrinsic_flag(${flag} "flag")
  endif ()

  if (flag)
    target_compile_options(${target_name} PUBLIC ${flag})
  endif ()

  target_sources(${dependent_target} PRIVATE $<TARGET_OBJECTS:${target_name}>)

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${target_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

# Adds sources in list named by $sources to $target and adds $flag to the
# compile flags for each source file.
function (add_intrinsics_source_to_target flag target sources)
  target_sources(${target} PRIVATE ${${sources}})
  if (MSVC)
    get_msvc_intrinsic_flag(${flag} "flag")
  endif ()
  if (flag)
    foreach (source ${${sources}})
      set_property(SOURCE ${source} APPEND PROPERTY COMPILE_FLAGS ${flag})
    endforeach ()
  endif ()
endfunction ()

# Writes object format for the current target to the var named by $out_format,
# or terminates the build when the object format for the current target is
# unknown.
function (get_asm_obj_format out_format)
  if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho64")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      set(objformat "elf64")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS" OR
            "${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      set(objformat "win64")
    else ()
      message(FATAL_ERROR "Unknown obj format: ${AOM_TARGET_SYSTEM}")
    endif ()
  elseif ("${AOM_TARGET_CPU}" STREQUAL "x86")
    if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho32")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      set(objformat "elf32")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "MSYS" OR
            "${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      set(objformat "win32")
    else ()
      message(FATAL_ERROR "Unknown obj format: ${AOM_TARGET_SYSTEM}")
    endif ()
  else ()
    message(FATAL_ERROR
            "Unknown obj format: ${AOM_TARGET_CPU}-${AOM_TARGET_SYSTEM}")
  endif ()

  set(${out_format} ${objformat} PARENT_SCOPE)
endfunction ()

# Adds library target named $lib_name for ASM files in variable named by
# $asm_sources. Builds an output directory path from $lib_name. Links $lib_name
# into $dependent_target. Generates a dummy C file with a dummy function to
# ensure that all cmake generators can determine the linker language, and that
# build tools don't complain that an object exposes no symbols.
function (add_asm_library lib_name asm_sources dependent_target)
  set(asm_lib_obj_dir "${AOM_CONFIG_DIR}/asm_objects/${lib_name}")
  if (NOT EXISTS "${asm_lib_obj_dir}")
    file(MAKE_DIRECTORY "${asm_lib_obj_dir}")
  endif ()

  # TODO(tomfinegan): If cmake ever allows addition of .o files to OBJECT lib
  # targets, make this OBJECT instead of STATIC to hide the target from
  # consumers of the AOM cmake build.
  add_library(${lib_name} STATIC ${${asm_sources}})

  foreach (asm_source ${${asm_sources}})
    get_filename_component(asm_source_name "${asm_source}" NAME)
    set(asm_object "${asm_lib_obj_dir}/${asm_source_name}.o")
    add_custom_command(OUTPUT "${asm_object}"
                       COMMAND ${AS_EXECUTABLE}
                       ARGS ${AOM_AS_FLAGS}
                            -I${AOM_ROOT}/ -I${AOM_CONFIG_DIR}/
                            -o "${asm_object}" "${asm_source}"
                       DEPENDS "${asm_source}"
                       COMMENT "Building ASM object ${asm_object}"
                       WORKING_DIRECTORY "${AOM_CONFIG_DIR}"
                       VERBATIM)
    target_sources(aom PRIVATE "${asm_object}")
  endforeach ()

  # The above created a target containing only ASM sources. Cmake needs help
  # here to determine the linker language. Add a dummy C file to force the
  # linker language to C. We don't bother with setting the LINKER_LANGUAGE
  # property on the library target because not all generators obey it (looking
  # at you, xcode generator).
  add_dummy_source_file_to_target("${lib_name}" "c")

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${lib_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

# Converts asm sources in $asm_sources using $AOM_ADS2GAS and calls
# add_asm_library() to create a library from the converted sources. At
# generation time the converted sources are created, and a custom rule is added
# to ensure the sources are reconverted when the original asm source is updated.
# See add_asm_library() for more information.
function (add_gas_asm_library lib_name asm_sources dependent_target)
  set(asm_converted_source_dir "${AOM_CONFIG_DIR}/asm_gas/${lib_name}")
  if (NOT EXISTS "${asm_converted_source_dir}")
    file(MAKE_DIRECTORY "${asm_converted_source_dir}")
  endif ()

  # Create the converted version of each assembly source at generation time.
  unset(gas_target_sources)
  foreach (neon_asm_source ${${asm_sources}})
    get_filename_component(output_asm_source "${neon_asm_source}" NAME)
    set(output_asm_source "${asm_converted_source_dir}/${output_asm_source}")
    set(output_asm_source "${output_asm_source}.${AOM_GAS_EXT}")
    execute_process(COMMAND "${PERL_EXECUTABLE}" "${AOM_ADS2GAS}"
                    INPUT_FILE "${neon_asm_source}"
                    OUTPUT_FILE "${output_asm_source}")
    list(APPEND gas_target_sources "${output_asm_source}")
  endforeach ()

  add_asm_library("${lib_name}" "gas_target_sources" "${dependent_target}")

  # For each of the converted sources, create a custom rule that will regenerate
  # the converted source when its input is touched.
  list(LENGTH gas_target_sources num_asm_files)
  math(EXPR num_asm_files "${num_asm_files} - 1")
  foreach(NUM RANGE ${num_asm_files})
    list(GET ${asm_sources} ${NUM} neon_asm_source)
    list(GET gas_target_sources ${NUM} gas_asm_source)

    # Grab only the filename for the custom command output to keep build output
    # reasonably sane.
    get_filename_component(neon_name "${neon_asm_source}" NAME)
    get_filename_component(gas_name "${gas_asm_source}" NAME)

    add_custom_command(
        OUTPUT "${gas_asm_source}"
        COMMAND ${PERL_EXECUTABLE}
        ARGS "${AOM_ADS2GAS}" < "${neon_asm_source}" > "${gas_asm_source}"
        DEPENDS "${neon_asm_source}"
        COMMENT "ads2gas conversion ${neon_name} -> ${gas_name}"
        WORKING_DIRECTORY "${AOM_CONFIG_DIR}"
        VERBATIM)
  endforeach ()

  # Update the sources list passed in to include the converted asm source files.
  list(APPEND asm_sources ${gas_target_sources})
  set(${asm_sources} ${${asm_sources}} PARENT_SCOPE)
endfunction ()

# Terminates generation if nasm found in PATH does not meet requirements.
# Currently checks only for presence of required object formats and support for
# the -Ox argument (multipass optimization).
function (test_nasm)
  execute_process(COMMAND ${AS_EXECUTABLE} -hf
                  OUTPUT_VARIABLE nasm_helptext)

  if (NOT "${nasm_helptext}" MATCHES "-Ox")
    message(FATAL_ERROR
            "Unsupported nasm: multipass optimization not supported.")
  endif ()

  if ("${AOM_TARGET_CPU}" STREQUAL "x86")
    if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      if (NOT "${nasm_helptext}" MATCHES "macho32")
        message(FATAL_ERROR
                "Unsupported nasm: macho32 object format not supported.")
      endif ()
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      if (NOT "${nasm_helptext}" MATCHES "elf32")
        message(FATAL_ERROR
                "Unsupported nasm: elf32 object format not supported.")
      endif ()
    endif ()
  else ()
    if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      if (NOT "${nasm_helptext}" MATCHES "macho64")
        message(FATAL_ERROR
                "Unsupported nasm: macho64 object format not supported.")
      endif ()
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      if (NOT "${nasm_helptext}" MATCHES "elf64")
        message(FATAL_ERROR
                "Unsupported nasm: elf64 object format not supported.")
      endif ()
    endif ()
  endif ()
endfunction ()

# Adds build command for generation of rtcd C source files using
# build/make/rtcd.pl. $config is the input perl file, $output is the output C
# include file, $source is the C source file, and $symbol is used for the symbol
# argument passed to rtcd.pl.
function (add_rtcd_build_step config output source symbol)
  add_custom_command(
    OUTPUT ${output}
    COMMAND ${PERL_EXECUTABLE}
    ARGS "${AOM_ROOT}/build/make/rtcd.pl"
      --arch=${AOM_TARGET_CPU}
      --sym=${symbol}
      ${AOM_RTCD_FLAGS}
      --config=${AOM_CONFIG_DIR}/${AOM_TARGET_CPU}_rtcd_config.rtcd
      ${config}
      > ${output}
    DEPENDS ${config}
    COMMENT "Generating ${output}"
    WORKING_DIRECTORY ${AOM_CONFIG_DIR}
    VERBATIM)
  set_property(SOURCE ${source} PROPERTY OBJECT_DEPENDS ${output})
  set_property(SOURCE ${output} PROPERTY GENERATED)
endfunction ()

endif ()  # AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_
