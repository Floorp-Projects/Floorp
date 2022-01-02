/*
Copyright Charly Chevalier 2015
Copyright Joel Falcou 2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MSGPACK_PREDEF_HARDWARE_SIMD_PPC_H
#define MSGPACK_PREDEF_HARDWARE_SIMD_PPC_H

#include <msgpack/predef/version_number.h>
#include <msgpack/predef/hardware/simd/ppc/versions.h>

/*`
 [heading `MSGPACK_HW_SIMD_PPC`]

 The SIMD extension for PowerPC (*if detected*).
 Version number depends on the most recent detected extension.

 [table
     [[__predef_symbol__] [__predef_version__]]

     [[`__VECTOR4DOUBLE__`] [__predef_detection__]]

     [[`__ALTIVEC__`] [__predef_detection__]]
     [[`__VEC__`] [__predef_detection__]]

     [[`__VSX__`] [__predef_detection__]]
     ]

 [table
     [[__predef_symbol__] [__predef_version__]]

     [[`__VECTOR4DOUBLE__`] [MSGPACK_HW_SIMD_PPC_QPX_VERSION]]

     [[`__ALTIVEC__`] [MSGPACK_HW_SIMD_PPC_VMX_VERSION]]
     [[`__VEC__`] [MSGPACK_HW_SIMD_PPC_VMX_VERSION]]

     [[`__VSX__`] [MSGPACK_HW_SIMD_PPC_VSX_VERSION]]
     ]

 */

#define MSGPACK_HW_SIMD_PPC MSGPACK_VERSION_NUMBER_NOT_AVAILABLE

#undef MSGPACK_HW_SIMD_PPC
#if !defined(MSGPACK_HW_SIMD_PPC) && defined(__VECTOR4DOUBLE__)
#   define MSGPACK_HW_SIMD_PPC MSGPACK_HW_SIMD_PPC_QPX_VERSION
#endif
#if !defined(MSGPACK_HW_SIMD_PPC) && defined(__VSX__)
#   define MSGPACK_HW_SIMD_PPC MSGPACK_HW_SIMD_PPC_VSX_VERSION
#endif
#if !defined(MSGPACK_HW_SIMD_PPC) && (defined(__ALTIVEC__) || defined(__VEC__))
#   define MSGPACK_HW_SIMD_PPC MSGPACK_HW_SIMD_PPC_VMX_VERSION
#endif

#if !defined(MSGPACK_HW_SIMD_PPC)
#   define MSGPACK_HW_SIMD_PPC MSGPACK_VERSION_NUMBER_NOT_AVAILABLE
#else
#   define MSGPACK_HW_SIMD_PPC_AVAILABLE
#endif

#define MSGPACK_HW_SIMD_PPC_NAME "PPC SIMD"

#endif

#include <msgpack/predef/detail/test.h>
MSGPACK_PREDEF_DECLARE_TEST(MSGPACK_HW_SIMD_PPC, MSGPACK_HW_SIMD_PPC_NAME)
