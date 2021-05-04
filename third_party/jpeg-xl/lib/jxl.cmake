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

# Lists all source files for the JPEG XL decoder library. These are also used
# by the encoder: the encoder uses both dec and enc ourse files, while the
# decoder uses only dec source files.
# TODO(lode): further prune these files and move to JPEGXL_INTERNAL_SOURCES_ENC:
#             only those files that the decoder absolutely needs, and or not
#             only for encoding, should be listed here.
set(JPEGXL_INTERNAL_SOURCES_DEC
  jxl/ac_context.h
  jxl/ac_strategy.cc
  jxl/ac_strategy.h
  jxl/alpha.cc
  jxl/alpha.h
  jxl/ans_common.cc
  jxl/ans_common.h
  jxl/ans_params.h
  jxl/aux_out.cc
  jxl/aux_out.h
  jxl/aux_out_fwd.h
  jxl/base/arch_macros.h
  jxl/base/bits.h
  jxl/base/byte_order.h
  jxl/base/cache_aligned.cc
  jxl/base/cache_aligned.h
  jxl/base/compiler_specific.h
  jxl/base/data_parallel.cc
  jxl/base/data_parallel.h
  jxl/base/descriptive_statistics.cc
  jxl/base/descriptive_statistics.h
  jxl/base/file_io.h
  jxl/base/iaca.h
  jxl/base/os_macros.h
  jxl/base/override.h
  jxl/base/padded_bytes.cc
  jxl/base/padded_bytes.h
  jxl/base/profiler.h
  jxl/base/robust_statistics.h
  jxl/base/span.h
  jxl/base/status.cc
  jxl/base/status.h
  jxl/base/thread_pool_internal.h
  jxl/base/time.cc
  jxl/base/time.h
  jxl/blending.cc
  jxl/blending.h
  jxl/chroma_from_luma.cc
  jxl/chroma_from_luma.h
  jxl/codec_in_out.h
  jxl/coeff_order.cc
  jxl/coeff_order.h
  jxl/coeff_order_fwd.h
  jxl/color_encoding_internal.cc
  jxl/color_encoding_internal.h
  jxl/color_management.cc
  jxl/color_management.h
  jxl/common.h
  jxl/compressed_dc.cc
  jxl/compressed_dc.h
  jxl/convolve-inl.h
  jxl/convolve.cc
  jxl/convolve.h
  jxl/dct-inl.h
  jxl/dct_block-inl.h
  jxl/dct_scales.cc
  jxl/dct_scales.h
  jxl/dct_util.h
  jxl/dec_ans.cc
  jxl/dec_ans.h
  jxl/dec_bit_reader.h
  jxl/dec_cache.h
  jxl/dec_context_map.cc
  jxl/dec_context_map.h
  jxl/dec_external_image.cc
  jxl/dec_external_image.h
  jxl/dec_frame.cc
  jxl/dec_frame.h
  jxl/dec_group.cc
  jxl/dec_group.h
  jxl/dec_group_border.cc
  jxl/dec_group_border.h
  jxl/dec_huffman.cc
  jxl/dec_huffman.h
  jxl/dec_modular.cc
  jxl/dec_modular.h
  jxl/dec_noise.cc
  jxl/dec_noise.h
  jxl/dec_params.h
  jxl/dec_patch_dictionary.cc
  jxl/dec_patch_dictionary.h
  jxl/dec_reconstruct.cc
  jxl/dec_reconstruct.h
  jxl/dec_transforms-inl.h
  jxl/dec_upsample.cc
  jxl/dec_upsample.h
  jxl/dec_xyb-inl.h
  jxl/dec_xyb.cc
  jxl/dec_xyb.h
  jxl/decode.cc
  jxl/enc_bit_writer.cc
  jxl/enc_bit_writer.h
  jxl/entropy_coder.cc
  jxl/entropy_coder.h
  jxl/epf.cc
  jxl/epf.h
  jxl/fast_math-inl.h
  jxl/field_encodings.h
  jxl/fields.cc
  jxl/fields.h
  jxl/filters.cc
  jxl/filters.h
  jxl/filters_internal.h
  jxl/frame_header.cc
  jxl/frame_header.h
  jxl/gauss_blur.cc
  jxl/gauss_blur.h
  jxl/headers.cc
  jxl/headers.h
  jxl/huffman_table.cc
  jxl/huffman_table.h
  jxl/icc_codec.cc
  jxl/icc_codec.h
  jxl/icc_codec_common.cc
  jxl/icc_codec_common.h
  jxl/image.cc
  jxl/image.h
  jxl/image_bundle.cc
  jxl/image_bundle.h
  jxl/image_metadata.cc
  jxl/image_metadata.h
  jxl/image_ops.h
  jxl/jpeg/dec_jpeg_data.cc
  jxl/jpeg/dec_jpeg_data.h
  jxl/jpeg/dec_jpeg_data_writer.cc
  jxl/jpeg/dec_jpeg_data_writer.h
  jxl/jpeg/dec_jpeg_output_chunk.h
  jxl/jpeg/dec_jpeg_serialization_state.h
  jxl/jpeg/jpeg_data.cc
  jxl/jpeg/jpeg_data.h
  jxl/jxl_inspection.h
  jxl/lehmer_code.h
  jxl/linalg.h
  jxl/loop_filter.cc
  jxl/loop_filter.h
  jxl/luminance.cc
  jxl/luminance.h
  jxl/memory_manager_internal.cc
  jxl/memory_manager_internal.h
  jxl/modular/encoding/context_predict.h
  jxl/modular/encoding/dec_ma.cc
  jxl/modular/encoding/dec_ma.h
  jxl/modular/encoding/encoding.cc
  jxl/modular/encoding/encoding.h
  jxl/modular/encoding/ma_common.h
  jxl/modular/modular_image.cc
  jxl/modular/modular_image.h
  jxl/modular/options.h
  jxl/modular/transform/near-lossless.h
  jxl/modular/transform/palette.h
  jxl/modular/transform/squeeze.h
  jxl/modular/transform/subtractgreen.h
  jxl/modular/transform/transform.cc
  jxl/modular/transform/transform.h
  jxl/noise.h
  jxl/noise_distributions.h
  jxl/opsin_params.cc
  jxl/opsin_params.h
  jxl/passes_state.cc
  jxl/passes_state.h
  jxl/patch_dictionary_internal.h
  jxl/quant_weights.cc
  jxl/quant_weights.h
  jxl/quantizer-inl.h
  jxl/quantizer.cc
  jxl/quantizer.h
  jxl/rational_polynomial-inl.h
  jxl/splines.cc
  jxl/splines.h
  jxl/toc.cc
  jxl/toc.h
  jxl/transfer_functions-inl.h
  jxl/transpose-inl.h
  jxl/xorshift128plus-inl.h
)

# List of source files only needed by the encoder or by tools (including
# decoding tools), but not by the decoder library.
set(JPEGXL_INTERNAL_SOURCES_ENC
  jxl/butteraugli/butteraugli.cc
  jxl/butteraugli/butteraugli.h
  jxl/butteraugli_wrapper.cc
  jxl/dec_file.cc
  jxl/dec_file.h
  jxl/enc_ac_strategy.cc
  jxl/enc_ac_strategy.h
  jxl/enc_adaptive_quantization.cc
  jxl/enc_adaptive_quantization.h
  jxl/enc_ans.cc
  jxl/enc_ans.h
  jxl/enc_ans_params.h
  jxl/enc_ar_control_field.cc
  jxl/enc_ar_control_field.h
  jxl/enc_butteraugli_comparator.cc
  jxl/enc_butteraugli_comparator.h
  jxl/enc_butteraugli_pnorm.cc
  jxl/enc_butteraugli_pnorm.h
  jxl/enc_cache.cc
  jxl/enc_cache.h
  jxl/enc_chroma_from_luma.cc
  jxl/enc_chroma_from_luma.h
  jxl/enc_cluster.cc
  jxl/enc_cluster.h
  jxl/enc_coeff_order.cc
  jxl/enc_coeff_order.h
  jxl/enc_color_management.cc
  jxl/enc_color_management.h
  jxl/enc_comparator.cc
  jxl/enc_comparator.h
  jxl/enc_context_map.cc
  jxl/enc_context_map.h
  jxl/enc_detect_dots.cc
  jxl/enc_detect_dots.h
  jxl/enc_dot_dictionary.cc
  jxl/enc_dot_dictionary.h
  jxl/enc_entropy_coder.cc
  jxl/enc_entropy_coder.h
  jxl/enc_external_image.cc
  jxl/enc_external_image.h
  jxl/enc_fast_heuristics.cc
  jxl/enc_file.cc
  jxl/enc_file.h
  jxl/enc_frame.cc
  jxl/enc_frame.h
  jxl/enc_gamma_correct.h
  jxl/enc_group.cc
  jxl/enc_group.h
  jxl/enc_heuristics.cc
  jxl/enc_heuristics.h
  jxl/enc_huffman.cc
  jxl/enc_huffman.h
  jxl/enc_icc_codec.cc
  jxl/enc_icc_codec.h
  jxl/enc_image_bundle.cc
  jxl/enc_image_bundle.h
  jxl/enc_modular.cc
  jxl/enc_modular.h
  jxl/enc_noise.cc
  jxl/enc_noise.h
  jxl/enc_params.h
  jxl/enc_patch_dictionary.cc
  jxl/enc_patch_dictionary.h
  jxl/enc_quant_weights.cc
  jxl/enc_quant_weights.h
  jxl/enc_splines.cc
  jxl/enc_splines.h
  jxl/enc_toc.cc
  jxl/enc_toc.h
  jxl/enc_transforms-inl.h
  jxl/enc_transforms.cc
  jxl/enc_transforms.h
  jxl/enc_xyb.cc
  jxl/enc_xyb.h
  jxl/encode.cc
  jxl/encode_internal.h
  jxl/gaborish.cc
  jxl/gaborish.h
  jxl/huffman_tree.cc
  jxl/huffman_tree.h
  jxl/jpeg/enc_jpeg_data.cc
  jxl/jpeg/enc_jpeg_data.h
  jxl/jpeg/enc_jpeg_data_reader.cc
  jxl/jpeg/enc_jpeg_data_reader.h
  jxl/jpeg/enc_jpeg_huffman_decode.cc
  jxl/jpeg/enc_jpeg_huffman_decode.h
  jxl/linalg.cc
  jxl/modular/encoding/enc_encoding.cc
  jxl/modular/encoding/enc_encoding.h
  jxl/modular/encoding/enc_ma.cc
  jxl/modular/encoding/enc_ma.h
  jxl/optimize.cc
  jxl/optimize.h
  jxl/progressive_split.cc
  jxl/progressive_split.h
)

set(JPEGXL_DEC_INTERNAL_LIBS
  brotlidec-static
  brotlicommon-static
  hwy
)

set(JPEGXL_INTERNAL_LIBS
  ${JPEGXL_DEC_INTERNAL_LIBS}
  brotlienc-static
  Threads::Threads
)

# strips the -static suffix from all the elements in LIST
function(strip_static OUTPUT_VAR LIB_LIST)
  foreach(lib IN LISTS ${LIB_LIST})
    string(REGEX REPLACE "-static$" "" lib "${lib}")
    list(APPEND out_list "${lib}")
  endforeach()
  set(${OUTPUT_VAR} ${out_list} PARENT_SCOPE)
endfunction()

if (JPEGXL_ENABLE_SKCMS)
  list(APPEND JPEGXL_INTERNAL_FLAGS -DJPEGXL_ENABLE_SKCMS=1)
  list(APPEND JPEGXL_INTERNAL_LIBS skcms)
else ()
  list(APPEND JPEGXL_INTERNAL_LIBS lcms2)
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

# Decoder-only object library
add_library(jxl_dec-obj OBJECT ${JPEGXL_INTERNAL_SOURCES_DEC})
target_compile_options(jxl_dec-obj PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jxl_dec-obj PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jxl_dec-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_dec-obj PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  $<TARGET_PROPERTY:hwy,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:brotlicommon-static,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(jxl_dec-obj PUBLIC
  ${OBJ_COMPILE_DEFINITIONS}
)

# Object library. This is used to hold the set of objects and properties.
add_library(jxl_enc-obj OBJECT ${JPEGXL_INTERNAL_SOURCES_ENC})
target_compile_options(jxl_enc-obj PRIVATE ${JPEGXL_INTERNAL_FLAGS})
target_compile_options(jxl_enc-obj PUBLIC ${JPEGXL_COVERAGE_FLAGS})
set_property(TARGET jxl_enc-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(jxl_enc-obj PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  $<TARGET_PROPERTY:hwy,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:brotlicommon-static,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(jxl_enc-obj PUBLIC
  ${OBJ_COMPILE_DEFINITIONS}
)

#TODO(lode): don't depend on CMS for the core library
if (JPEGXL_ENABLE_SKCMS)
  target_include_directories(jxl_enc-obj PRIVATE
    $<TARGET_PROPERTY:skcms,INCLUDE_DIRECTORIES>
  )
  target_include_directories(jxl_dec-obj PRIVATE
    $<TARGET_PROPERTY:skcms,INCLUDE_DIRECTORIES>
  )
else ()
  target_include_directories(jxl_enc-obj PRIVATE
    $<TARGET_PROPERTY:lcms2,INCLUDE_DIRECTORIES>
  )
  target_include_directories(jxl_dec-obj PRIVATE
    $<TARGET_PROPERTY:lcms2,INCLUDE_DIRECTORIES>
  )
endif ()

# Headers for exporting/importing public headers
include(GenerateExportHeader)
# TODO(deymo): Add these visibility properties to the static dependencies of
# jxl_{dec,enc}-obj since those are currently compiled with the default
# visibility.
set_target_properties(jxl_dec-obj PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JXL_INTERNAL_LIBRARY_BUILD
)
target_include_directories(jxl_dec-obj PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/include)

set_target_properties(jxl_enc-obj PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN 1
  DEFINE_SYMBOL JXL_INTERNAL_LIBRARY_BUILD
)
generate_export_header(jxl_enc-obj
  BASE_NAME JXL
  EXPORT_FILE_NAME include/jxl/jxl_export.h)
target_include_directories(jxl_enc-obj PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/include)

# Private static library. This exposes all the internal functions and is used
# for tests.
# TODO(lode): this library is missing symbols, more encoder-only code needs to
# be moved to JPEGXL_INTERNAL_SOURCES_ENC before this works
add_library(jxl_dec-static STATIC
  $<TARGET_OBJECTS:jxl_dec-obj>
)
target_link_libraries(jxl_dec-static
  PUBLIC ${JPEGXL_COVERAGE_FLAGS} ${JPEGXL_DEC_INTERNAL_LIBS} hwy)
target_include_directories(jxl_dec-static PUBLIC
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")

# Private static library. This exposes all the internal functions and is used
# for tests.
# TODO(lode): once the source files are correctly split so that it is possible
# to do, remove $<TARGET_OBJECTS:jxl_dec-obj> here and depend on jxl_dec-static
add_library(jxl-static STATIC
  $<TARGET_OBJECTS:jxl_enc-obj>
  $<TARGET_OBJECTS:jxl_dec-obj>
)
target_link_libraries(jxl-static
  PUBLIC ${JPEGXL_COVERAGE_FLAGS} ${JPEGXL_INTERNAL_LIBS} hwy)
target_include_directories(jxl-static PUBLIC
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")

# JXL_EXPORT is defined to "__declspec(dllimport)" automatically by CMake
# in Windows builds when including headers from the C API and compiling from
# outside the jxl library. This is required when using the shared library,
# however in windows this causes the function to not be found when linking
# against the static library. This define JXL_EXPORT= here forces it to not
# use dllimport in tests and other tools that require the static library.
target_compile_definitions(jxl-static INTERFACE -DJXL_EXPORT=)
target_compile_definitions(jxl_dec-static INTERFACE -DJXL_EXPORT=)

# TODO(deymo): Move TCMalloc linkage to the tools/ directory since the library
# shouldn't do any allocs anyway.
if(${JPEGXL_ENABLE_TCMALLOC})
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
  target_link_libraries(jxl-static PUBLIC PkgConfig::TCMallocMinimal)
endif()  # JPEGXL_ENABLE_TCMALLOC

# Install the static library too, but as jxl.a file without the -static except
# in Windows.
if (NOT WIN32)
  set_target_properties(jxl-static PROPERTIES OUTPUT_NAME "jxl")
  set_target_properties(jxl_dec-static PROPERTIES OUTPUT_NAME "jxl_dec")
endif()
install(TARGETS jxl-static DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS jxl_dec-static DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (((NOT DEFINED "${TARGET_SUPPORTS_SHARED_LIBS}") OR
     TARGET_SUPPORTS_SHARED_LIBS) AND NOT JPEGXL_STATIC)

# Public shared library.
add_library(jxl SHARED
  $<TARGET_OBJECTS:jxl_dec-obj>
  $<TARGET_OBJECTS:jxl_enc-obj>)
strip_static(JPEGXL_INTERNAL_SHARED_LIBS JPEGXL_INTERNAL_LIBS)
target_link_libraries(jxl PUBLIC ${JPEGXL_COVERAGE_FLAGS})
target_link_libraries(jxl PRIVATE ${JPEGXL_INTERNAL_SHARED_LIBS})
# Shared library include path contains only the "include/" paths.
target_include_directories(jxl PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")
set_target_properties(jxl PROPERTIES
  VERSION ${JPEGXL_LIBRARY_VERSION}
  SOVERSION ${JPEGXL_LIBRARY_SOVERSION}
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Public shared decoder library.
add_library(jxl_dec SHARED $<TARGET_OBJECTS:jxl_dec-obj>)
strip_static(JPEGXL_DEC_INTERNAL_SHARED_LIBS JPEGXL_DEC_INTERNAL_LIBS)
target_link_libraries(jxl_dec PUBLIC ${JPEGXL_COVERAGE_FLAGS})
target_link_libraries(jxl_dec PRIVATE ${JPEGXL_DEC_INTERNAL_SHARED_LIBS})
# Shared library include path contains only the "include/" paths.
target_include_directories(jxl_dec PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")
set_target_properties(jxl_dec PROPERTIES
  VERSION ${JPEGXL_LIBRARY_VERSION}
  SOVERSION ${JPEGXL_LIBRARY_SOVERSION}
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

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
endforeach()

# Only install libjxl shared library. The libjxl_dec is not installed since it
# contains symbols also in libjxl which would conflict if programs try to use
# both.
install(TARGETS jxl
  DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Add a pkg-config file for libjxl.
set(JPEGXL_LIBRARY_REQUIRES
    "libhwy libbrotlicommon libbrotlienc libbrotlidec")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/jxl/libjxl.pc.in"
               "libjxl.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjxl.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

else()
add_library(jxl ALIAS jxl-static)
add_library(jxl_dec ALIAS jxl_dec-static)
endif()  # TARGET_SUPPORTS_SHARED_LIBS AND NOT JPEGXL_STATIC
