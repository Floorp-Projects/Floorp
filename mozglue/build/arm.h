/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* compile-time and runtime tests for whether to use SSE instructions */

#ifndef mozilla_arm_h_
#define mozilla_arm_h_

// for definition of MFBT_DATA
#include "mozilla/Types.h"

/* This is patterned after SSE.h, but provides ARMv5E, ARMv6, and NEON
   detection. For reasons similar to the SSE code, code using NEON (even just
   in inline asm) needs to be in a separate compilation unit from the regular
   code, because it requires an ".fpu neon" directive which can't be undone.
   ARMv5E and ARMv6 code may also require an .arch directive, since by default
   the assembler refuses to generate code for opcodes outside of its current
   .arch setting.

   TODO: Add Thumb, Thumb2, VFP, iwMMX, etc. detection, if we need it. */

#if defined(__GNUC__) && defined(__arm__)

#  define MOZILLA_ARM_ARCH 3

#  if defined(__ARM_ARCH_4__) || defined(__ARM_ARCH_4T__) \
   || defined(_ARM_ARCH_4)
#    undef MOZILLA_ARM_ARCH
#    define MOZILLA_ARM_ARCH 4
#  endif

#  if defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5T__) \
   || defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) \
   || defined(__ARM_ARCH_5TEJ__) || defined(_ARM_ARCH_5)
#    undef MOZILLA_ARM_ARCH
#    define MOZILLA_ARM_ARCH 5
#  endif

#  if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
   || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
   || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) \
   || defined(__ARM_ARCH_6M__) || defined(_ARM_ARCH_6)
#    undef MOZILLA_ARM_ARCH
#    define MOZILLA_ARM_ARCH 6
#  endif

#  if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
   || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) \
   || defined(__ARM_ARCH_7EM__) || defined(_ARM_ARCH_7)
#    undef MOZILLA_ARM_ARCH
#    define MOZILLA_ARM_ARCH 7
#  endif


#  ifdef __GNUC__
#    define MOZILLA_MAY_SUPPORT_EDSP 1

#    if defined(HAVE_ARM_SIMD)
#      define MOZILLA_MAY_SUPPORT_ARMV6 1
#    endif

#    if defined(HAVE_ARM_NEON)
#      define MOZILLA_MAY_SUPPORT_NEON 1
#    endif

#    if defined(HAVE_ARM_SIMD)
#      define MOZILLA_MAY_SUPPORT_ARMV7 1
#    endif
#  endif

  // When using -mfpu=neon, gcc generates neon instructions.

#  if defined(__ARM_NEON__)
#    define MOZILLA_PRESUME_NEON 1
#  endif

  // Currently we only have CPU detection for Linux via /proc/cpuinfo
#  if defined(__linux__) || defined(ANDROID)
#    define MOZILLA_ARM_HAVE_CPUID_DETECTION 1
#  endif

#elif defined(_MSC_VER) && defined(_M_ARM)

#  define MOZILLA_ARM_HAVE_CPUID_DETECTION 1
  // _M_ARM on MSVC has current cpu architecture.
#  define MOZILLA_ARM_ARCH _M_ARM

  // MSVC only allows external asm for ARM, so we don't have to rely on
  // compiler support.
#  define MOZILLA_MAY_SUPPORT_EDSP 1
#  if defined(HAVE_ARM_SIMD)
#    define MOZILLA_MAY_SUPPORT_ARMV6 1
#    define MOZILLA_MAY_SUPPORT_ARMV7 1
#  endif
#  if defined(HAVE_ARM_NEON)
#    define MOZILLA_MAY_SUPPORT_NEON 1
#  endif

#endif

namespace mozilla {

  namespace arm_private {
#if defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
#if !defined(MOZILLA_PRESUME_EDSP)
    extern bool MFBT_DATA edsp_enabled;
#endif
#if !defined(MOZILLA_PRESUME_ARMV6)
    extern bool MFBT_DATA armv6_enabled;
#endif
#if !defined(MOZILLA_PRESUME_ARMV7)
    extern bool MFBT_DATA armv7_enabled;
#endif
#if !defined(MOZILLA_PRESUME_NEON)
    extern bool MFBT_DATA neon_enabled;
#endif
#endif
  } // namespace arm_private

#if defined(MOZILLA_PRESUME_EDSP)
#  define MOZILLA_MAY_SUPPORT_EDSP 1
  inline bool supports_edsp() { return true; }
#elif defined(MOZILLA_MAY_SUPPORT_EDSP) \
   && defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
  inline bool supports_edsp() { return arm_private::edsp_enabled; }
#else
  inline bool supports_edsp() { return false; }
#endif

#if defined(MOZILLA_PRESUME_ARMV6)
#  define MOZILLA_MAY_SUPPORT_ARMV6 1
  inline bool supports_armv6() { return true; }
#elif defined(MOZILLA_MAY_SUPPORT_ARMV6) \
   && defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
  inline bool supports_armv6() { return arm_private::armv6_enabled; }
#else
  inline bool supports_armv6() { return false; }
#endif

#if defined(MOZILLA_PRESUME_ARMV7)
#  define MOZILLA_MAY_SUPPORT_ARMV7 1
  inline bool supports_armv7() { return true; }
#elif defined(MOZILLA_MAY_SUPPORT_ARMV7) \
   && defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
  inline bool supports_armv7() { return arm_private::armv7_enabled; }
#else
  inline bool supports_armv7() { return false; }
#endif

#if defined(MOZILLA_PRESUME_NEON)
#  define MOZILLA_MAY_SUPPORT_NEON 1
  inline bool supports_neon() { return true; }
#elif defined(MOZILLA_MAY_SUPPORT_NEON) \
   && defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
  inline bool supports_neon() { return arm_private::neon_enabled; }
#else
  inline bool supports_neon() { return false; }
#endif

} // namespace mozilla

#endif /* !defined(mozilla_arm_h_) */
