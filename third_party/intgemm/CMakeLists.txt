cmake_minimum_required(VERSION 3.5)
project(intgemm)
string(ASCII 27 Esc)
set(Orange "${Esc}[33m")
set(ColourReset "${Esc}[m")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 11)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-unknown-pragmas)
  if (COMPILE_WASM)
    # Disabling Pthreads + memory growth warning to be an error for WASM
    # Pthreads + memory growth causes JS accessing the wasm memory to be slow
    # https://github.com/WebAssembly/design/issues/1271
    add_compile_options(-Wno-error=pthreads-mem-growth)
  endif()
endif()

# Check if compiler supports AVX2 (this should only catch emscripten)
try_compile(INTGEMM_COMPILER_SUPPORTS_AVX2
  ${CMAKE_CURRENT_BINARY_DIR}/compile_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/compile_test/avx2.cc)

# Check if compiler supports AVX512BW
try_compile(INTGEMM_COMPILER_SUPPORTS_AVX512BW
  ${CMAKE_CURRENT_BINARY_DIR}/compile_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/compile_test/avx512bw.cc)

# Check if the compiler supports AVX512VNNI
try_compile(INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
  ${CMAKE_CURRENT_BINARY_DIR}/compile_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/compile_test/avx512vnni.cc)

if (NOT INTGEMM_COMPILER_SUPPORTS_AVX2 OR NOT INTGEMM_COMPILER_SUPPORTS_AVX512BW OR NOT INTGEMM_COMPILER_SUPPORTS_AVX512VNNI)
  set(UNSUPPORTED "Your compiler is too old to support")
  if (NOT INTGEMM_COMPILER_SUPPORTS_AVX2)
    set(UNSUPPORTED "${UNSUPPORTED} AVX2")
  endif()
  if (NOT INTGEMM_COMPILER_SUPPORTS_AVX512BW)
    set(UNSUPPORTED "${UNSUPPORTED} AVX512BW")
  endif()
  if (NOT INTGEMM_COMPILER_SUPPORTS_AVX512VNNI)
    set(UNSUPPORTED "${UNSUPPORTED} AVX512VNNI")
  endif()
  message(WARNING "${Orange}${UNSUPPORTED}.  Multiplication will be slower on CPUs that support these instructions. For details rerun cmake with --debug-trycompile then try to build in compile_tests/CMakeFiles/CMakeTmp.${ColourReset}")
endif()


add_library(intgemm STATIC intgemm/intgemm.cc)

# Generate configure file
configure_file(intgemm/intgemm_config.h.in intgemm/intgemm_config.h)
#Ensure it is included by users.
include_directories(${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(intgemm PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# This isn't necessary since intgemm uses entirely relative paths but source code depending on it may want to #include <intgemm/intgemm.h>
target_include_directories(intgemm INTERFACE .)

option(USE_OPENMP "Use OpenMP" OFF)
if (USE_OPENMP)
  message(STATUS "Compiling with OpenMP")
  find_package(OpenMP)
  if (NOT ${OpenMP_CXX_FOUND})
    message(SEND_ERROR "OpenMP requested but C++ support not found")
  endif()
  add_compile_options(${OpenMP_CXX_FLAGS})
  target_link_libraries(intgemm PUBLIC OpenMP::OpenMP_CXX)
endif()

if (COMPILE_WASM)
    # A compile defintion to compile intgemm on WASM platform
    target_compile_definitions(intgemm PUBLIC WASM)
endif()

option(WORMHOLE "Use WASM wormhole https://bugzilla.mozilla.org/show_bug.cgi?id=1672160" OFF)
if (WORMHOLE)
  target_compile_definitions(intgemm PUBLIC INTGEMM_WORMHOLE)
endif()

option(INTGEMM_CPUID_ENVIRONMENT "Allow INTGEMM_CPUID environment variable to downgrade CPU model, which is mainly for testing." ON)
if (INTGEMM_CPUID_ENVIRONMENT)
  target_compile_definitions(intgemm PRIVATE INTGEMM_CPUID_ENVIRONMENT)
endif()

if(INTGEMM_DONT_BUILD_TESTS)
  return()
endif()

foreach(exe benchmark biasmultiply benchmark_quantizer)
  add_executable(${exe} benchmarks/${exe}.cc)
  target_link_libraries(${exe} intgemm)
endforeach()

add_executable(example example.cc)
target_link_libraries(example intgemm)

add_executable(tests
  test/test.cc

  # General tests
  test/add127_test.cc
  test/multiply_test.cc
  test/prepare_b_quantized_transposed.cc
  test/prepare_b_transposed.cc
  test/quantize_test.cc
  test/utils_test.cc

  # Kernels tests
  test/kernels/add_bias_test.cc
  test/kernels/bitwise_not_test.cc
  test/kernels/downcast_test.cc
  test/kernels/exp_test.cc
  test/kernels/floor_test.cc
  test/kernels/multiply_test.cc
  test/kernels/quantize_test.cc
  test/kernels/relu_test.cc
  test/kernels/rescale_test.cc
  test/kernels/sigmoid_test.cc
  test/kernels/tanh_test.cc
  test/kernels/unquantize_test.cc
  test/kernels/upcast_test.cc
  test/kernels/write_test.cc
)
target_link_libraries(tests intgemm)

#CTest integration with Catch2
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/Catch.cmake)
include(CTest)
catch_discover_tests(tests)
