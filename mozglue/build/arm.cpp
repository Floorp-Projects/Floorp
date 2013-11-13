/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* compile-time and runtime tests for whether to use various ARM extensions */

#include "mozilla/NullPtr.h"

#include "arm.h"

#if defined(MOZILLA_ARM_HAVE_CPUID_DETECTION)
namespace {

// arm.h has parallel #ifs which declare MOZILLA_ARM_HAVE_CPUID_DETECTION.
// We don't check it here so that we get compile errors if it's defined, but
// we don't compile one of these detection methods. The detection code here is
// based on the CPU detection in libtheora.

#  if defined(_MSC_VER)
//For GetExceptionCode() and EXCEPTION_ILLEGAL_INSTRUCTION.
#    define WIN32_LEAN_AND_MEAN
#    define WIN32_EXTRA_LEAN
#    include <windows.h>

#    if !defined(MOZILLA_PRESUME_EDSP)
static bool
check_edsp(void)
{
#      if defined(MOZILLA_MAY_SUPPORT_EDSP)
  __try
  {
    //PLD [r13]
    __emit(0xF5DDF000);
    return true;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION)
  {
    //Ignore exception.
  }
#      endif
  return false;
}
#    endif // !MOZILLA_PRESUME_EDSP

#    if !defined(MOZILLA_PRESUME_ARMV6)
static bool
check_armv6(void)
{
#      if defined(MOZILLA_MAY_SUPPORT_ARMV6)
  __try
  {
    //SHADD8 r3,r3,r3
    __emit(0xE6333F93);
    return true;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION)
  {
    //Ignore exception.
  }
#      endif
  return false;
}
#    endif // !MOZILLA_PRESUME_ARMV6

#    if !defined(MOZILLA_PRESUME_ARMV7)
static bool
check_armv7(void)
{
#      if defined(MOZILLA_MAY_SUPPORT_ARMV7)
  __try
  {
    // ARMv7 DMB (Data Memory Barrier) for stores (DMB.ST)
    // The Data Memory Barrier existed before ARMv7 as a
    // cp15 operation, but ARMv7 introduced a dedicated
    // instruction, DMB.
    emit(0xF57FF05E);
    return true;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION)
  {
    //Ignore exception.
  }
#      endif
  return false;
}
#    endif // !MOZILLA_PRESUME_ARMV7

#    if !defined(MOZILLA_PRESUME_NEON)
static bool
check_neon(void)
{
#      if defined(MOZILLA_MAY_SUPPORT_NEON)
  __try
  {
    //VORR q0,q0,q0
    __emit(0xF2200150);
    return true;
  }
  __except(GetExceptionCode()==EXCEPTION_ILLEGAL_INSTRUCTION)
  {
    //Ignore exception.
  }
#      endif
  return false;
}
#    endif // !MOZILLA_PRESUME_NEON

#  elif defined(__linux__) || defined(ANDROID)
#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>

enum{
  MOZILLA_HAS_EDSP_FLAG=1,
  MOZILLA_HAS_ARMV6_FLAG=2,
  MOZILLA_HAS_ARMV7_FLAG=4,
  MOZILLA_HAS_NEON_FLAG=8
};

static unsigned
get_arm_cpu_flags(void)
{
  unsigned  flags;
  FILE     *fin;
  bool      armv6_processor = false;
  flags = 0;
  /*Reading /proc/self/auxv would be easier, but that doesn't work reliably on
    Android. This also means that detection will fail in Scratchbox, which is
    desirable, as NEON does not work in the qemu shipped with the Maemo 5 SDK.
    I don't know if /proc/self/auxv would do any better in that case, anyway,
    or if it would return random flags from the host CPU.*/
  fin = fopen ("/proc/cpuinfo","r");
  if (fin != nullptr)
  {
    /*512 should be enough for anybody (it's even enough for all the flags that
      x86 has accumulated... so far).*/
    char buf[512];
    while (fgets(buf, 511, fin) != nullptr)
    {
      if (memcmp(buf, "Features", 8) == 0)
      {
        char *p;
        p = strstr(buf, " edsp");
        if (p != nullptr && (p[5] == ' ' || p[5] == '\n'))
          flags |= MOZILLA_HAS_EDSP_FLAG;
        p = strstr(buf, " neon");
        if( p != nullptr && (p[5] == ' ' || p[5] == '\n'))
          flags |= MOZILLA_HAS_NEON_FLAG;
      }
      if (memcmp(buf, "CPU architecture:", 17) == 0)
      {
        int version;
        version = atoi(buf + 17);
        if (version >= 6)
          flags |= MOZILLA_HAS_ARMV6_FLAG;
        if (version >= 7)
          flags |= MOZILLA_HAS_ARMV7_FLAG;
      }
      /* media/webrtc/trunk/src/system_wrappers/source/cpu_features_arm.c
       * Unfortunately, it seems that certain ARMv6-based CPUs
       * report an incorrect architecture number of 7!
       *
       * We try to correct this by looking at the 'elf_format'
       * field reported by the 'Processor' field, which is of the
       * form of "(v7l)" for an ARMv7-based CPU, and "(v6l)" for
       * an ARMv6-one.
       */
      if (memcmp(buf, "Processor\t:", 11) == 0) {
          if (strstr(buf, "(v6l)") != 0) {
              armv6_processor = true;
          }
      }
    }
    fclose(fin);
  }
  if (armv6_processor) {
      // ARMv6 pretending to be ARMv7? clear flag
      if (flags & MOZILLA_HAS_ARMV7_FLAG) {
          flags &= ~MOZILLA_HAS_ARMV7_FLAG;
      }
  }
  return flags;
}

// Cache a local copy so we only have to read /proc/cpuinfo once.
static unsigned arm_cpu_flags = get_arm_cpu_flags();

#    if !defined(MOZILLA_PRESUME_EDSP)
static bool
check_edsp(void)
{
  return (arm_cpu_flags & MOZILLA_HAS_EDSP_FLAG) != 0;
}
#    endif

#    if !defined(MOZILLA_PRESUME_ARMV6)
static bool
check_armv6(void)
{
  return (arm_cpu_flags & MOZILLA_HAS_ARMV6_FLAG) != 0;
}
#    endif

#    if !defined(MOZILLA_PRESUME_ARMV7)
static bool
check_armv7(void)
{
  return (arm_cpu_flags & MOZILLA_HAS_ARMV7_FLAG) != 0;
}
#    endif

#    if !defined(MOZILLA_PRESUME_NEON)
static bool
check_neon(void)
{
  return (arm_cpu_flags & MOZILLA_HAS_NEON_FLAG) != 0;
}
#    endif

#  endif // defined(__linux__) || defined(ANDROID)

}

namespace mozilla {
  namespace arm_private {
#  if !defined(MOZILLA_PRESUME_EDSP)
    bool edsp_enabled = check_edsp();
#  endif
#  if !defined(MOZILLA_PRESUME_ARMV6)
    bool armv6_enabled = check_armv6();
#  endif
#  if !defined(MOZILLA_PRESUME_ARMV7)
    bool armv7_enabled = check_armv7();
#  endif
#  if !defined(MOZILLA_PRESUME_NEON)
    bool neon_enabled = check_neon();
#  endif
  } // namespace arm_private
} // namespace mozilla

#endif // MOZILLA_ARM_HAVE_CPUID_DETECTION
