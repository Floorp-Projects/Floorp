# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# codec_jpg is included always for loading of lossless reconstruction but
# decoding to pixels is only supported if libjpeg is found and
# JPEGXL_ENABLE_JPEG=1.
set(JPEGXL_EXTRAS_SOURCES
  extras/codec.cc
  extras/codec.h
  extras/codec_jpg.cc
  extras/codec_jpg.h
  extras/codec_pgx.cc
  extras/codec_pgx.h
  extras/codec_png.cc
  extras/codec_png.h
  extras/codec_pnm.cc
  extras/codec_pnm.h
  extras/codec_psd.cc
  extras/codec_psd.h
  extras/color_description.cc
  extras/color_description.h
  extras/color_hints.cc
  extras/color_hints.h
  extras/hlg.cc
  extras/hlg.h
  extras/packed_image.h
  extras/packed_image_convert.cc
  extras/packed_image_convert.h
  extras/time.cc
  extras/time.h
  extras/tone_mapping.cc
  extras/tone_mapping.h
)

# We only define a static library for jxl_extras since it uses internal parts
# of jxl library which are not accessible from outside the library in the
# shared library case.
add_library(jxl_extras-static STATIC EXCLUDE_FROM_ALL
  "${JPEGXL_EXTRAS_SOURCES}")
target_compile_options(jxl_extras-static PRIVATE "${JPEGXL_INTERNAL_FLAGS}")
set_property(TARGET jxl_extras-static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_extras-static PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(jxl_extras-static PUBLIC
  jxl-static
  lodepng
)

find_package(GIF 5.1)
if(GIF_FOUND)
  target_sources(jxl_extras-static PRIVATE
    extras/codec_gif.cc
    extras/codec_gif.h
  )
  target_include_directories(jxl_extras-static PUBLIC "${GIF_INCLUDE_DIRS}")
  target_link_libraries(jxl_extras-static PUBLIC ${GIF_LIBRARIES})
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_GIF=1)
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libgif-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libgif COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

find_package(JPEG)
if(JPEG_FOUND)
  target_include_directories(jxl_extras-static PUBLIC "${JPEG_INCLUDE_DIRS}")
  target_link_libraries(jxl_extras-static PUBLIC ${JPEG_LIBRARIES})
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_JPEG=1)
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libjpeg-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libjpeg COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

find_package(ZLIB)  # dependency of PNG
find_package(PNG)
if(PNG_FOUND AND ZLIB_FOUND)
  target_sources(jxl_extras-static PRIVATE
    extras/codec_apng.cc
    extras/codec_apng.h
  )
  target_include_directories(jxl_extras-static PUBLIC "${PNG_INCLUDE_DIRS}")
  target_link_libraries(jxl_extras-static PUBLIC ${PNG_LIBRARIES})
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_APNG=1)
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/zlib1g-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.zlib COPYONLY)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libpng-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libpng COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

if (JPEGXL_ENABLE_SJPEG)
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_SJPEG=1)
  target_link_libraries(jxl_extras-static PUBLIC sjpeg)
endif ()

if (JPEGXL_ENABLE_OPENEXR)
pkg_check_modules(OpenEXR IMPORTED_TARGET OpenEXR)
if (OpenEXR_FOUND)
  target_sources(jxl_extras-static PRIVATE
    extras/codec_exr.cc
    extras/codec_exr.h
  )
  target_compile_definitions(jxl_extras-static PUBLIC -DJPEGXL_ENABLE_EXR=1)
  target_link_libraries(jxl_extras-static PUBLIC PkgConfig::OpenEXR)
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libopenexr-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.libopenexr COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
  # OpenEXR generates exceptions, so we need exception support to catch them.
  # Actually those flags counteract the ones set in JPEGXL_INTERNAL_FLAGS.
  if (NOT WIN32)
    set_source_files_properties(extras/codec_exr.cc PROPERTIES COMPILE_FLAGS -fexceptions)
    if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
      set_source_files_properties(extras/codec_exr.cc PROPERTIES COMPILE_FLAGS -fcxx-exceptions)
    endif()
  endif()
endif() # OpenEXR_FOUND
endif() # JPEGXL_ENABLE_OPENEXR
