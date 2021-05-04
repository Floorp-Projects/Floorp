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

#ifndef LIB_JXL_BASE_COMPILER_SPECIFIC_H_
#define LIB_JXL_BASE_COMPILER_SPECIFIC_H_

// Macros for compiler version + nonstandard keywords, e.g. __builtin_expect.

#include <stdint.h>

// #if is shorter and safer than #ifdef. *_VERSION are zero if not detected,
// otherwise 100 * major + minor version. Note that other packages check for
// #ifdef COMPILER_MSVC, so we cannot use that same name.

#ifdef _MSC_VER
#define JXL_COMPILER_MSVC _MSC_VER
#else
#define JXL_COMPILER_MSVC 0
#endif

#ifdef __GNUC__
#define JXL_COMPILER_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define JXL_COMPILER_GCC 0
#endif

#ifdef __clang__
#define JXL_COMPILER_CLANG (__clang_major__ * 100 + __clang_minor__)
// Clang pretends to be GCC for compatibility.
#undef JXL_COMPILER_GCC
#define JXL_COMPILER_GCC 0
#else
#define JXL_COMPILER_CLANG 0
#endif

#if JXL_COMPILER_MSVC
#define JXL_RESTRICT __restrict
#elif JXL_COMPILER_GCC || JXL_COMPILER_CLANG
#define JXL_RESTRICT __restrict__
#else
#define JXL_RESTRICT
#endif

#if JXL_COMPILER_MSVC
#define JXL_INLINE __forceinline
#define JXL_NOINLINE __declspec(noinline)
#else
#define JXL_INLINE inline __attribute__((always_inline))
#define JXL_NOINLINE __attribute__((noinline))
#endif

#if JXL_COMPILER_MSVC
#define JXL_NORETURN __declspec(noreturn)
#elif JXL_COMPILER_GCC || JXL_COMPILER_CLANG
#define JXL_NORETURN __attribute__((noreturn))
#endif

#if JXL_COMPILER_MSVC
#define JXL_UNREACHABLE __assume(false)
#elif JXL_COMPILER_CLANG || JXL_COMPILER_GCC >= 405
#define JXL_UNREACHABLE __builtin_unreachable()
#else
#define JXL_UNREACHABLE
#endif

#if JXL_COMPILER_MSVC
#define JXL_MAYBE_UNUSED
#else
// Encountered "attribute list cannot appear here" when using the C++17
// [[maybe_unused]], so only use the old style attribute for now.
#define JXL_MAYBE_UNUSED __attribute__((unused))
#endif

#if JXL_COMPILER_MSVC
// Unsupported, __assume is not the same.
#define JXL_LIKELY(expr) expr
#define JXL_UNLIKELY(expr) expr
#else
#define JXL_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define JXL_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#endif

#if JXL_COMPILER_MSVC
#include <intrin.h>

#pragma intrinsic(_ReadWriteBarrier)
#define JXL_COMPILER_FENCE _ReadWriteBarrier()
#elif JXL_COMPILER_GCC || JXL_COMPILER_CLANG
#define JXL_COMPILER_FENCE asm volatile("" : : : "memory")
#else
#define JXL_COMPILER_FENCE
#endif

// Returns a void* pointer which the compiler then assumes is N-byte aligned.
// Example: float* JXL_RESTRICT aligned = (float*)JXL_ASSUME_ALIGNED(in, 32);
//
// The assignment semantics are required by GCC/Clang. ICC provides an in-place
// __assume_aligned, whereas MSVC's __assume appears unsuitable.
#if JXL_COMPILER_CLANG
// Early versions of Clang did not support __builtin_assume_aligned.
#define JXL_HAS_ASSUME_ALIGNED __has_builtin(__builtin_assume_aligned)
#elif JXL_COMPILER_GCC
#define JXL_HAS_ASSUME_ALIGNED 1
#else
#define JXL_HAS_ASSUME_ALIGNED 0
#endif

#if JXL_HAS_ASSUME_ALIGNED
#define JXL_ASSUME_ALIGNED(ptr, align) __builtin_assume_aligned((ptr), (align))
#else
#define JXL_ASSUME_ALIGNED(ptr, align) (ptr) /* not supported */
#endif

#ifdef __has_attribute
#define JXL_HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define JXL_HAVE_ATTRIBUTE(x) 0
#endif

// Raises warnings if the function return value is unused. Should appear as the
// first part of a function definition/declaration.
#if JXL_HAVE_ATTRIBUTE(nodiscard)
#define JXL_MUST_USE_RESULT [[nodiscard]]
#elif JXL_COMPILER_CLANG && JXL_HAVE_ATTRIBUTE(warn_unused_result)
#define JXL_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define JXL_MUST_USE_RESULT
#endif

#if JXL_HAVE_ATTRIBUTE(__format__)
#define JXL_FORMAT(idx_fmt, idx_arg) \
  __attribute__((__format__(__printf__, idx_fmt, idx_arg)))
#else
#define JXL_FORMAT(idx_fmt, idx_arg)
#endif

#if JXL_COMPILER_MSVC
using ssize_t = intptr_t;
#endif

#endif  // LIB_JXL_BASE_COMPILER_SPECIFIC_H_
