/*
Copyright Charly Chevalier 2015
Copyright Joel Falcou 2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef MSGPACK_PREDEF_HARDWARE_SIMD_PPC_VERSIONS_H
#define MSGPACK_PREDEF_HARDWARE_SIMD_PPC_VERSIONS_H

#include <msgpack/predef/version_number.h>

/*`
 Those defines represent Power PC SIMD extensions versions.

 [note You *MUST* compare them with the predef `MSGPACK_HW_SIMD_PPC`.]
 */

// ---------------------------------

/*`
 [heading `MSGPACK_HW_SIMD_PPC_VMX_VERSION`]

 The [@https://en.wikipedia.org/wiki/AltiVec#VMX128 VMX] powerpc extension
 version number.

 Version number is: *1.0.0*.
 */
#define MSGPACK_HW_SIMD_PPC_VMX_VERSION MSGPACK_VERSION_NUMBER(1, 0, 0)

/*`
 [heading `MSGPACK_HW_SIMD_PPC_VSX_VERSION`]

 The [@https://en.wikipedia.org/wiki/AltiVec#VSX VSX] powerpc extension version
 number.

 Version number is: *1.1.0*.
 */
#define MSGPACK_HW_SIMD_PPC_VSX_VERSION MSGPACK_VERSION_NUMBER(1, 1, 0)

/*`
 [heading `MSGPACK_HW_SIMD_PPC_QPX_VERSION`]

 The QPX powerpc extension version number.

 Version number is: *2.0.0*.
 */
#define MSGPACK_HW_SIMD_PPC_QPX_VERSION MSGPACK_VERSION_NUMBER(2, 0, 0)

#endif
