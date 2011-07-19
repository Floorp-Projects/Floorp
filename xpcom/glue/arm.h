/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is arm.h
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Timothy B. Terriberry <tterriberry@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* compile-time and runtime tests for whether to use SSE instructions */

#ifndef mozilla_arm_h_
#define mozilla_arm_h_

// for definition of NS_COM_GLUE
#include "nscore.h"

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


#  if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#    define MOZILLA_MAY_SUPPORT_EDSP 1
#  endif

#  if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#    if defined(HAVE_ARM_SIMD)
#      define MOZILLA_MAY_SUPPORT_ARMV6 1
#    endif
#  endif

  // Technically 4.2.x only works in the CodeSourcery releases, but I don't
  // know how to detect those separately from mainline gcc (which got support
  // in 4.3). The Maemo version 5 SDK shipped with the CodeSourcery 4.2.1
  // release, which we need to work.
#  if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)
#    if defined(HAVE_ARM_NEON)
#      define MOZILLA_MAY_SUPPORT_NEON 1
#    endif
#  endif

  // Currently we only have CPU detection for Linux via /proc/cpuinfo
#  if defined(__linux__) || defined(ANDROID)
#    define MOZILLA_ARM_HAVE_CPUID_DETECTION 1
#  endif

#elif defined(_MSC_VER) && defined(_M_ARM)

#  define MOZILLA_ARM_HAVE_CPUID_DETECTION 1
  // I don't know how to do arch detection at compile time for MSVC, so assume
  // the worst for now.
#  define MOZILLA_ARM_ARCH 3

  // MSVC only allows external asm for ARM, so we don't have to rely on
  // compiler support.
#  define MOZILLA_MAY_SUPPORT_EDSP 1
#  if defined(HAVE_ARM_SIMD)
#    define MOZILLA_MAY_SUPPORT_ARMV6 1
#  endif
#  if defined(HAVE_ARM_NEON)
#    define MOZILLA_MAY_SUPPORT_NEON 1
#  endif

#endif

namespace mozilla {

  namespace arm_private {
#if defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
#if !defined(MOZILLA_PRESUME_EDSP)
    extern bool NS_COM_GLUE edsp_enabled;
#endif
#if !defined(MOZILLA_PRESUME_ARMV6)
    extern bool NS_COM_GLUE armv6_enabled;
#endif
#if !defined(MOZILLA_PRESUME_NEON)
    extern bool NS_COM_GLUE neon_enabled;
#endif
#endif
  }

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

#if defined(MOZILLA_PRESUME_NEON)
#  define MOZILLA_MAY_SUPPORT_NEON 1
  inline bool supports_neon() { return true; }
#elif defined(MOZILLA_MAY_SUPPORT_NEON) \
   && defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
  inline bool supports_neon() { return arm_private::neon_enabled; }
#else
  inline bool supports_neon() { return false; }
#endif

}

#endif /* !defined(mozilla_arm_h_) */
