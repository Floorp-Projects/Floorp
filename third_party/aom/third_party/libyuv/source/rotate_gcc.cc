/*
 *  Copyright 2015 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"
#include "libyuv/rotate_row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// This module is for GCC x86 and x64.
#if !defined(LIBYUV_DISABLE_X86) && (defined(__x86_64__) || defined(__i386__))

#if !defined(LIBYUV_DISABLE_X86) && \
    (defined(__i386__) || (defined(__x86_64__) && !defined(__native_client__)))
void TransposeWx8_SSSE3(const uint8* src, int src_stride,
                        uint8* dst, int dst_stride, int width) {
  asm volatile (
    // Read in the data from the source pointer.
    // First round of bit swap.
    ".p2align  2                                 \n"
  "1:                                            \n"
    "movq       (%0),%%xmm0                      \n"
    "movq       (%0,%3),%%xmm1                   \n"
    "lea        (%0,%3,2),%0                     \n"
    "punpcklbw  %%xmm1,%%xmm0                    \n"
    "movq       (%0),%%xmm2                      \n"
    "movdqa     %%xmm0,%%xmm1                    \n"
    "palignr    $0x8,%%xmm1,%%xmm1               \n"
    "movq       (%0,%3),%%xmm3                   \n"
    "lea        (%0,%3,2),%0                     \n"
    "punpcklbw  %%xmm3,%%xmm2                    \n"
    "movdqa     %%xmm2,%%xmm3                    \n"
    "movq       (%0),%%xmm4                      \n"
    "palignr    $0x8,%%xmm3,%%xmm3               \n"
    "movq       (%0,%3),%%xmm5                   \n"
    "lea        (%0,%3,2),%0                     \n"
    "punpcklbw  %%xmm5,%%xmm4                    \n"
    "movdqa     %%xmm4,%%xmm5                    \n"
    "movq       (%0),%%xmm6                      \n"
    "palignr    $0x8,%%xmm5,%%xmm5               \n"
    "movq       (%0,%3),%%xmm7                   \n"
    "lea        (%0,%3,2),%0                     \n"
    "punpcklbw  %%xmm7,%%xmm6                    \n"
    "neg        %3                               \n"
    "movdqa     %%xmm6,%%xmm7                    \n"
    "lea        0x8(%0,%3,8),%0                  \n"
    "palignr    $0x8,%%xmm7,%%xmm7               \n"
    "neg        %3                               \n"
     // Second round of bit swap.
    "punpcklwd  %%xmm2,%%xmm0                    \n"
    "punpcklwd  %%xmm3,%%xmm1                    \n"
    "movdqa     %%xmm0,%%xmm2                    \n"
    "movdqa     %%xmm1,%%xmm3                    \n"
    "palignr    $0x8,%%xmm2,%%xmm2               \n"
    "palignr    $0x8,%%xmm3,%%xmm3               \n"
    "punpcklwd  %%xmm6,%%xmm4                    \n"
    "punpcklwd  %%xmm7,%%xmm5                    \n"
    "movdqa     %%xmm4,%%xmm6                    \n"
    "movdqa     %%xmm5,%%xmm7                    \n"
    "palignr    $0x8,%%xmm6,%%xmm6               \n"
    "palignr    $0x8,%%xmm7,%%xmm7               \n"
    // Third round of bit swap.
    // Write to the destination pointer.
    "punpckldq  %%xmm4,%%xmm0                    \n"
    "movq       %%xmm0,(%1)                      \n"
    "movdqa     %%xmm0,%%xmm4                    \n"
    "palignr    $0x8,%%xmm4,%%xmm4               \n"
    "movq       %%xmm4,(%1,%4)                   \n"
    "lea        (%1,%4,2),%1                     \n"
    "punpckldq  %%xmm6,%%xmm2                    \n"
    "movdqa     %%xmm2,%%xmm6                    \n"
    "movq       %%xmm2,(%1)                      \n"
    "palignr    $0x8,%%xmm6,%%xmm6               \n"
    "punpckldq  %%xmm5,%%xmm1                    \n"
    "movq       %%xmm6,(%1,%4)                   \n"
    "lea        (%1,%4,2),%1                     \n"
    "movdqa     %%xmm1,%%xmm5                    \n"
    "movq       %%xmm1,(%1)                      \n"
    "palignr    $0x8,%%xmm5,%%xmm5               \n"
    "movq       %%xmm5,(%1,%4)                   \n"
    "lea        (%1,%4,2),%1                     \n"
    "punpckldq  %%xmm7,%%xmm3                    \n"
    "movq       %%xmm3,(%1)                      \n"
    "movdqa     %%xmm3,%%xmm7                    \n"
    "palignr    $0x8,%%xmm7,%%xmm7               \n"
    "sub        $0x8,%2                          \n"
    "movq       %%xmm7,(%1,%4)                   \n"
    "lea        (%1,%4,2),%1                     \n"
    "jg         1b                               \n"
    : "+r"(src),    // %0
      "+r"(dst),    // %1
      "+r"(width)   // %2
    : "r"((intptr_t)(src_stride)),  // %3
      "r"((intptr_t)(dst_stride))   // %4
    : "memory", "cc",
      "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
  );
}

#if !defined(LIBYUV_DISABLE_X86) && defined(__i386__)  && !defined(__clang__)
void TransposeUVWx8_SSE2(const uint8* src, int src_stride,
                         uint8* dst_a, int dst_stride_a,
                         uint8* dst_b, int dst_stride_b, int width);
  asm (
    DECLARE_FUNCTION(TransposeUVWx8_SSE2)
    "push   %ebx                               \n"
    "push   %esi                               \n"
    "push   %edi                               \n"
    "push   %ebp                               \n"
    "mov    0x14(%esp),%eax                    \n"
    "mov    0x18(%esp),%edi                    \n"
    "mov    0x1c(%esp),%edx                    \n"
    "mov    0x20(%esp),%esi                    \n"
    "mov    0x24(%esp),%ebx                    \n"
    "mov    0x28(%esp),%ebp                    \n"
    "mov    %esp,%ecx                          \n"
    "sub    $0x14,%esp                         \n"
    "and    $0xfffffff0,%esp                   \n"
    "mov    %ecx,0x10(%esp)                    \n"
    "mov    0x2c(%ecx),%ecx                    \n"

"1:                                            \n"
    "movdqu (%eax),%xmm0                       \n"
    "movdqu (%eax,%edi,1),%xmm1                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm0,%xmm7                        \n"
    "punpcklbw %xmm1,%xmm0                     \n"
    "punpckhbw %xmm1,%xmm7                     \n"
    "movdqa %xmm7,%xmm1                        \n"
    "movdqu (%eax),%xmm2                       \n"
    "movdqu (%eax,%edi,1),%xmm3                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm2,%xmm7                        \n"
    "punpcklbw %xmm3,%xmm2                     \n"
    "punpckhbw %xmm3,%xmm7                     \n"
    "movdqa %xmm7,%xmm3                        \n"
    "movdqu (%eax),%xmm4                       \n"
    "movdqu (%eax,%edi,1),%xmm5                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm4,%xmm7                        \n"
    "punpcklbw %xmm5,%xmm4                     \n"
    "punpckhbw %xmm5,%xmm7                     \n"
    "movdqa %xmm7,%xmm5                        \n"
    "movdqu (%eax),%xmm6                       \n"
    "movdqu (%eax,%edi,1),%xmm7                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqu %xmm5,(%esp)                       \n"
    "neg    %edi                               \n"
    "movdqa %xmm6,%xmm5                        \n"
    "punpcklbw %xmm7,%xmm6                     \n"
    "punpckhbw %xmm7,%xmm5                     \n"
    "movdqa %xmm5,%xmm7                        \n"
    "lea    0x10(%eax,%edi,8),%eax             \n"
    "neg    %edi                               \n"
    "movdqa %xmm0,%xmm5                        \n"
    "punpcklwd %xmm2,%xmm0                     \n"
    "punpckhwd %xmm2,%xmm5                     \n"
    "movdqa %xmm5,%xmm2                        \n"
    "movdqa %xmm1,%xmm5                        \n"
    "punpcklwd %xmm3,%xmm1                     \n"
    "punpckhwd %xmm3,%xmm5                     \n"
    "movdqa %xmm5,%xmm3                        \n"
    "movdqa %xmm4,%xmm5                        \n"
    "punpcklwd %xmm6,%xmm4                     \n"
    "punpckhwd %xmm6,%xmm5                     \n"
    "movdqa %xmm5,%xmm6                        \n"
    "movdqu (%esp),%xmm5                       \n"
    "movdqu %xmm6,(%esp)                       \n"
    "movdqa %xmm5,%xmm6                        \n"
    "punpcklwd %xmm7,%xmm5                     \n"
    "punpckhwd %xmm7,%xmm6                     \n"
    "movdqa %xmm6,%xmm7                        \n"
    "movdqa %xmm0,%xmm6                        \n"
    "punpckldq %xmm4,%xmm0                     \n"
    "punpckhdq %xmm4,%xmm6                     \n"
    "movdqa %xmm6,%xmm4                        \n"
    "movdqu (%esp),%xmm6                       \n"
    "movlpd %xmm0,(%edx)                       \n"
    "movhpd %xmm0,(%ebx)                       \n"
    "movlpd %xmm4,(%edx,%esi,1)                \n"
    "lea    (%edx,%esi,2),%edx                 \n"
    "movhpd %xmm4,(%ebx,%ebp,1)                \n"
    "lea    (%ebx,%ebp,2),%ebx                 \n"
    "movdqa %xmm2,%xmm0                        \n"
    "punpckldq %xmm6,%xmm2                     \n"
    "movlpd %xmm2,(%edx)                       \n"
    "movhpd %xmm2,(%ebx)                       \n"
    "punpckhdq %xmm6,%xmm0                     \n"
    "movlpd %xmm0,(%edx,%esi,1)                \n"
    "lea    (%edx,%esi,2),%edx                 \n"
    "movhpd %xmm0,(%ebx,%ebp,1)                \n"
    "lea    (%ebx,%ebp,2),%ebx                 \n"
    "movdqa %xmm1,%xmm0                        \n"
    "punpckldq %xmm5,%xmm1                     \n"
    "movlpd %xmm1,(%edx)                       \n"
    "movhpd %xmm1,(%ebx)                       \n"
    "punpckhdq %xmm5,%xmm0                     \n"
    "movlpd %xmm0,(%edx,%esi,1)                \n"
    "lea    (%edx,%esi,2),%edx                 \n"
    "movhpd %xmm0,(%ebx,%ebp,1)                \n"
    "lea    (%ebx,%ebp,2),%ebx                 \n"
    "movdqa %xmm3,%xmm0                        \n"
    "punpckldq %xmm7,%xmm3                     \n"
    "movlpd %xmm3,(%edx)                       \n"
    "movhpd %xmm3,(%ebx)                       \n"
    "punpckhdq %xmm7,%xmm0                     \n"
    "sub    $0x8,%ecx                          \n"
    "movlpd %xmm0,(%edx,%esi,1)                \n"
    "lea    (%edx,%esi,2),%edx                 \n"
    "movhpd %xmm0,(%ebx,%ebp,1)                \n"
    "lea    (%ebx,%ebp,2),%ebx                 \n"
    "jg     1b                                 \n"
    "mov    0x10(%esp),%esp                    \n"
    "pop    %ebp                               \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "pop    %ebx                               \n"
#if defined(__native_client__)
    "pop    %ecx                               \n"
    "and    $0xffffffe0,%ecx                   \n"
    "jmp    *%ecx                              \n"
#else
    "ret                                       \n"
#endif
);
#endif
#if !defined(LIBYUV_DISABLE_X86) && !defined(__native_client__) && \
    defined(__x86_64__)
// 64 bit version has enough registers to do 16x8 to 8x16 at a time.
void TransposeWx8_Fast_SSSE3(const uint8* src, int src_stride,
                             uint8* dst, int dst_stride, int width) {
  asm volatile (
  // Read in the data from the source pointer.
  // First round of bit swap.
  ".p2align  2                                 \n"
"1:                                            \n"
  "movdqu     (%0),%%xmm0                      \n"
  "movdqu     (%0,%3),%%xmm1                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm0,%%xmm8                    \n"
  "punpcklbw  %%xmm1,%%xmm0                    \n"
  "punpckhbw  %%xmm1,%%xmm8                    \n"
  "movdqu     (%0),%%xmm2                      \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "movdqa     %%xmm8,%%xmm9                    \n"
  "palignr    $0x8,%%xmm1,%%xmm1               \n"
  "palignr    $0x8,%%xmm9,%%xmm9               \n"
  "movdqu     (%0,%3),%%xmm3                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm2,%%xmm10                   \n"
  "punpcklbw  %%xmm3,%%xmm2                    \n"
  "punpckhbw  %%xmm3,%%xmm10                   \n"
  "movdqa     %%xmm2,%%xmm3                    \n"
  "movdqa     %%xmm10,%%xmm11                  \n"
  "movdqu     (%0),%%xmm4                      \n"
  "palignr    $0x8,%%xmm3,%%xmm3               \n"
  "palignr    $0x8,%%xmm11,%%xmm11             \n"
  "movdqu     (%0,%3),%%xmm5                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm4,%%xmm12                   \n"
  "punpcklbw  %%xmm5,%%xmm4                    \n"
  "punpckhbw  %%xmm5,%%xmm12                   \n"
  "movdqa     %%xmm4,%%xmm5                    \n"
  "movdqa     %%xmm12,%%xmm13                  \n"
  "movdqu     (%0),%%xmm6                      \n"
  "palignr    $0x8,%%xmm5,%%xmm5               \n"
  "palignr    $0x8,%%xmm13,%%xmm13             \n"
  "movdqu     (%0,%3),%%xmm7                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm6,%%xmm14                   \n"
  "punpcklbw  %%xmm7,%%xmm6                    \n"
  "punpckhbw  %%xmm7,%%xmm14                   \n"
  "neg        %3                               \n"
  "movdqa     %%xmm6,%%xmm7                    \n"
  "movdqa     %%xmm14,%%xmm15                  \n"
  "lea        0x10(%0,%3,8),%0                 \n"
  "palignr    $0x8,%%xmm7,%%xmm7               \n"
  "palignr    $0x8,%%xmm15,%%xmm15             \n"
  "neg        %3                               \n"
   // Second round of bit swap.
  "punpcklwd  %%xmm2,%%xmm0                    \n"
  "punpcklwd  %%xmm3,%%xmm1                    \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "movdqa     %%xmm1,%%xmm3                    \n"
  "palignr    $0x8,%%xmm2,%%xmm2               \n"
  "palignr    $0x8,%%xmm3,%%xmm3               \n"
  "punpcklwd  %%xmm6,%%xmm4                    \n"
  "punpcklwd  %%xmm7,%%xmm5                    \n"
  "movdqa     %%xmm4,%%xmm6                    \n"
  "movdqa     %%xmm5,%%xmm7                    \n"
  "palignr    $0x8,%%xmm6,%%xmm6               \n"
  "palignr    $0x8,%%xmm7,%%xmm7               \n"
  "punpcklwd  %%xmm10,%%xmm8                   \n"
  "punpcklwd  %%xmm11,%%xmm9                   \n"
  "movdqa     %%xmm8,%%xmm10                   \n"
  "movdqa     %%xmm9,%%xmm11                   \n"
  "palignr    $0x8,%%xmm10,%%xmm10             \n"
  "palignr    $0x8,%%xmm11,%%xmm11             \n"
  "punpcklwd  %%xmm14,%%xmm12                  \n"
  "punpcklwd  %%xmm15,%%xmm13                  \n"
  "movdqa     %%xmm12,%%xmm14                  \n"
  "movdqa     %%xmm13,%%xmm15                  \n"
  "palignr    $0x8,%%xmm14,%%xmm14             \n"
  "palignr    $0x8,%%xmm15,%%xmm15             \n"
  // Third round of bit swap.
  // Write to the destination pointer.
  "punpckldq  %%xmm4,%%xmm0                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movdqa     %%xmm0,%%xmm4                    \n"
  "palignr    $0x8,%%xmm4,%%xmm4               \n"
  "movq       %%xmm4,(%1,%4)                   \n"
  "lea        (%1,%4,2),%1                     \n"
  "punpckldq  %%xmm6,%%xmm2                    \n"
  "movdqa     %%xmm2,%%xmm6                    \n"
  "movq       %%xmm2,(%1)                      \n"
  "palignr    $0x8,%%xmm6,%%xmm6               \n"
  "punpckldq  %%xmm5,%%xmm1                    \n"
  "movq       %%xmm6,(%1,%4)                   \n"
  "lea        (%1,%4,2),%1                     \n"
  "movdqa     %%xmm1,%%xmm5                    \n"
  "movq       %%xmm1,(%1)                      \n"
  "palignr    $0x8,%%xmm5,%%xmm5               \n"
  "movq       %%xmm5,(%1,%4)                   \n"
  "lea        (%1,%4,2),%1                     \n"
  "punpckldq  %%xmm7,%%xmm3                    \n"
  "movq       %%xmm3,(%1)                      \n"
  "movdqa     %%xmm3,%%xmm7                    \n"
  "palignr    $0x8,%%xmm7,%%xmm7               \n"
  "movq       %%xmm7,(%1,%4)                   \n"
  "lea        (%1,%4,2),%1                     \n"
  "punpckldq  %%xmm12,%%xmm8                   \n"
  "movq       %%xmm8,(%1)                      \n"
  "movdqa     %%xmm8,%%xmm12                   \n"
  "palignr    $0x8,%%xmm12,%%xmm12             \n"
  "movq       %%xmm12,(%1,%4)                  \n"
  "lea        (%1,%4,2),%1                     \n"
  "punpckldq  %%xmm14,%%xmm10                  \n"
  "movdqa     %%xmm10,%%xmm14                  \n"
  "movq       %%xmm10,(%1)                     \n"
  "palignr    $0x8,%%xmm14,%%xmm14             \n"
  "punpckldq  %%xmm13,%%xmm9                   \n"
  "movq       %%xmm14,(%1,%4)                  \n"
  "lea        (%1,%4,2),%1                     \n"
  "movdqa     %%xmm9,%%xmm13                   \n"
  "movq       %%xmm9,(%1)                      \n"
  "palignr    $0x8,%%xmm13,%%xmm13             \n"
  "movq       %%xmm13,(%1,%4)                  \n"
  "lea        (%1,%4,2),%1                     \n"
  "punpckldq  %%xmm15,%%xmm11                  \n"
  "movq       %%xmm11,(%1)                     \n"
  "movdqa     %%xmm11,%%xmm15                  \n"
  "palignr    $0x8,%%xmm15,%%xmm15             \n"
  "sub        $0x10,%2                         \n"
  "movq       %%xmm15,(%1,%4)                  \n"
  "lea        (%1,%4,2),%1                     \n"
  "jg         1b                               \n"
  : "+r"(src),    // %0
    "+r"(dst),    // %1
    "+r"(width)   // %2
  : "r"((intptr_t)(src_stride)),  // %3
    "r"((intptr_t)(dst_stride))   // %4
  : "memory", "cc",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",  "xmm14",  "xmm15"
);
}

void TransposeUVWx8_SSE2(const uint8* src, int src_stride,
                         uint8* dst_a, int dst_stride_a,
                         uint8* dst_b, int dst_stride_b, int width) {
  asm volatile (
  // Read in the data from the source pointer.
  // First round of bit swap.
  ".p2align  2                                 \n"
"1:                                            \n"
  "movdqu     (%0),%%xmm0                      \n"
  "movdqu     (%0,%4),%%xmm1                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm0,%%xmm8                    \n"
  "punpcklbw  %%xmm1,%%xmm0                    \n"
  "punpckhbw  %%xmm1,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm1                    \n"
  "movdqu     (%0),%%xmm2                      \n"
  "movdqu     (%0,%4),%%xmm3                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm2,%%xmm8                    \n"
  "punpcklbw  %%xmm3,%%xmm2                    \n"
  "punpckhbw  %%xmm3,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm3                    \n"
  "movdqu     (%0),%%xmm4                      \n"
  "movdqu     (%0,%4),%%xmm5                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm4,%%xmm8                    \n"
  "punpcklbw  %%xmm5,%%xmm4                    \n"
  "punpckhbw  %%xmm5,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm5                    \n"
  "movdqu     (%0),%%xmm6                      \n"
  "movdqu     (%0,%4),%%xmm7                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm6,%%xmm8                    \n"
  "punpcklbw  %%xmm7,%%xmm6                    \n"
  "neg        %4                               \n"
  "lea        0x10(%0,%4,8),%0                 \n"
  "punpckhbw  %%xmm7,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm7                    \n"
  "neg        %4                               \n"
   // Second round of bit swap.
  "movdqa     %%xmm0,%%xmm8                    \n"
  "movdqa     %%xmm1,%%xmm9                    \n"
  "punpckhwd  %%xmm2,%%xmm8                    \n"
  "punpckhwd  %%xmm3,%%xmm9                    \n"
  "punpcklwd  %%xmm2,%%xmm0                    \n"
  "punpcklwd  %%xmm3,%%xmm1                    \n"
  "movdqa     %%xmm8,%%xmm2                    \n"
  "movdqa     %%xmm9,%%xmm3                    \n"
  "movdqa     %%xmm4,%%xmm8                    \n"
  "movdqa     %%xmm5,%%xmm9                    \n"
  "punpckhwd  %%xmm6,%%xmm8                    \n"
  "punpckhwd  %%xmm7,%%xmm9                    \n"
  "punpcklwd  %%xmm6,%%xmm4                    \n"
  "punpcklwd  %%xmm7,%%xmm5                    \n"
  "movdqa     %%xmm8,%%xmm6                    \n"
  "movdqa     %%xmm9,%%xmm7                    \n"
  // Third round of bit swap.
  // Write to the destination pointer.
  "movdqa     %%xmm0,%%xmm8                    \n"
  "punpckldq  %%xmm4,%%xmm0                    \n"
  "movlpd     %%xmm0,(%1)                      \n"  // Write back U channel
  "movhpd     %%xmm0,(%2)                      \n"  // Write back V channel
  "punpckhdq  %%xmm4,%%xmm8                    \n"
  "movlpd     %%xmm8,(%1,%5)                   \n"
  "lea        (%1,%5,2),%1                     \n"
  "movhpd     %%xmm8,(%2,%6)                   \n"
  "lea        (%2,%6,2),%2                     \n"
  "movdqa     %%xmm2,%%xmm8                    \n"
  "punpckldq  %%xmm6,%%xmm2                    \n"
  "movlpd     %%xmm2,(%1)                      \n"
  "movhpd     %%xmm2,(%2)                      \n"
  "punpckhdq  %%xmm6,%%xmm8                    \n"
  "movlpd     %%xmm8,(%1,%5)                   \n"
  "lea        (%1,%5,2),%1                     \n"
  "movhpd     %%xmm8,(%2,%6)                   \n"
  "lea        (%2,%6,2),%2                     \n"
  "movdqa     %%xmm1,%%xmm8                    \n"
  "punpckldq  %%xmm5,%%xmm1                    \n"
  "movlpd     %%xmm1,(%1)                      \n"
  "movhpd     %%xmm1,(%2)                      \n"
  "punpckhdq  %%xmm5,%%xmm8                    \n"
  "movlpd     %%xmm8,(%1,%5)                   \n"
  "lea        (%1,%5,2),%1                     \n"
  "movhpd     %%xmm8,(%2,%6)                   \n"
  "lea        (%2,%6,2),%2                     \n"
  "movdqa     %%xmm3,%%xmm8                    \n"
  "punpckldq  %%xmm7,%%xmm3                    \n"
  "movlpd     %%xmm3,(%1)                      \n"
  "movhpd     %%xmm3,(%2)                      \n"
  "punpckhdq  %%xmm7,%%xmm8                    \n"
  "sub        $0x8,%3                          \n"
  "movlpd     %%xmm8,(%1,%5)                   \n"
  "lea        (%1,%5,2),%1                     \n"
  "movhpd     %%xmm8,(%2,%6)                   \n"
  "lea        (%2,%6,2),%2                     \n"
  "jg         1b                               \n"
  : "+r"(src),    // %0
    "+r"(dst_a),  // %1
    "+r"(dst_b),  // %2
    "+r"(width)   // %3
  : "r"((intptr_t)(src_stride)),    // %4
    "r"((intptr_t)(dst_stride_a)),  // %5
    "r"((intptr_t)(dst_stride_b))   // %6
  : "memory", "cc",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9"
);
}
#endif
#endif

#endif  // defined(__x86_64__) || defined(__i386__)

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
