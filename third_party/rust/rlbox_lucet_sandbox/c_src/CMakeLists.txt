cmake_minimum_required(VERSION 3.13)

project(rlbox_lucet_testlib
        VERSION 0.1
        DESCRIPTION "RLBox integration with WASM modules compiled with lucet")

if(NOT DEFINED ENV{rlbox_SOURCE_DIR})
  message(FATAL_ERROR "Set rlbox_SOURCE_DIR environment variable")
endif()

if(NOT DEFINED ENV{wasiclang_SOURCE_DIR})
  message(FATAL_ERROR "Set wasiclang_SOURCE_DIR environment variable")
endif()

if(NOT DEFINED ENV{LUCET_WASI_DIR})
  message(FATAL_ERROR "Set LUCET_WASI_DIR environment variable")
endif()

if(NOT DEFINED ENV{LUCET_DIR})
  message(FATAL_ERROR "Set LUCET_DIR environment variable")
endif()

set(rlbox_SOURCE_DIR $ENV{rlbox_SOURCE_DIR})
set(wasiclang_SOURCE_DIR $ENV{wasiclang_SOURCE_DIR})
set(LUCET_WASI_DIR $ENV{LUCET_WASI_DIR})
set(LUCET_DIR $ENV{LUCET_DIR})

set(CMAKE_C_COMPILER ${wasiclang_SOURCE_DIR}/opt/wasi-sdk/bin/clang)
set(CMAKE_CXX_COMPILER ${wasiclang_SOURCE_DIR}/opt/wasi-sdk/bin/clang++)
set(CMAKE_BUILD_TYPE Release)

# Apply settings suitable for wasm module compilation
set(CMAKE_C_FLAGS
    "--sysroot ${wasiclang_SOURCE_DIR}/opt/wasi-sdk/share/wasi-sysroot/")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--export-all")

add_executable(glue_lib_lucet.wasm
               lucet_sandbox_wrapper.c
               ${rlbox_SOURCE_DIR}/code/tests/rlbox_glue/lib/libtest.c)

set(GLUE_LIB_PATH "${CMAKE_BINARY_DIR}/glue_lib_lucet.so")

add_custom_command(OUTPUT ${GLUE_LIB_PATH}
                   DEPENDS glue_lib_lucet.wasm
                   COMMAND ${LUCET_DIR}/lucetc
                           --bindings
                           ${LUCET_WASI_DIR}/bindings.json
                           --guard-size
                           "4GiB"
                           --min-reserved-size
                           "4GiB"
                           --max-reserved-size
                           "4GiB"
                           glue_lib_lucet.wasm
                           -o
                           ${GLUE_LIB_PATH}
                   COMMENT "Compiling wasm file to native")

add_custom_target(glue_lib_so ALL DEPENDS ${GLUE_LIB_PATH})
