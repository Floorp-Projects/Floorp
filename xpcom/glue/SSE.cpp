/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is SSE.cpp
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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

#include "SSE.h"

namespace {

// SSE.h has parallel #ifs which declare MOZILLA_SSE_HAVE_CPUID_DETECTION.
// We can't declare these functions in the header file, however, because
// <intrin.h> conflicts with <windows.h> on MSVC 2005, and some files want to
// include both SSE.h and <windows.h>.

#ifdef HAVE_CPUID_H

// cpuid.h is available on gcc 4.3 and higher on i386 and x86_64
#include <cpuid.h>

enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

static bool
has_cpuid_bit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  unsigned int regs[4];
  return __get_cpuid(level, &regs[0], &regs[1], &regs[2], &regs[3]) &&
         (regs[reg] & bit);
}

#elif defined(_MSC_VER) && _MSC_VER >= 1400 && (defined(_M_IX86) || defined(_M_AMD64))

// MSVC 2005 or newer on x86-32 or x86-64
#include <intrin.h>

enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

static bool
has_cpuid_bit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  // Check that the level in question is supported.
  int regs[4];
  __cpuid(regs, level & 0x80000000u);
  if (unsigned(regs[0]) < level)
    return false;

  __cpuid(regs, level);
  return !!(unsigned(regs[reg]) & bit);
}

#elif defined(__SUNPRO_CC) && (defined(__i386) || defined(__x86_64__))

enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

#ifdef __i386
static void
moz_cpuid(int CPUInfo[4], int InfoType)
{
  asm (
    "xchg %esi, %ebx\n"
    "cpuid\n"
    "movl %eax, (%edi)\n"
    "movl %ebx, 4(%edi)\n"
    "movl %ecx, 8(%edi)\n"
    "movl %edx, 12(%edi)\n"
    "xchg %esi, %ebx\n"
    :
    : "a"(InfoType), // %eax
      "D"(CPUInfo) // %edi
    : "%ecx", "%edx", "%esi"
  );
}
#else
static void
moz_cpuid(int CPUInfo[4], int InfoType)
{
  asm (
    "xchg %rsi, %rbx\n"
    "cpuid\n"
    "movl %eax, (%rdi)\n"
    "movl %ebx, 4(%rdi)\n"
    "movl %ecx, 8(%rdi)\n"
    "movl %edx, 12(%rdi)\n"
    "xchg %rsi, %rbx\n"
    :
    : "a"(InfoType), // %eax
      "D"(CPUInfo) // %rdi
    : "%ecx", "%edx", "%rsi"
  );
}
#endif

static bool
has_cpuid_bit(unsigned int level, CPUIDRegister reg, unsigned int bit)
{
  // Check that the level in question is supported.
  volatile int regs[4];
  moz_cpuid((int *)regs, level & 0x80000000u);
  if (unsigned(regs[0]) < level)
    return false;

  moz_cpuid((int *)regs, level);
  return !!(unsigned(regs[reg]) & bit);
}

#endif // end CPUID declarations

}

namespace mozilla {

namespace sse_private {

#if defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)

#if !defined(MOZILLA_PRESUME_MMX)
  bool mmx_enabled = has_cpuid_bit(1u, edx, (1u<<23));
#endif

#if !defined(MOZILLA_PRESUME_SSE)
  bool sse_enabled = has_cpuid_bit(1u, edx, (1u<<25));
#endif

#if !defined(MOZILLA_PRESUME_SSE2)
  bool sse2_enabled = has_cpuid_bit(1u, edx, (1u<<26));
#endif

#if !defined(MOZILLA_PRESUME_SSE3)
  bool sse3_enabled = has_cpuid_bit(1u, ecx, (1u<<0));
#endif

#if !defined(MOZILLA_PRESUME_SSSE3)
  bool ssse3_enabled = has_cpuid_bit(1u, ecx, (1u<<9));
#endif

#if !defined(MOZILLA_PRESUME_SSE4A)
  bool sse4a_enabled = has_cpuid_bit(0x80000001u, ecx, (1u<<6));
#endif

#if !defined(MOZILLA_PRESUME_SSE4_1)
  bool sse4_1_enabled = has_cpuid_bit(1u, ecx, (1u<<19));
#endif

#if !defined(MOZILLA_PRESUME_SSE4_2)
  bool sse4_2_enabled = has_cpuid_bit(1u, ecx, (1u<<20));
#endif

#endif

} // namespace sse_private
} // namespace mozilla
