/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is SSE.h
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

#ifndef mozilla_SSE_h_
#define mozilla_SSE_h_

// for definition of NS_COM_GLUE
#include "nscore.h"

/**
 * The public interface of this header consists of a set of macros and
 * functions for Intel CPU features.
 *
 * CODE USING ASSEMBLY
 * ===================
 *
 * For each feature handled here, this header defines a single function
 * that can be used to test whether to use code written in *assembly*:
 *    mozilla::supports_mmx
 *    mozilla::supports_sse
 *    mozilla::supports_sse2
 *    mozilla::supports_sse3
 *    mozilla::supports_ssse3
 *    mozilla::supports_sse4a
 *    mozilla::supports_sse4_1
 *    mozilla::supports_sse4_2
 * Note that the dynamic detection depends on cpuid intrinsics only
 * available in gcc 4.3 or later and MSVC 8.0 (Visual C++ 2005) or
 * later, so the dynamic detection returns false in older compilers.
 * (This could be fixed by replacing the code with inline assembly.)
 *
 * CODE USING INTRINSICS
 * =====================
 *
 * The remainder of the functions and macros that are the API from this
 * header relate to code written using CPU intrinsics.
 *
 * In each macro-function pair, the function may not be available if the
 * macro is undefined.  They should be used in the following pattern:
 *
 *     if (mozilla::use_abc()) {
 *   #ifdef MOZILLA_COMPILE_WITH_ABC
 *       // abc-specific code here
 *   #endif
 *     } else {
 *      // generic code here
 *     }
 *
 * In addition, on some platforms, the headers that contain the
 * intrinsics for many of these features won't compile unless we define
 * a particular macro first (to pretend that we gave gcc an appropriate
 * -march option).  Therefore, code using this header should NOT include
 * the headers for intrinsics directly, but should instead request the
 * header by defining the header macro given below *before* including
 * this file (which, in practice, means before including *any* header
 * files).
 *
 * The dynamic detection depends on cpuid intrinsics only available in
 * gcc 4.3 or later and MSVC 8.0 (Visual C++ 2005) or later, so the
 * dynamic detection returns false in older compilers.  However, it
 * could be extended to avoid this restriction; see the code in
 * mozilla/jpeg/jdapimin.c for some hints.
 *
 * Macro: MOZILLA_COMPILE_WITH_MMX
 * Function: mozilla::use_mmx
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_MMX
 * Header: <mmintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSE
 * Function: mozilla::use_sse
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE
 * Header: <xmmintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSE2
 * Function: mozilla::use_sse2
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE2
 * Header: <emmintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSE3
 * Function: mozilla::use_sse3
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE3
 * Header: <pmmintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSSE3
 * Function: mozilla::use_ssse3
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSSE3
 * Header: <tmmintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSE4A
 * Function: mozilla::use_sse4a
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE4A
 * Header: <ammintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSE4_1
 * Function: mozilla::use_sse4_1
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE4_1
 * Header: <smmintrin.h>
 *
 * Macro: MOZILLA_COMPILE_WITH_SSE4_2
 * Function: mozilla::use_sse4_2
 * Header Macro: MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE4_2
 * Header: <nmmintrin.h>
 */

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
// GCC

// FIXME: Is any of this available on arm?  GCC seems to offer mmintrin.h

#if 0
  // #pragma target doesn't actually work yet; making it work depends on
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39787 and
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=41201 being fixed.
  // Note that this means there are cases where mozilla::use_abc() will
  // return true even though we can't define MOZILLA_COMPILE_WITH_ABC.
  #define MOZILLA_SSE_HAVE_PRAGMA_TARGET
#endif

#ifdef MOZILLA_SSE_HAVE_PRAGMA_TARGET
  // Limit compilation to compiler versions where the *intrin.h headers
  // are present.  (These ifdefs actually aren't useful because they're
  // all (currently) weaker than MOZILLA_SSE_HAVE_PRAGMA_TARGET.)
  #if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1) // GCC 3.1 and up
    #define MOZILLA_COMPILE_WITH_MMX 1
    #define MOZILLA_COMPILE_WITH_SSE 1
  #endif // GCC 3.1 and up
  #if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) // GCC 3.3 and up
    #define MOZILLA_COMPILE_WITH_SSE2 1
    #define MOZILLA_COMPILE_WITH_SSE3 1
  #endif // GCC 3.3 and up
  #if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) // GCC 4.3 and up
    #define MOZILLA_COMPILE_WITH_SSSE3 1
    #define MOZILLA_COMPILE_WITH_SSE4A 1
    #define MOZILLA_COMPILE_WITH_SSE4_1 1
    #define MOZILLA_COMPILE_WITH_SSE4_2 1
  #endif // GCC 4.3 and up
  // GCC 4.3 and GCC 4.4 shipped with SSE5, but it is being removed in GCC
  // 4.5; see http://gcc.gnu.org/ml/gcc-patches/2009-07/msg01527.html and
  // http://gcc.gnu.org/ml/gcc-patches/2009-07/msg01536.html
#else
  #ifdef __MMX__
    #define MOZILLA_COMPILE_WITH_MMX 1
  #endif
  #ifdef __SSE__
    #define MOZILLA_COMPILE_WITH_SSE 1
  #endif
  #ifdef __SSE2__
    #define MOZILLA_COMPILE_WITH_SSE2 1
  #endif
  #ifdef __SSE3__
    #define MOZILLA_COMPILE_WITH_SSE3 1
  #endif
  #ifdef __SSSE3__
    #define MOZILLA_COMPILE_WITH_SSSE3 1
  #endif
  #ifdef __SSE4A__
    #define MOZILLA_COMPILE_WITH_SSE4A 1
  #endif
  #ifdef __SSE4_1__
    #define MOZILLA_COMPILE_WITH_SSE4_1 1
  #endif
  #ifdef __SSE4_2__
    #define MOZILLA_COMPILE_WITH_SSE4_2 1
  #endif
#endif

#ifdef __MMX__
  // It's ok to use MMX instructions based on the -march option (or
  // the default for x86_64 or for Intel Mac).
  #define MOZILLA_PRESUME_MMX 1
#endif
#ifdef __SSE__
  // It's ok to use SSE instructions based on the -march option (or
  // the default for x86_64 or for Intel Mac).
  #define MOZILLA_PRESUME_SSE 1
#endif
#ifdef __SSE2__
  // It's ok to use SSE2 instructions based on the -march option (or
  // the default for x86_64 or for Intel Mac).
  #define MOZILLA_PRESUME_SSE2 1
#endif
#ifdef __SSE3__
  // It's ok to use SSE3 instructions based on the -march option (or the
  // default for Intel Mac).
  #define MOZILLA_PRESUME_SSE3 1
#endif
#ifdef __SSSE3__
  // It's ok to use SSSE3 instructions based on the -march option.
  #define MOZILLA_PRESUME_SSSE3 1
#endif
#ifdef __SSE4A__
  // It's ok to use SSE4A instructions based on the -march option.
  #define MOZILLA_PRESUME_SSE4A 1
#endif
#ifdef __SSE4_1__
  // It's ok to use SSE4.1 instructions based on the -march option.
  #define MOZILLA_PRESUME_SSE4_1 1
#endif
#ifdef __SSE4_2__
  // It's ok to use SSE4.2 instructions based on the -march option.
  #define MOZILLA_PRESUME_SSE4_2 1
#endif

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)

// cpuid.h is available on gcc 4.3 and higher on i386 and x86_64
#include <cpuid.h>
#define MOZILLA_SSE_HAVE_CPUID_DETECTION

namespace mozilla {

  namespace sse_private {

    enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

    inline bool
    has_cpuid_bit(unsigned int level, CPUIDRegister reg, unsigned int bit)
    {
      unsigned int regs[4];
      return __get_cpuid(level, &regs[0], &regs[1], &regs[2], &regs[3]) &&
             (regs[reg] & bit);
    }

  }

}

#endif

// We need to #include headers quite carefully for CPUID-tested
// compilation.  GCC's headers for SSE intrinsics both:
//  * have #error in them when the appropriate macro is not defined, and
//  * depend on intrinsics that depend on -msse, etc.
// We could fix the first of these options with #define.  However, we
// can fix the second only with #pragma directives introduced in GCC
// 4.4 (but not yet working quite well enough on any gcc version), whose
// availability influenced (above) whether MOZILLA_COMPILE_WITH_* is
// defined.

#ifdef MOZILLA_SSE_HAVE_PRAGMA_TARGET
#pragma GCC push_options
#endif

#if defined(MOZILLA_COMPILE_WITH_MMX) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_MMX)
  #if !defined(__MMX__)
    #pragma GCC target ("mmx")
  #endif
  #include <mmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE)
  #if !defined(__SSE__)
    #pragma GCC target ("sse")
  #endif
  #include <xmmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE2) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE2)
  #if !defined(__SSE2__)
    #pragma GCC target ("sse2")
  #endif
  #include <emmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE3) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE3)
  #if !defined(__SSE3__)
    #pragma GCC target ("sse3")
  #endif
  #include <pmmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSSE3) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSSE3)
  #if !defined(__SSSE3__)
    #pragma GCC target ("ssse3")
  #endif
  #include <tmmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE4A) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE4A)
  #if !defined(__SSE4A__)
    #pragma GCC target ("sse4a")
  #endif
  #include <ammintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE4_1) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE4_1)
  #if !defined(__SSE4_1__)
    #pragma GCC target ("sse4.1")
  #endif
  #include <smmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE4_2) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE4_2)
  #if !defined(__SSE4_2__)
    #pragma GCC target ("sse4.2")
  #endif
  #include <nmmintrin.h>
#endif

#ifdef MOZILLA_SSE_HAVE_PRAGMA_TARGET
#pragma GCC pop_options
#endif

#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_AMD64))
// MSVC on x86 or amd64

// Limit compilation to compiler versions where the *intrin.h headers
// are present
#if 1 // Available at least back to Visual Studio 2003
#define MOZILLA_COMPILE_WITH_MMX 1
#define MOZILLA_COMPILE_WITH_SSE 1
#define MOZILLA_COMPILE_WITH_SSE2 1
#endif // Available at least back to Visual Studio 2003

#if _MSC_VER >= 1400
#include <intrin.h>
#define MOZILLA_SSE_HAVE_CPUID_DETECTION

namespace mozilla {

  namespace sse_private {

    enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

    inline bool
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

  }

}

#endif

#if defined(_M_AMD64)
  // MMX is always available on AMD64.
  #define MOZILLA_PRESUME_MMX
  // SSE is always available on AMD64.
  #define MOZILLA_PRESUME_SSE
  // SSE2 is always available on AMD64.
  #define MOZILLA_PRESUME_SSE2
#endif

#if defined(MOZILLA_COMPILE_WITH_MMX) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_MMX)
#include <mmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE)
#include <xmmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE2) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE2)
#include <emmintrin.h>
#endif

#elif defined(__SUNPRO_CC) && (defined(__i386) || defined(__x86_64__))
// Sun Studio on x86 or amd64

#define MOZILLA_COMPILE_WITH_MMX 1
#define MOZILLA_COMPILE_WITH_SSE 1
#define MOZILLA_COMPILE_WITH_SSE2 1

#define MOZILLA_SSE_HAVE_CPUID_DETECTION

namespace mozilla {

  namespace sse_private {

    enum CPUIDRegister { eax = 0, ebx = 1, ecx = 2, edx = 3 };

#ifdef __i386
    inline void
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
    inline void
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

    inline bool
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

  }

}

#if defined(__x86_64__)
  // MMX is always available on AMD64.
  #define MOZILLA_PRESUME_MMX
  // SSE is always available on AMD64.
  #define MOZILLA_PRESUME_SSE
  // SSE2 is always available on AMD64.
  #define MOZILLA_PRESUME_SSE2
#endif

#if defined(MOZILLA_COMPILE_WITH_MMX) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_MMX)
#include <mmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE)
#include <xmmintrin.h>
#endif

#if defined(MOZILLA_COMPILE_WITH_SSE2) && \
    defined(MOZILLA_SSE_INCLUDE_HEADER_FOR_SSE2)
#include <emmintrin.h>
#endif

#endif

namespace mozilla {

  namespace sse_private {
#if defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
#if !defined(MOZILLA_PRESUME_MMX)
    extern bool NS_COM_GLUE mmx_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSE)
    extern bool NS_COM_GLUE sse_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSE2)
    extern bool NS_COM_GLUE sse2_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSE3)
    extern bool NS_COM_GLUE sse3_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSSE3)
    extern bool NS_COM_GLUE ssse3_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSE4A)
    extern bool NS_COM_GLUE sse4a_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSE4_1)
    extern bool NS_COM_GLUE sse4_1_enabled;
#endif
#if !defined(MOZILLA_PRESUME_SSE4_2)
    extern bool NS_COM_GLUE sse4_2_enabled;
#endif
#endif
  }

#if defined(MOZILLA_PRESUME_MMX)
  inline bool supports_mmx() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_mmx() { return sse_private::mmx_enabled; }
#else
  inline bool supports_mmx() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSE)
  inline bool supports_sse() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_sse() { return sse_private::sse_enabled; }
#else
  inline bool supports_sse() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSE2)
  inline bool supports_sse2() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_sse2() { return sse_private::sse2_enabled; }
#else
  inline bool supports_sse2() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSE3)
  inline bool supports_sse3() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_sse3() { return sse_private::sse3_enabled; }
#else
  inline bool supports_sse3() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSSE3)
  inline bool supports_ssse3() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_ssse3() { return sse_private::ssse3_enabled; }
#else
  inline bool supports_ssse3() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSE4A)
  inline bool supports_sse4a() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_sse4a() { return sse_private::sse4a_enabled; }
#else
  inline bool supports_sse4a() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSE4_1)
  inline bool supports_sse4_1() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_sse4_1() { return sse_private::sse4_1_enabled; }
#else
  inline bool supports_sse4_1() { return false; }
#endif

#if defined(MOZILLA_PRESUME_SSE4_2)
  inline bool supports_sse4_2() { return true; }
#elif defined(MOZILLA_SSE_HAVE_CPUID_DETECTION)
  inline bool supports_sse4_2() { return sse_private::sse4_2_enabled; }
#else
  inline bool supports_sse4_2() { return false; }
#endif



#ifdef MOZILLA_COMPILE_WITH_MMX
  inline bool use_mmx() { return supports_mmx(); }
#else
  inline bool use_mmx() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE
  inline bool use_sse() { return supports_sse(); }
#else
  inline bool use_sse() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE2
  inline bool use_sse2() { return supports_sse2(); }
#else
  inline bool use_sse2() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE3
  inline bool use_sse3() { return supports_sse3(); }
#else
  inline bool use_sse3() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSSE3
  inline bool use_ssse3() { return supports_ssse3(); }
#else
  inline bool use_ssse3() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE4a
  inline bool use_sse4a() { return supports_sse4a(); }
#else
  inline bool use_sse4a() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE4_1
  inline bool use_sse4_1() { return supports_sse4_1(); }
#else
  inline bool use_sse4_1() { return false; }
#endif

#ifdef MOZILLA_COMPILE_WITH_SSE4_2
  inline bool use_sse4_2() { return supports_sse4_2(); }
#else
  inline bool use_sse4_2() { return false; }
#endif

}

#endif /* !defined(mozilla_SSE_h_) */
