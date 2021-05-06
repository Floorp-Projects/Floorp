// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// @file decode_cxx.h
/// @brief C++ header-only helper for @ref decode.h.
///
/// There's no binary library associated with the header since this is a header
/// only library.

#ifndef JXL_DECODE_CXX_H_
#define JXL_DECODE_CXX_H_

#include <memory>

#include "jxl/decode.h"

#if !(defined(__cplusplus) || defined(c_plusplus))
#error "This a C++ only header. Use jxl/decode.h from C sources."
#endif

/// Struct to call JxlDecoderDestroy from the JxlDecoderPtr unique_ptr.
struct JxlDecoderDestroyStruct {
  /// Calls @ref JxlDecoderDestroy() on the passed decoder.
  void operator()(JxlDecoder* decoder) { JxlDecoderDestroy(decoder); }
};

/// std::unique_ptr<> type that calls JxlDecoderDestroy() when releasing the
/// decoder.
///
/// Use this helper type from C++ sources to ensure the decoder is destroyed and
/// their internal resources released.
typedef std::unique_ptr<JxlDecoder, JxlDecoderDestroyStruct> JxlDecoderPtr;

/// Creates an instance of JxlDecoder into a JxlDecoderPtr and initializes it.
///
/// This function returns a unique_ptr that will call JxlDecoderDestroy() when
/// releasing the pointer. See @ref JxlDecoderCreate for details on the
/// instance creation.
///
/// @param memory_manager custom allocator function. It may be NULL. The memory
///        manager will be copied internally.
/// @return a @c NULL JxlDecoderPtr if the instance can not be allocated or
///         initialized
/// @return initialized JxlDecoderPtr instance otherwise.
static inline JxlDecoderPtr JxlDecoderMake(
    const JxlMemoryManager* memory_manager) {
  return JxlDecoderPtr(JxlDecoderCreate(memory_manager));
}

#endif  // JXL_DECODE_CXX_H_
