# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

find_package(Threads REQUIRED)

include(jxl_lists.cmake)

add_library(jxl_threads ${JPEGXL_INTERNAL_THREADS_SOURCES})
target_compile_options(jxl_threads PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jxl_threads PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jxl_threads PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(jxl_threads
  PRIVATE
    "${PROJECT_SOURCE_DIR}"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include")

target_link_libraries(jxl_threads
  PUBLIC ${JPEGXL_COVERAGE_FLAGS} Threads::Threads
)

set_target_properties(jxl_threads PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JXL_THREADS_INTERNAL_LIBRARY_BUILD
)

install(TARGETS jxl_threads
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

set_target_properties(jxl_threads PROPERTIES
  VERSION ${JPEGXL_LIBRARY_VERSION}
  SOVERSION ${JPEGXL_LIBRARY_SOVERSION})

set_target_properties(jxl_threads PROPERTIES
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/jxl/jxl.version)
if(APPLE)
  set_property(TARGET ${target} APPEND_STRING PROPERTY
      LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/jxl/jxl_osx.syms")
elseif(WIN32)
# Nothing needed here, we use __declspec(dllexport) (jxl_threads_export.h)
else()
  set_property(TARGET jxl_threads APPEND_STRING PROPERTY
      LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/jxl/jxl.version")
endif()  # APPLE

# Compile the shared library such that the JXL_THREADS_EXPORT symbols are
# exported. Users of the library will not set this flag and therefore import
# those symbols.
target_compile_definitions(jxl_threads
  PRIVATE -DJXL_THREADS_INTERNAL_LIBRARY_BUILD)

generate_export_header(jxl_threads
  BASE_NAME JXL_THREADS
  EXPORT_FILE_NAME include/jxl/jxl_threads_export.h)


### Add a pkg-config file for libjxl_threads.

# Allow adding prefix if CMAKE_INSTALL_INCLUDEDIR not absolute.
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(PKGCONFIG_TARGET_INCLUDES "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(PKGCONFIG_TARGET_INCLUDES "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
# Allow adding prefix if CMAKE_INSTALL_LIBDIR not absolute.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PKGCONFIG_TARGET_LIBS "${CMAKE_INSTALL_LIBDIR}")
else()
    set(PKGCONFIG_TARGET_LIBS "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if (BUILD_SHARED_LIBS)
  set(JPEGXL_REQUIRES_TYPE "Requires.private")
else()
  set(JPEGXL_REQUIRES_TYPE "Requires")
endif()

set(JPEGXL_THREADS_LIBRARY_REQUIRES "")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/threads/libjxl_threads.pc.in"
               "libjxl_threads.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjxl_threads.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
