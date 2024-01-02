# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

include(jxl_lists.cmake)

if (JPEGXL_ENABLE_TOOLS OR JPEGXL_ENABLE_DEVTOOLS OR JPEGXL_ENABLE_BOXES)
list(APPEND JPEGXL_INTERNAL_DEC_SOURCES ${JPEGXL_INTERNAL_DEC_BOX_SOURCES})
endif()

if (JPEGXL_ENABLE_TRANSCODE_JPEG OR JPEGXL_ENABLE_TOOLS OR JPEGXL_ENABLE_DEVTOOLS)
list(APPEND JPEGXL_INTERNAL_DEC_SOURCES ${JPEGXL_INTERNAL_DEC_JPEG_SOURCES})
endif()

set_source_files_properties(jxl/enc_fast_lossless.cc PROPERTIES COMPILE_FLAGS -O3)

set(JPEGXL_DEC_INTERNAL_LIBS
  hwy
  Threads::Threads
  ${ATOMICS_LIBRARIES}
)

if (JPEGXL_ENABLE_TRANSCODE_JPEG OR JPEGXL_ENABLE_BOXES)
list(APPEND JPEGXL_DEC_INTERNAL_LIBS brotlidec brotlicommon)
endif()

set(JPEGXL_INTERNAL_LIBS
  ${JPEGXL_DEC_INTERNAL_LIBS}
  brotlienc
)

if (JPEGXL_ENABLE_TRANSCODE_JPEG)
  list(APPEND JPEGXL_INTERNAL_FLAGS -DJPEGXL_ENABLE_TRANSCODE_JPEG=1)
else()
  list(APPEND JPEGXL_INTERNAL_FLAGS -DJPEGXL_ENABLE_TRANSCODE_JPEG=0)
endif ()

if (JPEGXL_ENABLE_BOXES)
  list(APPEND JPEGXL_INTERNAL_FLAGS -DJPEGXL_ENABLE_BOXES=1)
else()
  list(APPEND JPEGXL_INTERNAL_FLAGS -DJPEGXL_ENABLE_BOXES=0)
endif ()

set(OBJ_COMPILE_DEFINITIONS
  JPEGXL_MAJOR_VERSION=${JPEGXL_MAJOR_VERSION}
  JPEGXL_MINOR_VERSION=${JPEGXL_MINOR_VERSION}
  JPEGXL_PATCH_VERSION=${JPEGXL_PATCH_VERSION}
  # Used to determine if we are building the library when defined or just
  # including the library when not defined. This is public so libjxl shared
  # library gets this define too.
  JXL_INTERNAL_LIBRARY_BUILD
)

# Generate version.h
configure_file("jxl/version.h.in" "include/jxl/version.h")

# Headers for exporting/importing public headers
include(GenerateExportHeader)

# CMake does not allow generate_export_header for INTERFACE library, so we
# add this stub library just for file generation.
add_library(jxl_export OBJECT ${JPEGXL_INTERNAL_PUBLIC_HEADERS})
set_target_properties(jxl_export PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JXL_INTERNAL_LIBRARY_BUILD
  LINKER_LANGUAGE CXX
)
generate_export_header(jxl_export
  BASE_NAME JXL
  EXPORT_FILE_NAME include/jxl/jxl_export.h)
# Place all public headers in a single directory.
foreach(path ${JPEGXL_INTERNAL_PUBLIC_HEADERS})
  configure_file(
    ${path}
    ${path}
    COPYONLY
  )
endforeach()

add_library(jxl_base INTERFACE)
target_include_directories(jxl_base SYSTEM INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)
target_include_directories(jxl_base INTERFACE
  ${PROJECT_SOURCE_DIR}
  ${JXL_HWY_INCLUDE_DIRS}
)
add_dependencies(jxl_base jxl_export)

# Decoder-only object library
add_library(jxl_dec-obj OBJECT ${JPEGXL_INTERNAL_DEC_SOURCES})
target_compile_options(jxl_dec-obj PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jxl_dec-obj PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jxl_dec-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_dec-obj PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
  "${JXL_HWY_INCLUDE_DIRS}"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:brotlicommon,INTERFACE_INCLUDE_DIRECTORIES>>"
)
target_compile_definitions(jxl_dec-obj PUBLIC
  ${OBJ_COMPILE_DEFINITIONS}
)
target_link_libraries(jxl_dec-obj PUBLIC jxl_base)

# Object library. This is used to hold the set of objects and properties.
add_library(jxl_enc-obj OBJECT ${JPEGXL_INTERNAL_ENC_SOURCES})
target_compile_options(jxl_enc-obj PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jxl_enc-obj PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jxl_enc-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_enc-obj PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${JXL_HWY_INCLUDE_DIRS}
  $<TARGET_PROPERTY:brotlicommon,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(jxl_enc-obj PUBLIC
  ${OBJ_COMPILE_DEFINITIONS}
)
target_link_libraries(jxl_enc-obj PUBLIC jxl_base)

set_target_properties(jxl_dec-obj PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JXL_INTERNAL_LIBRARY_BUILD
)

set_target_properties(jxl_enc-obj PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JXL_INTERNAL_LIBRARY_BUILD
)

# Private static library. This exposes all the internal functions and is used
# for tests.
add_library(jxl_dec-internal STATIC
  $<TARGET_OBJECTS:jxl_dec-obj>
  ${JXL_CMS_OBJECTS}
)
target_link_libraries(jxl_dec-internal PUBLIC
  ${JPEGXL_COVERAGE_FLAGS}
  ${JPEGXL_DEC_INTERNAL_LIBS}
  jxl_base
)

# The list of objects in the static and shared libraries.
set(JPEGXL_INTERNAL_OBJECTS
  $<TARGET_OBJECTS:jxl_enc-obj>
  $<TARGET_OBJECTS:jxl_dec-obj>
)

# Private static library. This exposes all the internal functions and is used
# for tests.
# TODO(lode): once the source files are correctly split so that it is possible
# to do, remove $<TARGET_OBJECTS:jxl_dec-obj> here and depend on jxl_dec-internal
add_library(jxl-internal STATIC
  ${JPEGXL_INTERNAL_OBJECTS}
)
target_link_libraries(jxl-internal PUBLIC
  ${JPEGXL_COVERAGE_FLAGS}
  ${JPEGXL_INTERNAL_LIBS}
  jxl_cms
  jxl_base
)
target_include_directories(jxl-internal PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")

target_compile_definitions(jxl-internal INTERFACE -DJXL_STATIC_DEFINE)
target_compile_definitions(jxl_dec-internal INTERFACE -DJXL_STATIC_DEFINE)
target_compile_definitions(jxl-internal INTERFACE -DJXL_STATIC_DEFINE)
target_compile_definitions(jxl_dec-internal INTERFACE -DJXL_STATIC_DEFINE)

# TODO(deymo): Move TCMalloc linkage to the tools/ directory since the library
# shouldn't do any allocs anyway.
if(JPEGXL_ENABLE_TCMALLOC)
  pkg_check_modules(TCMallocMinimal REQUIRED IMPORTED_TARGET
      libtcmalloc_minimal)
  # tcmalloc 2.8 has concurrency issues that makes it sometimes return nullptr
  # for large allocs. See https://github.com/gperftools/gperftools/issues/1204
  # for details.
  if(TCMallocMinimal_VERSION VERSION_EQUAL 2.8)
    message(FATAL_ERROR
        "tcmalloc version 2.8 has a concurrency bug. You have installed "
        "version ${TCMallocMinimal_VERSION}, please either downgrade tcmalloc "
        "to version 2.7, upgrade to 2.8.1 or newer or pass "
        "-DJPEGXL_ENABLE_TCMALLOC=OFF to jpeg-xl cmake line. See the following "
        "bug for details:\n"
        "   https://github.com/gperftools/gperftools/issues/1204\n")
  endif()
  target_link_libraries(jxl-internal PUBLIC PkgConfig::TCMallocMinimal)
endif()  # JPEGXL_ENABLE_TCMALLOC

# Public library.
add_library(jxl ${JPEGXL_INTERNAL_OBJECTS})
strip_internal(JPEGXL_INTERNAL_SHARED_LIBS JPEGXL_INTERNAL_LIBS)
target_link_libraries(jxl PUBLIC ${JPEGXL_COVERAGE_FLAGS} jxl_base)
target_link_libraries(jxl PUBLIC jxl_cms)
target_link_libraries(jxl PRIVATE ${JPEGXL_INTERNAL_SHARED_LIBS})
set_target_properties(jxl PROPERTIES
  VERSION ${JPEGXL_LIBRARY_VERSION}
  SOVERSION ${JPEGXL_LIBRARY_SOVERSION})

# Public decoder library.
add_library(jxl_dec $<TARGET_OBJECTS:jxl_dec-obj>)
strip_internal(JPEGXL_DEC_INTERNAL_SHARED_LIBS JPEGXL_DEC_INTERNAL_LIBS)
target_link_libraries(jxl_dec PUBLIC ${JPEGXL_COVERAGE_FLAGS} jxl_base)
target_link_libraries(jxl_dec PRIVATE ${JPEGXL_DEC_INTERNAL_SHARED_LIBS})
set_target_properties(jxl_dec PROPERTIES
  VERSION ${JPEGXL_LIBRARY_VERSION}
  SOVERSION ${JPEGXL_LIBRARY_SOVERSION})

# Check whether the linker support excluding libs
set(LINKER_EXCLUDE_LIBS_FLAG "-Wl,--exclude-libs=ALL")
include(CheckCSourceCompiles)
list(APPEND CMAKE_EXE_LINKER_FLAGS ${LINKER_EXCLUDE_LIBS_FLAG})
check_c_source_compiles("int main(){return 0;}" LINKER_SUPPORT_EXCLUDE_LIBS)
list(REMOVE_ITEM CMAKE_EXE_LINKER_FLAGS ${LINKER_EXCLUDE_LIBS_FLAG})

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(jxl PUBLIC -DJXL_STATIC_DEFINE)
  target_compile_definitions(jxl_dec PUBLIC -DJXL_STATIC_DEFINE)
endif()

# Add a jxl.version file as a version script to tag symbols with the
# appropriate version number. This script is also used to limit what's exposed
# in the shared library from the static dependencies bundled here.
foreach(target IN ITEMS jxl jxl_dec)
  set_target_properties(${target} PROPERTIES
      LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/jxl/jxl.version)
  if(APPLE)
  set_property(TARGET ${target} APPEND_STRING PROPERTY
      LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/jxl/jxl_osx.syms")
  elseif(WIN32)
    # Nothing needed here, we use __declspec(dllexport) (jxl_export.h)
  else()
  set_property(TARGET ${target} APPEND_STRING PROPERTY
      LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/jxl/jxl.version")
  endif()  # APPLE
  # This hides the default visibility symbols from static libraries bundled into
  # the shared library. In particular this prevents exposing symbols from hwy
  # and skcms in the shared library.
  if(LINKER_SUPPORT_EXCLUDE_LIBS)
    set_property(TARGET ${target} APPEND_STRING PROPERTY
        LINK_FLAGS " ${LINKER_EXCLUDE_LIBS_FLAG}")
  endif()
endforeach()

# Only install libjxl public library. The libjxl_dec is not installed since it
# contains symbols also in libjxl which would conflict if programs try to use
# both.
install(TARGETS jxl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Add a pkg-config file for libjxl.
set(JPEGXL_LIBRARY_REQUIRES
    "libhwy libbrotlienc libbrotlidec libjxl_cms")

if (BUILD_SHARED_LIBS)
  set(JPEGXL_REQUIRES_TYPE "Requires.private")
else()
  set(JPEGXL_REQUIRES_TYPE "Requires")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jxl/libjxl.pc.in"
               "libjxl.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjxl.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
