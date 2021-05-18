cmake_minimum_required(VERSION 3.13)

project(rlbox_lucet
        VERSION 0.1
        DESCRIPTION "RLBox integration with WASM modules compiled with lucet")

# Project Settings ###################

# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DEV "Use settings suitable for dev contributions to rlbox" OFF)

file(GLOB_RECURSE
     ALL_CXX_SOURCE_FILES
     ${CMAKE_SOURCE_DIR}/include/*.[chi]pp
     ${CMAKE_SOURCE_DIR}/include/*.[chi]xx
     ${CMAKE_SOURCE_DIR}/include/*.cc
     ${CMAKE_SOURCE_DIR}/include/*.hh
     ${CMAKE_SOURCE_DIR}/include/*.ii
     ${CMAKE_SOURCE_DIR}/include/*.[CHI]
     ${CMAKE_SOURCE_DIR}/test/*.[chi]pp
     ${CMAKE_SOURCE_DIR}/test/*.[chi]xx
     ${CMAKE_SOURCE_DIR}/test/*.cc
     ${CMAKE_SOURCE_DIR}/test/*.hh
     ${CMAKE_SOURCE_DIR}/test/*.ii
     ${CMAKE_SOURCE_DIR}/test/*.[CHI]
     ${CMAKE_SOURCE_DIR}/c_src/*.[chi]pp
     ${CMAKE_SOURCE_DIR}/c_src/*.[chi]xx
     ${CMAKE_SOURCE_DIR}/c_src/*.cc
     ${CMAKE_SOURCE_DIR}/c_src/*.hh
     ${CMAKE_SOURCE_DIR}/c_src/*.ii
     ${CMAKE_SOURCE_DIR}/c_src/*.[CHI])

# Dev Tools ###################

if(DEV)
  if(MSVC)
    add_compile_options(/W4) # warnings
    add_compile_options(/WX) # warnings as errors
  else()
    add_compile_options(-Wall -Wextra -pedantic) # warnings
    add_compile_options(-Werror) # warnings as errors
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()

  find_program(CLANG_TIDY "clang-tidy")
  if(CLANG_TIDY)
    # Config in .clang-tidy
    set(CMAKE_CXX_CLANG_TIDY clang-tidy)
  endif()

  find_program(CLANG_FORMAT "clang-format")
  if(CLANG_FORMAT)
    # Config in .clang-format
    add_custom_target(format-source
                      COMMAND clang-format
                              -i
                              -style=file
                              ${ALL_CXX_SOURCE_FILES})
  endif()

endif()

# Dependencies ###################

find_program(CARGO "cargo")
if(!CARGO)
  message(
    FATAL_ERROR
      "Could not find cargo. Please install cargo as it is needed to build rust libraries."
    )
endif()

include(FetchContent)

FetchContent_Declare(
  rlbox
  GIT_REPOSITORY https://github.com/PLSysSec/rlbox_api_cpp17.git)
FetchContent_GetProperties(rlbox)
if(NOT rlbox_POPULATED)
  FetchContent_Populate(rlbox)
endif()

FetchContent_Declare(catch2
                     GIT_REPOSITORY https://github.com/catchorg/Catch2.git
                     GIT_TAG v2.9.1)
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
  FetchContent_Populate(catch2)
endif()

add_subdirectory("${catch2_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/contrib")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  FetchContent_Declare(
    wasiclang
    URL
      https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-11/wasi-sdk-11.0-macos.tar.gz
    )
else()
  FetchContent_Declare(
    wasiclang
    URL
      https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-11/wasi-sdk-11.0-linux.tar.gz
    )
endif()
FetchContent_GetProperties(wasiclang)
if(NOT wasiclang_POPULATED)
  FetchContent_Populate(wasiclang)
endif()

FetchContent_Declare(
  mod_lucet
  GIT_REPOSITORY https://github.com/PLSysSec/lucet_sandbox_compiler)
FetchContent_GetProperties(mod_lucet)
if(NOT mod_lucet_POPULATED)
  FetchContent_Populate(mod_lucet)
endif()

# Rust Lib ###################

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(DYLIB_EXT "dylib")
else()
  set(DYLIB_EXT "so")
endif()
set(LUCET_DIR "${CMAKE_BINARY_DIR}/cargo/release/")
set(LUCET_PATH "${LUCET_DIR}/lucetc")
file(GLOB_RECURSE LUCET_SOURCE_FILES ${mod_lucet_SOURCE_DIR}/*.rs ${mod_lucet_SOURCE_DIR}/*.S)

add_custom_command(OUTPUT ${LUCET_PATH}
                   DEPENDS ${LUCET_SOURCE_FILES}
                           ${mod_lucet_SOURCE_DIR}/Cargo.toml
                   COMMAND CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/cargo
                           ${CARGO}
                           build
                           --release
                   WORKING_DIRECTORY ${mod_lucet_SOURCE_DIR}
                   COMMENT "Building customized lucet compiler")
add_custom_target(lucet_release ALL DEPENDS ${LUCET_PATH})

# set(mod_lucet_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../lucet_sandbox_compiler")
# set(LUCET_DIR "${mod_lucet_SOURCE_DIR}/target/debug/")
# set(LUCET_PATH "${LUCET_DIR}/lucetc")

file(GLOB_RECURSE RUST_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/*.rs)

set(RUST_LIB_DEBUG_PATH
    "${CMAKE_BINARY_DIR}/cargo/debug/librlbox_lucet_sandbox.a")
add_custom_command(OUTPUT ${RUST_LIB_DEBUG_PATH}
                   DEPENDS ${RUST_SOURCE_FILES} ${LUCET_SOURCE_FILES} Cargo.toml
                   COMMAND CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/cargo ${CARGO}
                           build
                   COMMENT "Building librlbox_lucet_sandbox debug")
add_custom_target(lucet_sandbox_rustlib_debug ALL
                  DEPENDS ${RUST_LIB_DEBUG_PATH})

set(RUST_LIB_RELEASE_PATH
    "${CMAKE_BINARY_DIR}/cargo/release/librlbox_lucet_sandbox.a")
add_custom_command(OUTPUT ${RUST_LIB_RELEASE_PATH}
                   DEPENDS ${RUST_SOURCE_FILES} ${LUCET_SOURCE_FILES} Cargo.toml
                   COMMAND CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/cargo
                           ${CARGO}
                           build
                           --release
                   COMMENT "Building librlbox_lucet_sandbox release")
add_custom_target(lucet_sandbox_rustlib_release ALL
                  DEPENDS ${RUST_LIB_RELEASE_PATH})

add_library(lucet_sandbox_rustlib SHARED IMPORTED)
set_target_properties(lucet_sandbox_rustlib
                      PROPERTIES IMPORTED_LOCATION ${RUST_LIB_DEBUG_PATH})
set_target_properties(lucet_sandbox_rustlib
                      PROPERTIES IMPORTED_LOCATION_DEBUG ${RUST_LIB_DEBUG_PATH})
set_target_properties(lucet_sandbox_rustlib
                      PROPERTIES IMPORTED_LOCATION_RELEASE
                                 ${RUST_LIB_RELEASE_PATH})
# The wasi symbols needed by the wasm module are in this static lib, but must
# be part of the symbol table (locatable through dlsym). We need the following
# flag for this.
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
target_link_options(lucet_sandbox_rustlib INTERFACE "-framework" "Security" "-rdynamic")
else()
target_link_options(lucet_sandbox_rustlib INTERFACE "-rdynamic")
endif()
# Tests ###################

include(CTest)
include(Catch)

find_program(WASMCLANG "clang")

if(!WASMCLANG)
  message(
    FATAL_ERROR
      "Require clang with support for target 'wasm32-wasi' to build the WASM module"
    )
endif()

file(GLOB_RECURSE C_SOURCE_FILES ${CMAKE_SOURCE_DIR}/c_src/*)
set(GLUE_LIB_WASM "${CMAKE_BINARY_DIR}/wasm/glue_lib_lucet.wasm")
set(GLUE_LIB_SO "${CMAKE_BINARY_DIR}/wasm/glue_lib_lucet.${DYLIB_EXT}")

add_custom_command(OUTPUT ${GLUE_LIB_WASM} ${GLUE_LIB_SO}
                   DEPENDS ${C_SOURCE_FILES}
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/c_src
                   COMMAND rlbox_SOURCE_DIR=${rlbox_SOURCE_DIR}
                           wasiclang_SOURCE_DIR=${wasiclang_SOURCE_DIR}
                           LUCET_DIR=${LUCET_DIR}
                           LUCET_WASI_DIR=${mod_lucet_SOURCE_DIR}/lucet-wasi
                           ${CMAKE_COMMAND}
                           -S
                           .
                           -B
                           ${CMAKE_BINARY_DIR}/wasm
                   COMMAND VERBOSE=1
                           ${CMAKE_COMMAND}
                           --build
                           ${CMAKE_BINARY_DIR}/wasm
                           --target
                           all
                   COMMENT "Building wasm library")

add_custom_target(glue_lib_wasm ALL DEPENDS lucet_release ${GLUE_LIB_WASM} ${GLUE_LIB_SO})

add_executable(test_rlbox_glue test/test_lucet_sandbox_glue_main.cpp
                               test/test_lucet_sandbox_glue.cpp
                               test/test_lucet_sandbox_glue_preload.cpp
                               test/test_lucet_sandbox_glue_embedder_vars.cpp)
target_include_directories(test_rlbox_glue PUBLIC include)
target_include_directories(test_rlbox_glue
                           PUBLIC ${rlbox_SOURCE_DIR}/code/include)
target_include_directories(test_rlbox_glue
                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue)
target_include_directories(test_rlbox_glue
                           PUBLIC ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib)

find_package(Threads REQUIRED)

target_compile_definitions(test_rlbox_glue PUBLIC
                           GLUE_LIB_LUCET_PATH="${GLUE_LIB_SO}")

add_dependencies(test_rlbox_glue lucet_sandbox_rustlib_debug lucet_sandbox_rustlib_release)

if(MSVC)
  set(LINK_LUCET_LIB -WHOLEARCHIVE lucet_sandbox_rustlib)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(LINK_LUCET_LIB -Wl,-force_load lucet_sandbox_rustlib)
else()
  set(LINK_LUCET_LIB -Wl,--whole-archive lucet_sandbox_rustlib -Wl,--no-whole-archive)
endif()

target_link_libraries(test_rlbox_glue
                      Catch2::Catch2
                      ${LINK_LUCET_LIB}
                      ${CMAKE_THREAD_LIBS_INIT}
                      ${CMAKE_DL_LIBS}
                      # glue_lib_lucet
                      )

if(UNIX AND NOT (${CMAKE_SYSTEM_NAME} MATCHES "Darwin"))
  target_link_libraries(test_rlbox_glue rt)
endif()
catch_discover_tests(test_rlbox_glue)

# Shortcuts ###################

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V)
add_dependencies(check test_rlbox_glue glue_lib_wasm)

