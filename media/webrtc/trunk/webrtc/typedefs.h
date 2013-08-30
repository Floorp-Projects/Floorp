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

#ifndef WEBRTC_TYPEDEFS_H_
#define WEBRTC_TYPEDEFS_H_

// For access to standard POSIXish features, use WEBRTC_POSIX instead of a
// more specific macro.
#if defined(WEBRTC_MAC) || defined(WEBRTC_LINUX) || \
    defined(WEBRTC_ANDROID) || defined(WEBRTC_BSD)
#define WEBRTC_POSIX
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
// TODO(andrew): replace WEBRTC_LITTLE_ENDIAN with WEBRTC_ARCH_LITTLE_ENDIAN.
#if defined(_M_X64) || defined(__x86_64__)
#define WEBRTC_ARCH_X86_FAMILY
#define WEBRTC_ARCH_X86_64
#define WEBRTC_ARCH_64_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#elif defined(_M_IX86) || defined(__i386__)
#define WEBRTC_ARCH_X86_FAMILY
#define WEBRTC_ARCH_X86
#define WEBRTC_ARCH_32_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#elif defined(__ARMEL__)
// TODO(andrew): We'd prefer to control platform defines here, but this is
// currently provided by the Android makefiles. Commented to avoid duplicate
// definition warnings.
//#define WEBRTC_ARCH_ARM
// TODO(andrew): Chromium uses the following two defines. Should we switch?
//#define WEBRTC_ARCH_ARM_FAMILY
//#define WEBRTC_ARCH_ARMEL
#define WEBRTC_ARCH_32_BITS
#define WEBRTC_ARCH_LITTLE_ENDIAN
#define WEBRTC_LITTLE_ENDIAN
#elif defined(__powerpc64__)
#define WEBRTC_ARCH_PPC64 1
#define WEBRTC_ARCH_64_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__ppc__) || defined(__powerpc__)
#define WEBRTC_ARCH_PPC 1
#define WEBRTC_ARCH_32_BITS 1
#define WEBRTC_ARCH_BIG_ENDIAN
#define WEBRTC_BIG_ENDIAN
#elif defined(__sparc64__)
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
#else
#error Please add support for your architecture in typedefs.h
#endif

#if defined(__SSE2__) || defined(_MSC_VER)
#define WEBRTC_USE_SSE2
#endif

#if !defined(_MSC_VER)
#include <stdint.h>
#else
// Define C99 equivalent types, since MSVC doesn't provide stdint.h.
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef __int64             int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned __int64    uint64_t;
#endif

// Borrowed from Chromium's base/compiler_specific.h.
// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void foo() OVERRIDE;
#if defined(_MSC_VER)
#define OVERRIDE override
#elif defined(__clang__)
// Clang defaults to C++03 and warns about using override. Squelch that.
// Intentionally no push/pop here so all users of OVERRIDE ignore the warning
// too. This is like passing -Wno-c++11-extensions, except that GCC won't die
// (because it won't see this pragma).
#pragma clang diagnostic ignored "-Wc++11-extensions"
#define OVERRIDE override
#else
#define OVERRIDE
#endif

#endif  // WEBRTC_TYPEDEFS_H_
