/*
Copyright Charly Chevalier 2015
Copyright Joel Falcou 2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MSGPACK_PREDEF_HARDWARE_SIMD_ARM_H
#define MSGPACK_PREDEF_HARDWARE_SIMD_ARM_H

#include <msgpack/predef/version_number.h>
#include <msgpack/predef/hardware/simd/arm/versions.h>

/*`
 [heading `MSGPACK_HW_SIMD_ARM`]

 The SIMD extension for ARM (*if detected*).
 Version number depends on the most recent detected extension.

 [table
     [[__predef_symbol__] [__predef_version__]]

     [[`__ARM_NEON__`] [__predef_detection__]]
     [[`__aarch64__`] [__predef_detection__]]
     [[`_M_ARM`] [__predef_detection__]]
     ]

 [table
     [[__predef_symbol__] [__predef_version__]]

     [[`__ARM_NEON__`] [MSGPACK_HW_SIMD_ARM_NEON_VERSION]]
     [[`__aarch64__`] [MSGPACK_HW_SIMD_ARM_NEON_VERSION]]
     [[`_M_ARM`] [MSGPACK_HW_SIMD_ARM_NEON_VERSION]]
     ]

 */

#define MSGPACK_HW_SIMD_ARM MSGPACK_VERSION_NUMBER_NOT_AVAILABLE

#undef MSGPACK_HW_SIMD_ARM
#if !defined(MSGPACK_HW_SIMD_ARM) && (defined(__ARM_NEON__) || defined(__aarch64__) || defined (_M_ARM))
#   define MSGPACK_HW_SIMD_ARM MSGPACK_HW_SIMD_ARM_NEON_VERSION
#endif

#if !defined(MSGPACK_HW_SIMD_ARM)
#   define MSGPACK_HW_SIMD_ARM MSGPACK_VERSION_NUMBER_NOT_AVAILABLE
#else
#   define MSGPACK_HW_SIMD_ARM_AVAILABLE
#endif

#define MSGPACK_HW_SIMD_ARM_NAME "ARM SIMD"

#endif

#include <msgpack/predef/detail/test.h>
MSGPACK_PREDEF_DECLARE_TEST(MSGPACK_HW_SIMD_ARM, MSGPACK_HW_SIMD_ARM_NAME)
