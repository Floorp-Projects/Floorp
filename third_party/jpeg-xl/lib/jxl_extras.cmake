# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

include(compatibility.cmake)
include(jxl_lists.cmake)

list(APPEND JPEGXL_EXTRAS_CORE_SOURCES
  "${JPEGXL_INTERNAL_EXTRAS_SOURCES}"
  "${JPEGXL_INTERNAL_CODEC_JXL_SOURCES}"
  "${JPEGXL_INTERNAL_CODEC_PGX_SOURCES}"
  "${JPEGXL_INTERNAL_CODEC_PNM_SOURCES}"
  "${JPEGXL_INTERNAL_CODEC_NPY_SOURCES}"
)

add_library(jxl_extras_codec-obj OBJECT "${JPEGXL_EXTRAS_CORE_SOURCES}")
target_compile_options(jxl_extras_codec-obj PRIVATE "${JPEGXL_INTERNAL_FLAGS}")
target_compile_definitions(jxl_extras_codec-obj PRIVATE -DJXL_EXPORT=)
set_property(TARGET jxl_extras_codec-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_extras_codec-obj PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${JXL_HWY_INCLUDE_DIRS}
)
set(JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES)
set(JXL_EXTRAS_CODEC_PUBLIC_COMPILE_DEFINITIONS)

# We only define a static library for jxl_extras since it uses internal parts
# of jxl library which are not accessible from outside the library in the
# shared library case.
add_library(jxl_extras-static STATIC EXCLUDE_FROM_ALL
  "${JPEGXL_EXTRAS_CORE_SOURCES}"
  "${JPEGXL_INTERNAL_EXTRAS_FOR_TOOLS_SOURCES}"
)
target_compile_options(jxl_extras-static PRIVATE "${JPEGXL_INTERNAL_FLAGS}")
set_property(TARGET jxl_extras-static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_extras-static PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(jxl_extras-static PUBLIC
  jxl-static
  jxl_threads-static
)

# Define an extras library that does not have the image codecs, only the core
# extras code. This is needed for some of the fuzzers.
add_library(jxl_extras_nocodec-static STATIC EXCLUDE_FROM_ALL
  "${JPEGXL_EXTRAS_CORE_SOURCES}"
  "${JPEGXL_INTERNAL_EXTRAS_FOR_TOOLS_SOURCES}"
)
target_compile_options(jxl_extras_nocodec-static PRIVATE "${JPEGXL_INTERNAL_FLAGS}")
set_property(TARGET jxl_extras_nocodec-static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_extras_nocodec-static PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(jxl_extras_nocodec-static PUBLIC
  jxl-static
  jxl_threads-static
)

find_package(GIF 5.1)
if(GIF_FOUND)
  target_sources(jxl_extras_codec-obj PRIVATE
    extras/dec/gif.cc
    extras/dec/gif.h
  )
  target_include_directories(jxl_extras_codec-obj PRIVATE "${GIF_INCLUDE_DIRS}")
  list(APPEND JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES ${GIF_LIBRARIES})
  list(APPEND JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS -DJPEGXL_ENABLE_GIF=1)
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libgif-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libgif COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

find_package(JPEG)
if(JPEG_FOUND)
  target_sources(jxl_extras_codec-obj PRIVATE
    "${JPEGXL_INTERNAL_CODEC_JPG_SOURCES}"
  )
  target_include_directories(jxl_extras_codec-obj PRIVATE "${JPEG_INCLUDE_DIRS}")
  list(APPEND JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES ${JPEG_LIBRARIES})
  list(APPEND JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS -DJPEGXL_ENABLE_JPEG=1)
  target_sources(jxl_extras-static PRIVATE
    "${JPEGXL_INTERNAL_CODEC_JPG_SOURCES}"
  )
  target_include_directories(jxl_extras-static PRIVATE "${JPEG_INCLUDE_DIRS}")
  target_link_libraries(jxl_extras-static PRIVATE ${JPEG_LIBRARIES})
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_JPEG=1)
  if(JPEGXL_ENABLE_JPEGLI)
    target_sources(jxl_extras-static PRIVATE
      "${JPEGXL_INTERNAL_CODEC_JPEGLI_SOURCES}"
    )
    target_link_libraries(jxl_extras-static PRIVATE jpegli-static)
    target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_JPEGLI=1)
  endif()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libjpeg-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libjpeg COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

if(NOT JPEGXL_BUNDLE_LIBPNG)
  find_package(PNG)
endif()
if(PNG_FOUND)
  target_sources(jxl_extras_codec-obj PRIVATE
    "${JPEGXL_INTERNAL_CODEC_APNG_SOURCES}"
  )
  target_include_directories(jxl_extras_codec-obj PRIVATE "${PNG_INCLUDE_DIRS}")
  list(APPEND JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES ${PNG_LIBRARIES})
  list(APPEND JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS -DJPEGXL_ENABLE_APNG=1)
  target_sources(jxl_extras-static PRIVATE
    "${JPEGXL_INTERNAL_CODEC_APNG_SOURCES}"
  )
  target_include_directories(jxl_extras-static PUBLIC "${PNG_INCLUDE_DIRS}")
  target_link_libraries(jxl_extras-static PUBLIC ${PNG_LIBRARIES})
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_APNG=1)
  configure_file(extras/LICENSE.apngdis
                 ${PROJECT_BINARY_DIR}/LICENSE.apngdis COPYONLY)
endif()

if (JPEGXL_ENABLE_SJPEG)
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_SJPEG=1)
  target_link_libraries(jxl_extras-static PRIVATE sjpeg)
endif ()

if (JPEGXL_ENABLE_OPENEXR)
pkg_check_modules(OpenEXR IMPORTED_TARGET OpenEXR)
if (OpenEXR_FOUND)
  target_sources(jxl_extras_codec-obj PRIVATE
    "${JPEGXL_INTERNAL_CODEC_EXR_SOURCES}"
  )
  list(APPEND JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS -DJPEGXL_ENABLE_EXR=1)
  target_include_directories(jxl_extras_codec-obj PRIVATE "${OpenEXR_INCLUDE_DIRS}")
  list(APPEND JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES PkgConfig::OpenEXR)
  target_sources(jxl_extras-static PRIVATE
    "${JPEGXL_INTERNAL_CODEC_EXR_SOURCES}"
  )
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_EXR=1)
  target_link_libraries(jxl_extras-static PRIVATE PkgConfig::OpenEXR)
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libopenexr-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libopenexr COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
  # OpenEXR generates exceptions, so we need exception support to catch them.
  # Actually those flags counteract the ones set in JPEGXL_INTERNAL_FLAGS.
  if (NOT WIN32)
    set_source_files_properties(extras/dec/exr.cc extras/enc/exr.cc PROPERTIES COMPILE_FLAGS -fexceptions)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      set_source_files_properties(extras/dec/exr.cc extras/enc/exr.cc PROPERTIES COMPILE_FLAGS -fcxx-exceptions)
    endif()
  endif()
endif() # OpenEXR_FOUND
endif() # JPEGXL_ENABLE_OPENEXR

target_compile_definitions(jxl_extras_codec-obj PRIVATE ${JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS})

### Static library.
add_library(jxl_extras_codec-static STATIC $<TARGET_OBJECTS:jxl_extras_codec-obj>)
target_compile_definitions(jxl_extras_codec-static PUBLIC ${JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS})
target_link_libraries(jxl_extras_codec-static PRIVATE ${JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES} jxl)

### Shared library.
if (BUILD_SHARED_LIBS)
add_library(jxl_extras_codec SHARED $<TARGET_OBJECTS:jxl_extras_codec-obj>)
target_compile_definitions(jxl_extras_codec PUBLIC ${JXL_EXTRAS_CODEC_PUBLIC_DEFINITIONS})
target_link_libraries(jxl_extras_codec PRIVATE ${JXL_EXTRAS_CODEC_INTERNAL_LIBRARIES} jxl)
else()
add_library(jxl_extras_codec ALIAS jxl_extras_codec-static)
endif()  # BUILD_SHARED_LIBS
