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
if (NOT AOM_BUILD_CMAKE_UTIL_CMAKE_)
set(AOM_BUILD_CMAKE_UTIL_CMAKE_ 1)

function (create_dummy_source_file basename extension out_file_path)
  set(dummy_source_file "${AOM_CONFIG_DIR}/${basename}.${extension}")
   file(WRITE "${dummy_source_file}"
       "// Generated file. DO NOT EDIT!\n"
       "// ${target_name} needs a ${extension} file to force link language, \n"
       "// or to silence a harmless CMake warning: Ignore me.\n"
       "void ${target_name}_dummy_function(void) {}\n")
  set(${out_file_path} ${dummy_source_file} PARENT_SCOPE)
endfunction ()

function (add_dummy_source_file_to_target target_name extension)
  create_dummy_source_file("${target_name}" "${extension}" "dummy_source_file")
  target_sources(${target_name} PRIVATE ${dummy_source_file})
endfunction ()

endif()  # AOM_BUILD_CMAKE_UTIL_CMAKE_

