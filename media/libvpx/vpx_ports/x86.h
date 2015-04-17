/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_PORTS_X86_H_
#define VPX_PORTS_X86_H_
#include <stdlib.h>
#include "vpx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VPX_CPU_UNKNOWN = -1,
  VPX_CPU_AMD,
  VPX_CPU_AMD_OLD,
  VPX_CPU_CENTAUR,
  VPX_CPU_CYRIX,
  VPX_CPU_INTEL,
  VPX_CPU_NEXGEN,
  VPX_CPU_NSC,
  VPX_CPU_RISE,
  VPX_CPU_SIS,
  VPX_CPU_TRANSMETA,
  VPX_CPU_TRANSMETA_OLD,
  VPX_CPU_UMC,
  VPX_CPU_VIA,

  VPX_CPU_LAST
}  vpx_cpu_t;

#if defined(__GNUC__) && __GNUC__ || defined(__ANDROID__)
#if ARCH_X86_64
#define cpuid(func, func2, ax, bx, cx, dx)\
  __asm__ __volatile__ (\
                        "cpuid           \n\t" \
                        : "=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) \
                        : "a" (func), "c" (func2));
#else
#define cpuid(func, func2, ax, bx, cx, dx)\
  __asm__ __volatile__ (\
                        "mov %%ebx, %%edi   \n\t" \
                        "cpuid              \n\t" \
                        "xchg %%edi, %%ebx  \n\t" \
                        : "=a" (ax), "=D" (bx), "=c" (cx), "=d" (dx) \
                        : "a" (func), "c" (func2));
#endif
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC) /* end __GNUC__ or __ANDROID__*/
#if ARCH_X86_64
#define cpuid(func, func2, ax, bx, cx, dx)\
  asm volatile (\
                "xchg %rsi, %rbx \n\t" \
                "cpuid           \n\t" \
                "movl %ebx, %edi \n\t" \
                "xchg %rsi, %rbx \n\t" \
                : "=a" (ax), "=D" (bx), "=c" (cx), "=d" (dx) \
                : "a" (func), "c" (func2));
#else
#define cpuid(func, func2, ax, bx, cx, dx)\
  asm volatile (\
                "pushl %ebx       \n\t" \
                "cpuid            \n\t" \
                "movl %ebx, %edi  \n\t" \
                "popl %ebx        \n\t" \
                : "=a" (ax), "=D" (bx), "=c" (cx), "=d" (dx) \
                : "a" (func), "c" (func2));
#endif
#else /* end __SUNPRO__ */
#if ARCH_X86_64
#if defined(_MSC_VER) && _MSC_VER > 1500
void __cpuidex(int CPUInfo[4], int info_type, int ecxvalue);
#pragma intrinsic(__cpuidex)
#define cpuid(func, func2, a, b, c, d) do {\
    int regs[4];\
    __cpuidex(regs, func, func2); \
    a = regs[0];  b = regs[1];  c = regs[2];  d = regs[3];\
  } while(0)
#else
void __cpuid(int CPUInfo[4], int info_type);
#pragma intrinsic(__cpuid)
#define cpuid(func, func2, a, b, c, d) do {\
    int regs[4];\
    __cpuid(regs, func); \
    a = regs[0];  b = regs[1];  c = regs[2];  d = regs[3];\
  } while (0)
#endif
#else
#define cpuid(func, func2, a, b, c, d)\
  __asm mov eax, func\
  __asm mov ecx, func2\
  __asm cpuid\
  __asm mov a, eax\
  __asm mov b, ebx\
  __asm mov c, ecx\
  __asm mov d, edx
#endif
#endif /* end others */

#define HAS_MMX     0x01
#define HAS_SSE     0x02
#define HAS_SSE2    0x04
#define HAS_SSE3    0x08
#define HAS_SSSE3   0x10
#define HAS_SSE4_1  0x20
#define HAS_AVX     0x40
#define HAS_AVX2    0x80
#ifndef BIT
#define BIT(n) (1<<n)
#endif

static int
x86_simd_caps(void) {
  unsigned int flags = 0;
  unsigned int mask = ~0;
  unsigned int reg_eax, reg_ebx, reg_ecx, reg_edx;
  char *env;
  (void)reg_ebx;

  /* See if the CPU capabilities are being overridden by the environment */
  env = getenv("VPX_SIMD_CAPS");

  if (env && *env)
    return (int)strtol(env, NULL, 0);

  env = getenv("VPX_SIMD_CAPS_MASK");

  if (env && *env)
    mask = strtol(env, NULL, 0);

  /* Ensure that the CPUID instruction supports extended features */
  cpuid(0, 0, reg_eax, reg_ebx, reg_ecx, reg_edx);

  if (reg_eax < 1)
    return 0;

  /* Get the standard feature flags */
  cpuid(1, 0, reg_eax, reg_ebx, reg_ecx, reg_edx);

  if (reg_edx & BIT(23)) flags |= HAS_MMX;

  if (reg_edx & BIT(25)) flags |= HAS_SSE; /* aka xmm */

  if (reg_edx & BIT(26)) flags |= HAS_SSE2; /* aka wmt */

  if (reg_ecx & BIT(0)) flags |= HAS_SSE3;

  if (reg_ecx & BIT(9)) flags |= HAS_SSSE3;

  if (reg_ecx & BIT(19)) flags |= HAS_SSE4_1;

  if (reg_ecx & BIT(28)) flags |= HAS_AVX;

  /* Get the leaf 7 feature flags. Needed to check for AVX2 support */
  reg_eax = 7;
  reg_ecx = 0;
  cpuid(7, 0, reg_eax, reg_ebx, reg_ecx, reg_edx);

  if (reg_ebx & BIT(5)) flags |= HAS_AVX2;

  return flags & mask;
}

#if ARCH_X86_64 && defined(_MSC_VER)
unsigned __int64 __rdtsc(void);
#pragma intrinsic(__rdtsc)
#endif
static unsigned int
x86_readtsc(void) {
#if defined(__GNUC__) && __GNUC__
  unsigned int tsc;
  __asm__ __volatile__("rdtsc\n\t":"=a"(tsc):);
  return tsc;
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
  unsigned int tsc;
  asm volatile("rdtsc\n\t":"=a"(tsc):);
  return tsc;
#else
#if ARCH_X86_64
  return (unsigned int)__rdtsc();
#else
  __asm  rdtsc;
#endif
#endif
}


#if defined(__GNUC__) && __GNUC__
#define x86_pause_hint()\
  __asm__ __volatile__ ("pause \n\t")
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define x86_pause_hint()\
  asm volatile ("pause \n\t")
#else
#if ARCH_X86_64
#define x86_pause_hint()\
  _mm_pause();
#else
#define x86_pause_hint()\
  __asm pause
#endif
#endif

#if defined(__GNUC__) && __GNUC__
static void
x87_set_control_word(unsigned short mode) {
  __asm__ __volatile__("fldcw %0" : : "m"(*&mode));
}
static unsigned short
x87_get_control_word(void) {
  unsigned short mode;
  __asm__ __volatile__("fstcw %0\n\t":"=m"(*&mode):);
    return mode;
}
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
static void
x87_set_control_word(unsigned short mode) {
  asm volatile("fldcw %0" : : "m"(*&mode));
}
static unsigned short
x87_get_control_word(void) {
  unsigned short mode;
  asm volatile("fstcw %0\n\t":"=m"(*&mode):);
  return mode;
}
#elif ARCH_X86_64
/* No fldcw intrinsics on Windows x64, punt to external asm */
extern void           vpx_winx64_fldcw(unsigned short mode);
extern unsigned short vpx_winx64_fstcw(void);
#define x87_set_control_word vpx_winx64_fldcw
#define x87_get_control_word vpx_winx64_fstcw
#else
static void
x87_set_control_word(unsigned short mode) {
  __asm { fldcw mode }
}
static unsigned short
x87_get_control_word(void) {
  unsigned short mode;
  __asm { fstcw mode }
  return mode;
}
#endif

static unsigned short
x87_set_double_precision(void) {
  unsigned short mode = x87_get_control_word();
  x87_set_control_word((mode&~0x300) | 0x200);
  return mode;
}


extern void vpx_reset_mmx_state(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_PORTS_X86_H_
