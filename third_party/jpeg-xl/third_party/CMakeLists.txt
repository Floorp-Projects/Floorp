# Copyright (c) the JPEG XL Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Enable tests in third_party/ as well.
enable_testing()
include(CTest)

if(BUILD_TESTING)
# Add GTest from source and alias it to what the find_package(GTest) workflow
# defines. Omitting googletest/ directory would require it to be available in
# the base system instead, but it would work just fine. This makes packages
# using GTest and calling find_package(GTest) actually work.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/googletest/CMakeLists.txt" AND
    NOT JPEGXL_FORCE_SYSTEM_GTEST)
  add_subdirectory(googletest EXCLUDE_FROM_ALL)

  set(GTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest")
  set(GTEST_INCLUDE_DIR "$<TARGET_PROPERTY:INCLUDE_DIRECTORIES,gtest>"
      CACHE STRING "")
  set(GMOCK_INCLUDE_DIR "$<TARGET_PROPERTY:INCLUDE_DIRECTORIES,gmock>")
  set(GTEST_LIBRARY "$<TARGET_FILE:gtest>")
  set(GTEST_MAIN_LIBRARY "$<TARGET_FILE:gtest_main>")
  add_library(GTest::GTest ALIAS gtest)
  add_library(GTest::Main ALIAS gtest_main)

  set_target_properties(gtest PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  set_target_properties(gmock PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  set_target_properties(gtest_main PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  set_target_properties(gmock_main PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

  # googletest doesn't compile clean with clang-cl (-Wundef)
  if (WIN32 AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    set_target_properties(gtest PROPERTIES COMPILE_FLAGS "-Wno-error")
    set_target_properties(gmock PROPERTIES COMPILE_FLAGS "-Wno-error")
    set_target_properties(gtest_main PROPERTIES COMPILE_FLAGS "-Wno-error")
    set_target_properties(gmock_main PROPERTIES COMPILE_FLAGS "-Wno-error")
  endif ()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/googletest/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.googletest COPYONLY)
else()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/googletest/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.googletest COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()
find_package(GTest)
if (NOT GTEST_FOUND)
  set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
  message(SEND_ERROR "GTest not found. Install googletest package "
          "(libgtest-dev) in the system or download googletest to "
          "third_party/googletest from https://github.com/google/googletest ."
          "To disable tests instead re-run cmake with -DBUILD_TESTING=OFF.")
endif()  # NOT GTEST_FOUND

# Look for gmock in the system too.
if (NOT DEFINED GMOCK_INCLUDE_DIR)
  find_path(
      GMOCK_INCLUDE_DIR "gmock/gmock.h"
      HINTS ${GTEST_INCLUDE_DIRS})
  if ("${GMOCK_INCLUDE_DIR}" STREQUAL "GMOCK_INCLUDE_DIR-NOTFOUND")
    set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
    message(SEND_ERROR "GMock not found. Install googletest package "
            "(libgmock-dev) in the system or download googletest to "
            "third_party/googletest from https://github.com/google/googletest ."
            "To disable tests instead re-run cmake with -DBUILD_TESTING=OFF.")
  else()
    message(STATUS "Found GMock: ${GMOCK_INCLUDE_DIR}")
  endif()  # GMOCK_INCLUDE_DIR-NOTFOUND
endif()  # NOT DEFINED GMOCK_INCLUDE_DIR
endif()  # BUILD_TESTING

# Highway
set(HWY_SYSTEM_GTEST ON CACHE INTERNAL "")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/highway/CMakeLists.txt" AND
    NOT JPEGXL_FORCE_SYSTEM_HWY)
  add_subdirectory(highway)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/highway/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.highway COPYONLY)
else()
  pkg_check_modules(HWY libhwy)
  if (NOT HWY_FOUND)
    message(FATAL_ERROR
        "Highway library (hwy) not found. Install libhwy-dev or download it "
        "to third_party/highway from https://github.com/google/highway . "
        "Highway is required to build JPEG XL. You can run "
        "${PROJECT_SOURCE_DIR}/deps.sh to download this dependency.")
  endif()
  add_library(hwy INTERFACE IMPORTED GLOBAL)
  if(${CMAKE_VERSION} VERSION_LESS "3.13.5")
    set_property(TARGET hwy PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${HWY_INCLUDE_DIR})
    target_link_libraries(hwy INTERFACE ${HWY_LDFLAGS})
    set_property(TARGET hwy PROPERTY INTERFACE_COMPILE_OPTIONS ${HWY_CFLAGS_OTHER})
  else()
    target_include_directories(hwy INTERFACE ${HWY_INCLUDE_DIRS})
    target_link_libraries(hwy INTERFACE ${HWY_LINK_LIBRARIES})
    target_link_options(hwy INTERFACE ${HWY_LDFLAGS_OTHER})
    target_compile_options(hwy INTERFACE ${HWY_CFLAGS_OTHER})
  endif()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libhwy-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.highway COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
endif()

# lodepng
if( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lodepng/lodepng.h" )
  message(FATAL_ERROR "Please run ${PROJECT_SOURCE_DIR}/deps.sh to fetch the "
          "build dependencies.")
endif()
include(lodepng.cmake)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lodepng/LICENSE"
               ${PROJECT_BINARY_DIR}/LICENSE.lodepng COPYONLY)

# brotli
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/brotli/c/include/brotli/decode.h" OR
    JPEGXL_FORCE_SYSTEM_BROTLI)
  # Create the libbrotli* and libbrotli*-static targets.
  foreach(brlib IN ITEMS brotlienc brotlidec brotlicommon)
    # Use uppercase like "BROTLIENC" for the cmake variables
    string(TOUPPER "${brlib}" BRPREFIX)
    pkg_check_modules(${BRPREFIX} lib${brlib})
    if (${BRPREFIX}_FOUND)
      if(${CMAKE_VERSION} VERSION_LESS "3.13.5")
        add_library(${brlib} INTERFACE IMPORTED GLOBAL)
        set_property(TARGET ${brlib} PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${${BRPREFIX}_INCLUDE_DIR})
        target_link_libraries(${brlib} INTERFACE ${${BRPREFIX}_LDFLAGS})
        set_property(TARGET ${brlib} PROPERTY INTERFACE_COMPILE_OPTIONS ${${BRPREFIX}_CFLAGS_OTHER})

        add_library(${brlib}-static INTERFACE IMPORTED GLOBAL)
        set_property(TARGET ${brlib}-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${${BRPREFIX}_INCLUDE_DIR})
        target_link_libraries(${brlib}-static INTERFACE ${${BRPREFIX}_LDFLAGS})
        set_property(TARGET ${brlib}-static PROPERTY INTERFACE_COMPILE_OPTIONS ${${BRPREFIX}_CFLAGS_OTHER})
      else()
        add_library(${brlib} INTERFACE IMPORTED GLOBAL)
        target_include_directories(${brlib}
            INTERFACE ${${BRPREFIX}_INCLUDE_DIRS})
        target_link_libraries(${brlib}
            INTERFACE ${${BRPREFIX}_LINK_LIBRARIES})
        target_link_options(${brlib}
            INTERFACE ${${BRPREFIX}_LDFLAGS_OTHER})
        target_compile_options(${brlib}
            INTERFACE ${${BRPREFIX}_CFLAGS_OTHER})

        # TODO(deymo): Remove the -static library versions, this target is
        # currently needed by brunsli.cmake. When importing it this way, the
        # brotli*-static target is just an alias.
        add_library(${brlib}-static ALIAS ${brlib})
      endif()
    endif()
    unset(BRPREFIX)
  endforeach()

  if (BROTLIENC_FOUND AND BROTLIDEC_FOUND AND BROTLICOMMON_FOUND)
    set(BROTLI_FOUND 1)
  else()
    set(BROTLI_FOUND 0)
  endif()

  if (NOT BROTLI_FOUND)
    message(FATAL_ERROR
        "Brotli not found, install brotli-dev or download brotli source code to"
        " third_party/brotli from https://github.com/google/brotli. You can use"
        " ${PROJECT_SOURCE_DIR}/deps.sh to download this dependency.")
  endif ()
  if(JPEGXL_DEP_LICENSE_DIR)
    configure_file("${JPEGXL_DEP_LICENSE_DIR}/libbrotli-dev/copyright"
                   ${PROJECT_BINARY_DIR}/LICENSE.brotli COPYONLY)
  endif()  # JPEGXL_DEP_LICENSE_DIR
else()
  # Compile brotli from sources.
  set(BROTLI_DISABLE_TESTS ON CACHE STRING "Disable Brotli tests")
  add_subdirectory(brotli EXCLUDE_FROM_ALL)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/brotli/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.brotli COPYONLY)
  if(BROTLI_EMSCRIPTEN)
    # Brotli only defines the -static targets when using emscripten.
    foreach(brlib IN ITEMS brotlienc brotlidec brotlicommon)
      add_library(${brlib} ALIAS ${brlib}-static)
    endforeach()
  endif()  # BROTLI_EMSCRIPTEN
endif()

# *cms
if (JPEGXL_ENABLE_SKCMS)
  if( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/skcms/skcms.h" )
    message(FATAL_ERROR "Please run ${PROJECT_SOURCE_DIR}/deps.sh to fetch the "
            "build dependencies.")
  endif()
  include(skcms.cmake)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/skcms/LICENSE"
                 ${PROJECT_BINARY_DIR}/LICENSE.skcms COPYONLY)
endif ()
if (JPEGXL_ENABLE_VIEWERS OR NOT JPEGXL_ENABLE_SKCMS)
  if( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lcms/.git" )
    message(SEND_ERROR "Please run git submodule update --init")
  endif()
  include(lcms2.cmake)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lcms/COPYING"
                 ${PROJECT_BINARY_DIR}/LICENSE.lcms COPYONLY)
endif()

# sjpeg
if (JPEGXL_ENABLE_SJPEG)
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sjpeg/CMakeLists.txt")
    message(FATAL_ERROR "Please run ${PROJECT_SOURCE_DIR}/deps.sh to fetch the "
            "build dependencies.")
  endif()
  include(sjpeg.cmake)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sjpeg/COPYING"
                 ${PROJECT_BINARY_DIR}/LICENSE.sjpeg COPYONLY)
endif ()

