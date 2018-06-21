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
cmake_minimum_required(VERSION 3.5)

set(REQUIRED_ARGS "AOM_ROOT" "AOM_CONFIG_DIR" "CMAKE_INSTALL_PREFIX"
    "CMAKE_PROJECT_NAME" "CONFIG_MULTITHREAD" "HAVE_PTHREAD_H")

foreach(arg ${REQUIRED_ARGS})
  if("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "${arg} must not be empty.")
  endif()
endforeach()

include("${AOM_ROOT}/build/cmake/util.cmake")

extract_version_string("${AOM_CONFIG_DIR}/config/aom_version.h" aom_version)

# Create a version string suitable for comparison using the RPM version compare
# algorithm: strip out everything after the number.
string(FIND "${aom_version}" "-" dash_pos)
if(${dash_pos} EQUAL -1)
  set(package_version "${aom_version}")
else()
  string(SUBSTRING "${aom_version}" 0 ${dash_pos} package_version)
endif()

# Write pkg-config info.
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(pkgconfig_file "${AOM_CONFIG_DIR}/aom.pc")
string(TOLOWER ${CMAKE_PROJECT_NAME} pkg_name)
file(WRITE "${pkgconfig_file}" "# libaom pkg-config.\n")
file(APPEND "${pkgconfig_file}" "prefix=${prefix}\n")
file(APPEND "${pkgconfig_file}" "exec_prefix=\${prefix}/bin\n")
file(APPEND "${pkgconfig_file}" "libdir=\${prefix}/lib\n")
file(APPEND "${pkgconfig_file}" "includedir=\${prefix}/include\n\n")
file(APPEND "${pkgconfig_file}" "Name: ${pkg_name}\n")
file(APPEND "${pkgconfig_file}"
            "Description: AV1 codec library v${aom_version}.\n")
file(APPEND "${pkgconfig_file}" "Version: ${package_version}\n")
file(APPEND "${pkgconfig_file}" "Requires:\n")
file(APPEND "${pkgconfig_file}" "Conflicts:\n")
if(CONFIG_MULTITHREAD AND HAVE_PTHREAD_H)
  file(APPEND "${pkgconfig_file}"
              "Libs: -L\${prefix}/lib -l${pkg_name} -lm -lpthread\n")
  file(APPEND "${pkgconfig_file}" "Libs.private: -lm -lpthread\n")
else()
  file(APPEND "${pkgconfig_file}" "Libs: -L\${prefix}/lib -l${pkg_name} -lm\n")
  file(APPEND "${pkgconfig_file}" "Libs.private: -lm\n")
endif()
file(APPEND "${pkgconfig_file}" "Cflags: -I\${prefix}/include\n")
