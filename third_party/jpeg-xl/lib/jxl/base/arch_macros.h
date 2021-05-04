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

#ifndef LIB_JXL_BASE_ARCH_MACROS_H_
#define LIB_JXL_BASE_ARCH_MACROS_H_

// Defines the JXL_ARCH_* macros.

namespace jxl {

#if defined(__x86_64__) || defined(_M_X64)
#define JXL_ARCH_X64 1
#else
#define JXL_ARCH_X64 0
#endif

#if defined(__powerpc64__) || defined(_M_PPC)
#define JXL_ARCH_PPC 1
#else
#define JXL_ARCH_PPC 0
#endif

#if defined(__aarch64__) || defined(__arm__)
#define JXL_ARCH_ARM 1
#else
#define JXL_ARCH_ARM 0
#endif

}  // namespace jxl

#endif  // LIB_JXL_BASE_ARCH_MACROS_H_
