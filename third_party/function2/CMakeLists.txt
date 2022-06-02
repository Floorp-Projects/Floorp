cmake_minimum_required(VERSION 3.11)
project(function2 VERSION 4.2.0 LANGUAGES CXX)

if (NOT FU2_IS_FIND_INCLUDED)
  string(COMPARE EQUAL ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}
                      FU2_IS_TOP_LEVEL_PROJECT)
endif()

if (FU2_IS_TOP_LEVEL_PROJECT)
  add_library(function2 INTERFACE)
else()
  add_library(function2 INTERFACE IMPORTED GLOBAL)
endif()

add_library(function2::function2 ALIAS function2)

target_include_directories(function2
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(function2
  INTERFACE
    cxx_alias_templates
    cxx_auto_type
    cxx_constexpr
    cxx_decltype
    cxx_decltype_auto
    cxx_final
    cxx_lambdas
    cxx_lambda_init_captures
    cxx_generic_lambdas
    cxx_variadic_templates
    cxx_defaulted_functions
    cxx_nullptr
    cxx_trailing_return_types
    cxx_return_type_deduction)

if (FU2_IS_TOP_LEVEL_PROJECT)
  include(ExternalProject)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  # Create an install target:
  # Headers and license files
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/function2"
          DESTINATION "include")
  install(FILES "LICENSE.txt" DESTINATION .)
  install(FILES "Readme.md" DESTINATION .)

  # Config.cmake
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

  # ConfigVersion.cmake
  configure_package_config_file("cmake/config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    # PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR
  )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

  # Targets.cmake
  export(TARGETS ${PROJECT_NAME}
         NAMESPACE ${PROJECT_NAME}::
         FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")
  install(TARGETS ${PROJECT_NAME}
          EXPORT "${PROJECT_NAME}Targets"
          INCLUDES DESTINATION "include")
  install(EXPORT "${PROJECT_NAME}Targets"
          NAMESPACE ${PROJECT_NAME}::
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

  # Setup CPack for bundling
  set(CPACK_GENERATOR "ZIP")
  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

  # Since the header only library is platform independent
  # we name the packages after the native line feed
  if(WIN32)
    set(CPACK_SYSTEM_NAME "crlf")
  else()
    set(CPACK_SYSTEM_NAME "lf")
  endif()

  include(CPack)

  if (MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  endif()

  include(cmake/CMakeLists.txt)

  include(CTest)

  option(FU2_WITH_NO_EXCEPTIONS
      "Test without exceptions"
      OFF)
  option(FU2_WITH_NO_DEATH_TESTS
      "Test without death tests"
      OFF)
  option(FU2_WITH_CPP_LATEST
      "Enable the highest C++ standard available for testing polyfills"
      OFF)
  option(FU2_WITH_LIMITED_EMPTY_PROPAGATION
      "Test limiting empty propagation to only function pointers, member pointers, std::function, and specializations of fu2::function_base"
      OFF)

  if (BUILD_TESTING)
    if (FU2_WITH_NO_EXCEPTIONS)
      message(STATUS "Testing with exceptions disabled")
      add_definitions(-DTESTS_NO_EXCEPTIONS)
    endif()

    if (FU2_WITH_NO_DEATH_TESTS)
      message(STATUS "Testing without death tests")
      add_definitions(-DTESTS_NO_DEATH_TESTS)
    endif()

    if (FU2_WITH_LIMITED_EMPTY_PROPAGATION)
      message(STATUS "Testing with limited empty propagation")
      add_definitions(-DFU2_WITH_LIMITED_EMPTY_PROPAGATION)
    endif()

    add_subdirectory(test)
  endif()
endif ()
