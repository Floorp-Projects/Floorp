# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

include(jxl_lists.cmake)

# Headers for exporting/importing public headers
include(GenerateExportHeader)

add_library(jxl_cms
  ${JPEGXL_INTERNAL_CMS_SOURCES}
)
target_compile_options(jxl_cms PRIVATE "${JPEGXL_INTERNAL_FLAGS}")
set_target_properties(jxl_cms PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN 1)
target_link_libraries(jxl_cms PUBLIC jxl_base)
target_include_directories(jxl_cms PRIVATE
  ${JXL_HWY_INCLUDE_DIRS}
)
generate_export_header(jxl_cms
  BASE_NAME JXL_CMS
  EXPORT_FILE_NAME include/jxl/jxl_cms_export.h)
target_include_directories(jxl_cms BEFORE PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")

set(JPEGXL_CMS_LIBRARY_REQUIRES "")

if (JPEGXL_ENABLE_SKCMS)
  target_link_skcms(jxl_cms)
else()
  target_link_libraries(jxl_cms PRIVATE lcms2)
  if (JPEGXL_FORCE_SYSTEM_LCMS2)
    set(JPEGXL_CMS_LIBRARY_REQUIRES "lcms2")
  endif()
endif()

target_link_libraries(jxl_cms PRIVATE hwy)

set_target_properties(jxl_cms PROPERTIES
        VERSION ${JPEGXL_LIBRARY_VERSION}
        SOVERSION ${JPEGXL_LIBRARY_SOVERSION})

# Check whether the linker support excluding libs
set(LINKER_EXCLUDE_LIBS_FLAG "-Wl,--exclude-libs=ALL")
include(CheckCSourceCompiles)
list(APPEND CMAKE_EXE_LINKER_FLAGS ${LINKER_EXCLUDE_LIBS_FLAG})
check_c_source_compiles("int main(){return 0;}" LINKER_SUPPORT_EXCLUDE_LIBS)
list(REMOVE_ITEM CMAKE_EXE_LINKER_FLAGS ${LINKER_EXCLUDE_LIBS_FLAG})

if(LINKER_SUPPORT_EXCLUDE_LIBS)
  set_property(TARGET jxl_cms APPEND_STRING PROPERTY
      LINK_FLAGS " ${LINKER_EXCLUDE_LIBS_FLAG}")
endif()

install(TARGETS jxl_cms
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (BUILD_SHARED_LIBS)
  set(JPEGXL_REQUIRES_TYPE "Requires.private")
  set(JPEGXL_CMS_PRIVATE_LIBS "-lm ${PKGCONFIG_CXX_LIB}")
else()
  set(JPEGXL_REQUIRES_TYPE "Requires")
  set(JPEGXL_CMS_PRIVATE_LIBS "-lm ${PKGCONFIG_CXX_LIB}")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jxl/libjxl_cms.pc.in"
               "libjxl_cms.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjxl_cms.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
