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
 * The Original Code is arm.cpp
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

/* compile-time and runtime tests for whether to use various ARM extensions */

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
  MOZILLA_HAS_NEON_FLAG=4
};

static unsigned
get_arm_cpu_flags(void)
{
  unsigned  flags;
  FILE     *fin;
  flags = 0;
  /*Reading /proc/self/auxv would be easier, but that doesn't work reliably on
    Android. This also means that detection will fail in Scratchbox, which is
    desirable, as NEON does not work in the qemu shipped with the Maemo 5 SDK.
    I don't know if /proc/self/auxv would do any better in that case, anyway,
    or if it would return random flags from the host CPU.*/
  fin = fopen ("/proc/cpuinfo","r");
  if (fin != NULL)
  {
    /*512 should be enough for anybody (it's even enough for all the flags that
      x86 has accumulated... so far).*/
    char buf[512];
    while (fgets(buf, 511, fin) != NULL)
    {
      if (memcmp(buf, "Features", 8) == 0)
      {
        char *p;
        p = strstr(buf, " edsp");
        if (p != NULL && (p[5] == ' ' || p[5] == '\n'))
          flags |= MOZILLA_HAS_EDSP_FLAG;
        p = strstr(buf, " neon");
        if( p != NULL && (p[5] == ' ' || p[5] == '\n'))
          flags |= MOZILLA_HAS_NEON_FLAG;
      }
      if (memcmp(buf, "CPU architecture:", 17) == 0)
      {
        int version;
        version = atoi(buf + 17);
        if (version >= 6)
          flags |= MOZILLA_HAS_ARMV6_FLAG;
      }
    }
    fclose(fin);
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

#    if !defined(MOZILLA_PRESUME_NEON)
static bool
check_neon(void)
{
  return (arm_cpu_flags & MOZILLA_HAS_NEON_FLAG) != 0;
#    endif
}

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
#  if !defined(MOZILLA_PRESUME_NEON)
    bool neon_enabled = check_neon();
#  endif
  } // namespace arm_private
} // namespace mozilla

#endif // MOZILLA_ARM_HAVE_CPUID_DETECTION
