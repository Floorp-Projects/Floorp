/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains platform-specific typedefs and defines.
// Much of it is derived from Chromium's build/build_config.h.

#ifndef TYPEDEFS_H_
#define TYPEDEFS_H_

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define WEBRTC_ARCH_X86_FAMILY
#define WEBRTC_ARCH_X86_64
#define WEBRTC_ARCH_64_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#elif defined(__aarch64__)
#define WEBRTC_ARCH_ARM_FAMILY
#define WEBRTC_ARCH_64_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#elif defined(_M_IX86) || defined(__i386__)
#define WEBRTC_ARCH_X86_FAMILY
#define WEBRTC_ARCH_X86
#define WEBRTC_ARCH_32_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#elif defined(__ARMEL__)
#define WEBRTC_ARCH_ARM_FAMILY
#define WEBRTC_ARCH_32_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#elif defined(__powerpc64__)
#define WEBRTC_ARCH_PPC64 1
#define WEBRTC_ARCH_64_BITS 1
#ifdef __LITTLE_ENDIAN__
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#else
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#endif
#elif defined(__ppc__) || defined(__powerpc__)
#define WEBRTC_ARCH_PPC 1
#define WEBRTC_ARCH_32_BITS 1
#ifdef __LITTLE_ENDIAN__
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#else
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#endif
#elif defined(__sparc__) && defined(__arch64__)
#define WEBRTC_ARCH_SPARC 1
#define WEBRTC_ARCH_64_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__sparc__)
#define WEBRTC_ARCH_SPARC 1
#define WEBRTC_ARCH_32_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__mips__)
#define WEBRTC_ARCH_MIPS 1
#if defined(_ABI64) && _MIPS_SIM == _ABI64
#define WEBRTC_ARCH_64_BITS 1
#else
#define WEBRTC_ARCH_32_BITS 1
#endif
#if defined(__MIPSEB__)
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#else
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#endif
#elif defined(__hppa__)
#define WEBRTC_ARCH_HPPA 1
#define WEBRTC_ARCH_32_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__ia64__)
#define WEBRTC_ARCH_IA64 1
#define WEBRTC_ARCH_64_BITS 1
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#elif defined(__s390x__)
#define WEBRTC_ARCH_S390X 1
#define WEBRTC_ARCH_64_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__s390__)
#define WEBRTC_ARCH_S390 1
#define WEBRTC_ARCH_32_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__aarch64__) || defined(_M_ARM64)
#define WEBRTC_ARCH_AARCH64 1
#define WEBRTC_ARCH_64_BITS 1
#if defined(__AARCH64EL__) || defined(_M_ARM64)
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#elif defined(__AARCH64EB__)
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#endif
#elif defined(__alpha__)
#define WEBRTC_ARCH_ALPHA 1
#define WEBRTC_ARCH_64_BITS 1
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#elif defined(__avr32__)
#define WEBRTC_ARCH_AVR32 1
#define WEBRTC_ARCH_32_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__pnacl__)
#define WEBRTC_ARCH_32_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#else
#error Please add support for your architecture in typedefs.h
#endif

#if !(defined(WEBRTC_ARCH_LITTLE_ENDIAN) ^ defined(WEBRTC_ARCH_BIG_ENDIAN))
#error Define either WEBRTC_ARCH_LITTLE_ENDIAN or WEBRTC_ARCH_BIG_ENDIAN
#endif

// TODO(zhongwei.yao): WEBRTC_CPU_DETECTION is only used in one place; we should
// probably just remove it.
#if (defined(WEBRTC_ARCH_X86_FAMILY) && !defined(__SSE2__))
#define WEBRTC_CPU_DETECTION
#endif

#include <stdint.h>

// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() RTC_WARN_UNUSED_RESULT;
// To explicitly ignore a result, cast to void.
// TODO(kwiberg): Remove when we can use [[nodiscard]] from C++17.
#if defined(__clang__)
#define RTC_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#elif defined(__GNUC__)
// gcc has a __warn_unused_result__ attribute, but you can't quiet it by
// casting to void, so we don't use it.
#define RTC_WARN_UNUSED_RESULT
#else
#define RTC_WARN_UNUSED_RESULT
#endif

// Put after a variable that might not be used, to prevent compiler warnings:
//   int result ATTRIBUTE_UNUSED = DoSomething();
//   assert(result == 17);
// Deprecated since it only works with GCC & clang. See RTC_UNUSED below.
// TODO(terelius): Remove.
#ifndef ATTRIBUTE_UNUSED
#if defined(__GNUC__) || defined(__clang__)
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#else
#define ATTRIBUTE_UNUSED
#endif
#endif

// Macro to be used for switch-case fallthrough (required for enabling
// -Wimplicit-fallthrough warning on Clang).
#ifndef FALLTHROUGH
#if defined(__clang__)
#define FALLTHROUGH() [[clang::fallthrough]]
#else
#define FALLTHROUGH() do { } while (0)
#endif
#endif

#ifndef NO_RETURN
// Annotate a function that will not return control flow to the caller.
#if defined(_MSC_VER)
#define NO_RETURN __declspec(noreturn)
#elif defined(__GNUC__)
#define NO_RETURN __attribute__ ((__noreturn__))
#else
#define NO_RETURN
#endif
#endif

// Prevent the compiler from warning about an unused variable. For example:
//   int result = DoSomething();
//   assert(result == 17);
//   RTC_UNUSED(result);
// Note: In most cases it is better to remove the unused variable rather than
// suppressing the compiler warning.
#ifndef RTC_UNUSED
#define RTC_UNUSED(x) static_cast<void>(x)
#endif  // RTC_UNUSED

#endif  // TYPEDEFS_H_
