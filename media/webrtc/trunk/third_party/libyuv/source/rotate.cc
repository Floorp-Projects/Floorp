/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/rotate.h"

#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"
#include "rotate_priv.h"
#include "row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if (defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)) && \
    !defined(YUV_DISABLE_ASM)
// Note static const preferred, but gives internal compiler error on gcc 4.2
// Shuffle table for reversing the bytes of UV channels.
uvec8 kShuffleReverseUV = {
  14u, 12u, 10u, 8u, 6u, 4u, 2u, 0u, 15u, 13u, 11u, 9u, 7u, 5u, 3u, 1u
};

#if defined(__APPLE__) && defined(__i386__)
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".private_extern _" #name "                \n"                             \
    ".align 4,0x90                             \n"                             \
"_" #name ":                                   \n"
#elif (defined(__MINGW32__) || defined(__CYGWIN__)) && defined(__i386__)
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".align 4,0x90                             \n"                             \
"_" #name ":                                   \n"
#else
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".align 4,0x90                             \n"                             \
#name ":                                       \n"
#endif
#endif

typedef void (*reverse_uv_func)(const uint8*, uint8*, uint8*, int);
typedef void (*rotate_uv_wx8_func)(const uint8*, int,
                                   uint8*, int,
                                   uint8*, int, int);
typedef void (*rotate_uv_wxh_func)(const uint8*, int,
                                   uint8*, int,
                                   uint8*, int, int, int);
typedef void (*rotate_wx8_func)(const uint8*, int, uint8*, int, int);
typedef void (*rotate_wxh_func)(const uint8*, int, uint8*, int, int, int);

#ifdef __ARM_NEON__
#define HAS_REVERSE_ROW_NEON
void ReverseRow_NEON(const uint8* src, uint8* dst, int width);
#define HAS_REVERSE_ROW_UV_NEON
void ReverseRowUV_NEON(const uint8* src,
                        uint8* dst_a, uint8* dst_b,
                        int width);
#define HAS_TRANSPOSE_WX8_NEON
void TransposeWx8_NEON(const uint8* src, int src_stride,
                       uint8* dst, int dst_stride, int width);
#define HAS_TRANSPOSE_UVWX8_NEON
void TransposeUVWx8_NEON(const uint8* src, int src_stride,
                         uint8* dst_a, int dst_stride_a,
                         uint8* dst_b, int dst_stride_b,
                         int width);
#endif

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_TRANSPOSE_WX8_SSSE3
__declspec(naked)
static void TransposeWx8_SSSE3(const uint8* src, int src_stride,
                               uint8* dst, int dst_stride, int width) {
__asm {
    push      edi
    push      esi
    push      ebp
    mov       eax, [esp + 12 + 4]   // src
    mov       edi, [esp + 12 + 8]   // src_stride
    mov       edx, [esp + 12 + 12]  // dst
    mov       esi, [esp + 12 + 16]  // dst_stride
    mov       ecx, [esp + 12 + 20]  // width
 convertloop:
    // Read in the data from the source pointer.
    // First round of bit swap.
    movq      xmm0, qword ptr [eax]
    lea       ebp, [eax + 8]
    movq      xmm1, qword ptr [eax + edi]
    lea       eax, [eax + 2 * edi]
    punpcklbw xmm0, xmm1
    movq      xmm2, qword ptr [eax]
    movdqa    xmm1, xmm0
    palignr   xmm1, xmm1, 8
    movq      xmm3, qword ptr [eax + edi]
    lea       eax, [eax + 2 * edi]
    punpcklbw xmm2, xmm3
    movdqa    xmm3, xmm2
    movq      xmm4, qword ptr [eax]
    palignr   xmm3, xmm3, 8
    movq      xmm5, qword ptr [eax + edi]
    punpcklbw xmm4, xmm5
    lea       eax, [eax + 2 * edi]
    movdqa    xmm5, xmm4
    movq      xmm6, qword ptr [eax]
    palignr   xmm5, xmm5, 8
    movq      xmm7, qword ptr [eax + edi]
    punpcklbw xmm6, xmm7
    mov       eax, ebp
    movdqa    xmm7, xmm6
    palignr   xmm7, xmm7, 8
    // Second round of bit swap.
    punpcklwd xmm0, xmm2
    punpcklwd xmm1, xmm3
    movdqa    xmm2, xmm0
    movdqa    xmm3, xmm1
    palignr   xmm2, xmm2, 8
    palignr   xmm3, xmm3, 8
    punpcklwd xmm4, xmm6
    punpcklwd xmm5, xmm7
    movdqa    xmm6, xmm4
    movdqa    xmm7, xmm5
    palignr   xmm6, xmm6, 8
    palignr   xmm7, xmm7, 8
    // Third round of bit swap.
    // Write to the destination pointer.
    punpckldq xmm0, xmm4
    movq      qword ptr [edx], xmm0
    movdqa    xmm4, xmm0
    palignr   xmm4, xmm4, 8
    movq      qword ptr [edx + esi], xmm4
    lea       edx, [edx + 2 * esi]
    punpckldq xmm2, xmm6
    movdqa    xmm6, xmm2
    palignr   xmm6, xmm6, 8
    movq      qword ptr [edx], xmm2
    punpckldq xmm1, xmm5
    movq      qword ptr [edx + esi], xmm6
    lea       edx, [edx + 2 * esi]
    movdqa    xmm5, xmm1
    movq      qword ptr [edx], xmm1
    palignr   xmm5, xmm5, 8
    punpckldq xmm3, xmm7
    movq      qword ptr [edx + esi], xmm5
    lea       edx, [edx + 2 * esi]
    movq      qword ptr [edx], xmm3
    movdqa    xmm7, xmm3
    palignr   xmm7, xmm7, 8
    movq      qword ptr [edx + esi], xmm7
    lea       edx, [edx + 2 * esi]
    sub       ecx, 8
    ja        convertloop

    pop       ebp
    pop       esi
    pop       edi
    ret
  }
}

#define HAS_TRANSPOSE_UVWX8_SSE2
__declspec(naked)
static void TransposeUVWx8_SSE2(const uint8* src, int src_stride,
                                uint8* dst_a, int dst_stride_a,
                                uint8* dst_b, int dst_stride_b,
                                int w) {
__asm {
    push      ebx
    push      esi
    push      edi
    push      ebp
    mov       eax, [esp + 16 + 4]   // src
    mov       edi, [esp + 16 + 8]   // src_stride
    mov       edx, [esp + 16 + 12]  // dst_a
    mov       esi, [esp + 16 + 16]  // dst_stride_a
    mov       ebx, [esp + 16 + 20]  // dst_b
    mov       ebp, [esp + 16 + 24]  // dst_stride_b
    mov       ecx, esp
    sub       esp, 4 + 16
    and       esp, ~15
    mov       [esp + 16], ecx
    mov       ecx, [ecx + 16 + 28]  // w
 convertloop:
    // Read in the data from the source pointer.
    // First round of bit swap.
    movdqa    xmm0, [eax]
    movdqa    xmm1, [eax + edi]
    lea       eax, [eax + 2 * edi]
    movdqa    xmm7, xmm0  // use xmm7 as temp register.
    punpcklbw xmm0, xmm1
    punpckhbw xmm7, xmm1
    movdqa    xmm1, xmm7
    movdqa    xmm2, [eax]
    movdqa    xmm3, [eax + edi]
    lea       eax, [eax + 2 * edi]
    movdqa    xmm7, xmm2
    punpcklbw xmm2, xmm3
    punpckhbw xmm7, xmm3
    movdqa    xmm3, xmm7
    movdqa    xmm4, [eax]
    movdqa    xmm5, [eax + edi]
    lea       eax, [eax + 2 * edi]
    movdqa    xmm7, xmm4
    punpcklbw xmm4, xmm5
    punpckhbw xmm7, xmm5
    movdqa    xmm5, xmm7
    movdqa    xmm6, [eax]
    movdqa    xmm7, [eax + edi]
    lea       eax, [eax + 2 * edi]
    movdqa    [esp], xmm5  // backup xmm5
    neg       edi
    movdqa    xmm5, xmm6   // use xmm5 as temp register.
    punpcklbw xmm6, xmm7
    punpckhbw xmm5, xmm7
    movdqa    xmm7, xmm5
    lea       eax, [eax + 8 * edi + 16]
    neg       edi
    // Second round of bit swap.
    movdqa    xmm5, xmm0
    punpcklwd xmm0, xmm2
    punpckhwd xmm5, xmm2
    movdqa    xmm2, xmm5
    movdqa    xmm5, xmm1
    punpcklwd xmm1, xmm3
    punpckhwd xmm5, xmm3
    movdqa    xmm3, xmm5
    movdqa    xmm5, xmm4
    punpcklwd xmm4, xmm6
    punpckhwd xmm5, xmm6
    movdqa    xmm6, xmm5
    movdqa    xmm5, [esp]  // restore xmm5
    movdqa    [esp], xmm6  // backup xmm6
    movdqa    xmm6, xmm5    // use xmm6 as temp register.
    punpcklwd xmm5, xmm7
    punpckhwd xmm6, xmm7
    movdqa    xmm7, xmm6
    // Third round of bit swap.
    // Write to the destination pointer.
    movdqa    xmm6, xmm0
    punpckldq xmm0, xmm4
    punpckhdq xmm6, xmm4
    movdqa    xmm4, xmm6
    movdqa    xmm6, [esp]  // restore xmm6
    movlpd    qword ptr [edx], xmm0
    movhpd    qword ptr [ebx], xmm0
    movlpd    qword ptr [edx + esi], xmm4
    lea       edx, [edx + 2 * esi]
    movhpd    qword ptr [ebx + ebp], xmm4
    lea       ebx, [ebx + 2 * ebp]
    movdqa    xmm0, xmm2   // use xmm0 as the temp register.
    punpckldq xmm2, xmm6
    movlpd    qword ptr [edx], xmm2
    movhpd    qword ptr [ebx], xmm2
    punpckhdq xmm0, xmm6
    movlpd    qword ptr [edx + esi], xmm0
    lea       edx, [edx + 2 * esi]
    movhpd    qword ptr [ebx + ebp], xmm0
    lea       ebx, [ebx + 2 * ebp]
    movdqa    xmm0, xmm1   // use xmm0 as the temp register.
    punpckldq xmm1, xmm5
    movlpd    qword ptr [edx], xmm1
    movhpd    qword ptr [ebx], xmm1
    punpckhdq xmm0, xmm5
    movlpd    qword ptr [edx + esi], xmm0
    lea       edx, [edx + 2 * esi]
    movhpd    qword ptr [ebx + ebp], xmm0
    lea       ebx, [ebx + 2 * ebp]
    movdqa    xmm0, xmm3   // use xmm0 as the temp register.
    punpckldq xmm3, xmm7
    movlpd    qword ptr [edx], xmm3
    movhpd    qword ptr [ebx], xmm3
    punpckhdq xmm0, xmm7
    movlpd    qword ptr [edx + esi], xmm0
    lea       edx, [edx + 2 * esi]
    movhpd    qword ptr [ebx + ebp], xmm0
    lea       ebx, [ebx + 2 * ebp]
    sub       ecx, 8
    ja        convertloop

    mov       esp, [esp + 16]
    pop       ebp
    pop       edi
    pop       esi
    pop       ebx
    ret
  }
}
#elif (defined(__i386__) || defined(__x86_64__)) && !defined(YUV_DISABLE_ASM)
#define HAS_TRANSPOSE_WX8_SSSE3
static void TransposeWx8_SSSE3(const uint8* src, int src_stride,
                               uint8* dst, int dst_stride, int width) {
  asm volatile (
  // Read in the data from the source pointer.
  // First round of bit swap.
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
  "movq       %%xmm7,(%1,%4)                   \n"
  "lea        (%1,%4,2),%1                     \n"
  "sub        $0x8,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src),    // %0
    "+r"(dst),    // %1
    "+r"(width)   // %2
  : "r"(static_cast<intptr_t>(src_stride)),  // %3
    "r"(static_cast<intptr_t>(dst_stride))   // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
);
}

#if defined (__i386__)
#define HAS_TRANSPOSE_UVWX8_SSE2
extern "C" void TransposeUVWx8_SSE2(const uint8* src, int src_stride,
                                    uint8* dst_a, int dst_stride_a,
                                    uint8* dst_b, int dst_stride_b,
                                    int w);
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
    "movdqa (%eax),%xmm0                       \n"
    "movdqa (%eax,%edi,1),%xmm1                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm0,%xmm7                        \n"
    "punpcklbw %xmm1,%xmm0                     \n"
    "punpckhbw %xmm1,%xmm7                     \n"
    "movdqa %xmm7,%xmm1                        \n"
    "movdqa (%eax),%xmm2                       \n"
    "movdqa (%eax,%edi,1),%xmm3                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm2,%xmm7                        \n"
    "punpcklbw %xmm3,%xmm2                     \n"
    "punpckhbw %xmm3,%xmm7                     \n"
    "movdqa %xmm7,%xmm3                        \n"
    "movdqa (%eax),%xmm4                       \n"
    "movdqa (%eax,%edi,1),%xmm5                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm4,%xmm7                        \n"
    "punpcklbw %xmm5,%xmm4                     \n"
    "punpckhbw %xmm5,%xmm7                     \n"
    "movdqa %xmm7,%xmm5                        \n"
    "movdqa (%eax),%xmm6                       \n"
    "movdqa (%eax,%edi,1),%xmm7                \n"
    "lea    (%eax,%edi,2),%eax                 \n"
    "movdqa %xmm5,(%esp)                       \n"
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
    "movdqa (%esp),%xmm5                       \n"
    "movdqa %xmm6,(%esp)                       \n"
    "movdqa %xmm5,%xmm6                        \n"
    "punpcklwd %xmm7,%xmm5                     \n"
    "punpckhwd %xmm7,%xmm6                     \n"
    "movdqa %xmm6,%xmm7                        \n"
    "movdqa %xmm0,%xmm6                        \n"
    "punpckldq %xmm4,%xmm0                     \n"
    "punpckhdq %xmm4,%xmm6                     \n"
    "movdqa %xmm6,%xmm4                        \n"
    "movdqa (%esp),%xmm6                       \n"
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
    "movlpd %xmm0,(%edx,%esi,1)                \n"
    "lea    (%edx,%esi,2),%edx                 \n"
    "movhpd %xmm0,(%ebx,%ebp,1)                \n"
    "lea    (%ebx,%ebp,2),%ebx                 \n"
    "sub    $0x8,%ecx                          \n"
    "ja     1b                                 \n"
    "mov    0x10(%esp),%esp                    \n"
    "pop    %ebp                               \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "pop    %ebx                               \n"
    "ret                                       \n"
);
#elif defined (__x86_64__)
// 64 bit version has enough registers to do 16x8 to 8x16 at a time.
#define HAS_TRANSPOSE_WX8_FAST_SSSE3
static void TransposeWx8_FAST_SSSE3(const uint8* src, int src_stride,
                                    uint8* dst, int dst_stride, int width) {
  asm volatile (
  // Read in the data from the source pointer.
  // First round of bit swap.
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     (%0,%3),%%xmm1                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm0,%%xmm8                    \n"
  "punpcklbw  %%xmm1,%%xmm0                    \n"
  "punpckhbw  %%xmm1,%%xmm8                    \n"
  "movdqa     (%0),%%xmm2                      \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "movdqa     %%xmm8,%%xmm9                    \n"
  "palignr    $0x8,%%xmm1,%%xmm1               \n"
  "palignr    $0x8,%%xmm9,%%xmm9               \n"
  "movdqa     (%0,%3),%%xmm3                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm2,%%xmm10                   \n"
  "punpcklbw  %%xmm3,%%xmm2                    \n"
  "punpckhbw  %%xmm3,%%xmm10                   \n"
  "movdqa     %%xmm2,%%xmm3                    \n"
  "movdqa     %%xmm10,%%xmm11                  \n"
  "movdqa     (%0),%%xmm4                      \n"
  "palignr    $0x8,%%xmm3,%%xmm3               \n"
  "palignr    $0x8,%%xmm11,%%xmm11             \n"
  "movdqa     (%0,%3),%%xmm5                   \n"
  "lea        (%0,%3,2),%0                     \n"
  "movdqa     %%xmm4,%%xmm12                   \n"
  "punpcklbw  %%xmm5,%%xmm4                    \n"
  "punpckhbw  %%xmm5,%%xmm12                   \n"
  "movdqa     %%xmm4,%%xmm5                    \n"
  "movdqa     %%xmm12,%%xmm13                  \n"
  "movdqa     (%0),%%xmm6                      \n"
  "palignr    $0x8,%%xmm5,%%xmm5               \n"
  "palignr    $0x8,%%xmm13,%%xmm13             \n"
  "movdqa     (%0,%3),%%xmm7                   \n"
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
  "movq       %%xmm15,(%1,%4)                  \n"
  "lea        (%1,%4,2),%1                     \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src),    // %0
    "+r"(dst),    // %1
    "+r"(width)   // %2
  : "r"(static_cast<intptr_t>(src_stride)),  // %3
    "r"(static_cast<intptr_t>(dst_stride))   // %4
  : "memory", "cc",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",  "xmm14",  "xmm15"
);
}

#define HAS_TRANSPOSE_UVWX8_SSE2
static void TransposeUVWx8_SSE2(const uint8* src, int src_stride,
                                uint8* dst_a, int dst_stride_a,
                                uint8* dst_b, int dst_stride_b,
                                int w) {
  asm volatile (
  // Read in the data from the source pointer.
  // First round of bit swap.
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     (%0,%4),%%xmm1                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm0,%%xmm8                    \n"
  "punpcklbw  %%xmm1,%%xmm0                    \n"
  "punpckhbw  %%xmm1,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm1                    \n"
  "movdqa     (%0),%%xmm2                      \n"
  "movdqa     (%0,%4),%%xmm3                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm2,%%xmm8                    \n"
  "punpcklbw  %%xmm3,%%xmm2                    \n"
  "punpckhbw  %%xmm3,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm3                    \n"
  "movdqa     (%0),%%xmm4                      \n"
  "movdqa     (%0,%4),%%xmm5                   \n"
  "lea        (%0,%4,2),%0                     \n"
  "movdqa     %%xmm4,%%xmm8                    \n"
  "punpcklbw  %%xmm5,%%xmm4                    \n"
  "punpckhbw  %%xmm5,%%xmm8                    \n"
  "movdqa     %%xmm8,%%xmm5                    \n"
  "movdqa     (%0),%%xmm6                      \n"
  "movdqa     (%0,%4),%%xmm7                   \n"
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
  "movlpd     %%xmm8,(%1,%5)                   \n"
  "lea        (%1,%5,2),%1                     \n"
  "movhpd     %%xmm8,(%2,%6)                   \n"
  "lea        (%2,%6,2),%2                     \n"
  "sub        $0x8,%3                          \n"
  "ja         1b                               \n"
  : "+r"(src),    // %0
    "+r"(dst_a),  // %1
    "+r"(dst_b),  // %2
    "+r"(w)   // %3
  : "r"(static_cast<intptr_t>(src_stride)),    // %4
    "r"(static_cast<intptr_t>(dst_stride_a)),  // %5
    "r"(static_cast<intptr_t>(dst_stride_b))   // %6
  : "memory", "cc",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9"
);
}
#endif
#endif

static void TransposeWx8_C(const uint8* src, int src_stride,
                           uint8* dst, int dst_stride,
                           int w) {
  int i;
  for (i = 0; i < w; ++i) {
    dst[0] = src[0 * src_stride];
    dst[1] = src[1 * src_stride];
    dst[2] = src[2 * src_stride];
    dst[3] = src[3 * src_stride];
    dst[4] = src[4 * src_stride];
    dst[5] = src[5 * src_stride];
    dst[6] = src[6 * src_stride];
    dst[7] = src[7 * src_stride];
    ++src;
    dst += dst_stride;
  }
}

static void TransposeWxH_C(const uint8* src, int src_stride,
                           uint8* dst, int dst_stride,
                           int width, int height) {
  int i, j;
  for (i = 0; i < width; ++i)
    for (j = 0; j < height; ++j)
      dst[i * dst_stride + j] = src[j * src_stride + i];
}

void TransposePlane(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height) {
  int i = height;
  rotate_wx8_func TransposeWx8;
  rotate_wxh_func TransposeWxH;

#if defined(HAS_TRANSPOSE_WX8_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    TransposeWx8 = TransposeWx8_NEON;
    TransposeWxH = TransposeWxH_C;
  } else
#endif
#if defined(HAS_TRANSPOSE_WX8_FAST_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst, 8) && IS_ALIGNED(dst_stride, 8)) {
    TransposeWx8 = TransposeWx8_FAST_SSSE3;
    TransposeWxH = TransposeWxH_C;
  } else
#endif
#if defined(HAS_TRANSPOSE_WX8_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(src, 8) && IS_ALIGNED(src_stride, 8) &&
      IS_ALIGNED(dst, 8) && IS_ALIGNED(dst_stride, 8)) {
    TransposeWx8 = TransposeWx8_SSSE3;
    TransposeWxH = TransposeWxH_C;
  } else
#endif
  {
    TransposeWx8 = TransposeWx8_C;
    TransposeWxH = TransposeWxH_C;
  }

  // work across the source in 8x8 tiles
  while (i >= 8) {
    TransposeWx8(src, src_stride, dst, dst_stride, width);

    src += 8 * src_stride;    // go down 8 rows
    dst += 8;                 // move over 8 columns
    i   -= 8;
  }

  TransposeWxH(src, src_stride, dst, dst_stride, width, i);
}

void RotatePlane90(const uint8* src, int src_stride,
                   uint8* dst, int dst_stride,
                   int width, int height) {
  // Rotate by 90 is a transpose with the source read
  // from bottom to top.  So set the source pointer to the end
  // of the buffer and flip the sign of the source stride.
  src += src_stride * (height - 1);
  src_stride = -src_stride;

  TransposePlane(src, src_stride, dst, dst_stride, width, height);
}

void RotatePlane270(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height) {
  // Rotate by 270 is a transpose with the destination written
  // from bottom to top.  So set the destination pointer to the end
  // of the buffer and flip the sign of the destination stride.
  dst += dst_stride * (width - 1);
  dst_stride = -dst_stride;

  TransposePlane(src, src_stride, dst, dst_stride, width, height);
}

void RotatePlane180(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride,
                    int width, int height) {
  void (*ReverseRow)(const uint8* src, uint8* dst, int width);
#if defined(HAS_REVERSE_ROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ReverseRow = ReverseRow_NEON;
  } else
#endif
#if defined(HAS_REVERSE_ROW_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst, 16) && IS_ALIGNED(dst_stride, 16)) {
    ReverseRow = ReverseRow_SSSE3;
  } else
#endif
#if defined(HAS_REVERSE_ROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst, 16) && IS_ALIGNED(dst_stride, 16)) {
    ReverseRow = ReverseRow_SSE2;
  } else
#endif
  {
    ReverseRow = ReverseRow_C;
  }

  // Rotate by 180 is a mirror and vertical flip
  src += src_stride * (height - 1);

  for (int y = 0; y < height; ++y) {
    ReverseRow(src, dst, width);
    src -= src_stride;
    dst += dst_stride;
  }
}

static void TransposeUVWx8_C(const uint8* src, int src_stride,
                             uint8* dst_a, int dst_stride_a,
                             uint8* dst_b, int dst_stride_b,
                             int w) {
  int i;
  for (i = 0; i < w; ++i) {
    dst_a[0] = src[0 * src_stride + 0];
    dst_b[0] = src[0 * src_stride + 1];
    dst_a[1] = src[1 * src_stride + 0];
    dst_b[1] = src[1 * src_stride + 1];
    dst_a[2] = src[2 * src_stride + 0];
    dst_b[2] = src[2 * src_stride + 1];
    dst_a[3] = src[3 * src_stride + 0];
    dst_b[3] = src[3 * src_stride + 1];
    dst_a[4] = src[4 * src_stride + 0];
    dst_b[4] = src[4 * src_stride + 1];
    dst_a[5] = src[5 * src_stride + 0];
    dst_b[5] = src[5 * src_stride + 1];
    dst_a[6] = src[6 * src_stride + 0];
    dst_b[6] = src[6 * src_stride + 1];
    dst_a[7] = src[7 * src_stride + 0];
    dst_b[7] = src[7 * src_stride + 1];
    src += 2;
    dst_a += dst_stride_a;
    dst_b += dst_stride_b;
  }
}

static void TransposeUVWxH_C(const uint8* src, int src_stride,
                             uint8* dst_a, int dst_stride_a,
                             uint8* dst_b, int dst_stride_b,
                             int w, int h) {
  int i, j;
  for (i = 0; i < w * 2; i += 2)
    for (j = 0; j < h; ++j) {
      dst_a[j + ((i >> 1) * dst_stride_a)] = src[i + (j * src_stride)];
      dst_b[j + ((i >> 1) * dst_stride_b)] = src[i + (j * src_stride) + 1];
    }
}

void TransposeUV(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height) {
  int i = height;
  rotate_uv_wx8_func TransposeWx8;
  rotate_uv_wxh_func TransposeWxH;

#if defined(HAS_TRANSPOSE_UVWX8_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    TransposeWx8 = TransposeUVWx8_NEON;
    TransposeWxH = TransposeUVWxH_C;
  } else
#endif
#if defined(HAS_TRANSPOSE_UVWX8_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(width, 8) &&
      IS_ALIGNED(src, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_a, 8) && IS_ALIGNED(dst_stride_a, 8) &&
      IS_ALIGNED(dst_b, 8) && IS_ALIGNED(dst_stride_b, 8)) {
    TransposeWx8 = TransposeUVWx8_SSE2;
    TransposeWxH = TransposeUVWxH_C;
  } else
#endif
  {
    TransposeWx8 = TransposeUVWx8_C;
    TransposeWxH = TransposeUVWxH_C;
  }

  // work through the source in 8x8 tiles
  while (i >= 8) {
    TransposeWx8(src, src_stride,
                 dst_a, dst_stride_a,
                 dst_b, dst_stride_b,
                 width);

    src   += 8 * src_stride;    // go down 8 rows
    dst_a += 8;                 // move over 8 columns
    dst_b += 8;                 // move over 8 columns
    i     -= 8;
  }

  TransposeWxH(src, src_stride,
               dst_a, dst_stride_a,
               dst_b, dst_stride_b,
               width, i);

}

void RotateUV90(const uint8* src, int src_stride,
                uint8* dst_a, int dst_stride_a,
                uint8* dst_b, int dst_stride_b,
                int width, int height) {
  src += src_stride * (height - 1);
  src_stride = -src_stride;

  TransposeUV(src, src_stride,
              dst_a, dst_stride_a,
              dst_b, dst_stride_b,
              width, height);
}

void RotateUV270(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height) {
  dst_a += dst_stride_a * (width - 1);
  dst_b += dst_stride_b * (width - 1);
  dst_stride_a = -dst_stride_a;
  dst_stride_b = -dst_stride_b;

  TransposeUV(src, src_stride,
              dst_a, dst_stride_a,
              dst_b, dst_stride_b,
              width, height);
}

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_REVERSE_ROW_UV_SSSE3
__declspec(naked)
void ReverseRowUV_SSSE3(const uint8* src,
                         uint8* dst_a, uint8* dst_b,
                         int width) {
__asm {
    push      edi
    mov       eax, [esp + 4 + 4]   // src
    mov       edx, [esp + 4 + 8]   // dst_a
    mov       edi, [esp + 4 + 12]  // dst_b
    mov       ecx, [esp + 4 + 16]  // width
    movdqa    xmm5, kShuffleReverseUV
    lea       eax, [eax + ecx * 2 - 16]

 convertloop:
    movdqa    xmm0, [eax]
    lea       eax, [eax - 16]
    pshufb    xmm0, xmm5
    movlpd    qword ptr [edx], xmm0
    movhpd    qword ptr [edi], xmm0
    lea       edx, [edx + 8]
    lea       edi, [edi + 8]
    sub       ecx, 8
    ja        convertloop
    pop       edi
    ret
  }
}

#elif (defined(__i386__) || defined(__x86_64__)) && \
    !defined(YUV_DISABLE_ASM)
#define HAS_REVERSE_ROW_UV_SSSE3
void ReverseRowUV_SSSE3(const uint8* src,
                        uint8* dst_a, uint8* dst_b,
                        int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
  "movdqa     %4,%%xmm5                        \n"
  "lea        -16(%0,%3,2),%0                  \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "lea        -16(%0),%0                       \n"
  "pshufb     %%xmm5,%%xmm0                    \n"
  "movlpd     %%xmm0,(%1)                      \n"
  "movhpd     %%xmm0,(%2)                      \n"
  "lea        8(%1),%1                         \n"
  "lea        8(%2),%2                         \n"
  "sub        $8,%3                            \n"
  "ja         1b                               \n"
  : "+r"(src),      // %0
    "+r"(dst_a),    // %1
    "+r"(dst_b),    // %2
    "+r"(temp_width)  // %3
  : "m"(kShuffleReverseUV) // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif
  );
}
#endif

static void ReverseRowUV_C(const uint8* src,
                            uint8* dst_a, uint8* dst_b,
                            int width) {
  int i;
  src += width << 1;
  for (i = 0; i < width; ++i) {
    src -= 2;
    dst_a[i] = src[0];
    dst_b[i] = src[1];
  }
}

void RotateUV180(const uint8* src, int src_stride,
                 uint8* dst_a, int dst_stride_a,
                 uint8* dst_b, int dst_stride_b,
                 int width, int height) {
  int i;
  reverse_uv_func ReverseRow;

#if defined(HAS_REVERSE_ROW_UV_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    ReverseRow = ReverseRowUV_NEON;
  } else
#endif
#if defined(HAS_REVERSE_ROW_UV_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_a, 8) && IS_ALIGNED(dst_stride_a, 8) &&
      IS_ALIGNED(dst_b, 8) && IS_ALIGNED(dst_stride_b, 8) ) {
    ReverseRow = ReverseRowUV_SSSE3;
  } else
#endif
  {
    ReverseRow = ReverseRowUV_C;
  }

  dst_a += dst_stride_a * (height - 1);
  dst_b += dst_stride_b * (height - 1);

  for (i = 0; i < height; ++i) {
    ReverseRow(src, dst_a, dst_b, width);

    src   += src_stride;      // down one line at a time
    dst_a -= dst_stride_a;    // nominally up one line at a time
    dst_b -= dst_stride_b;    // nominally up one line at a time
  }
}

int I420Rotate(const uint8* src_y, int src_stride_y,
               const uint8* src_u, int src_stride_u,
               const uint8* src_v, int src_stride_v,
               uint8* dst_y, int dst_stride_y,
               uint8* dst_u, int dst_stride_u,
               uint8* dst_v, int dst_stride_v,
               int width, int height,
               RotationMode mode) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;

  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (halfheight - 1) * src_stride_u;
    src_v = src_v + (halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  switch (mode) {
    case kRotate0:
      // copy frame
      return I420Copy(src_y, src_stride_y,
                      src_u, src_stride_u,
                      src_v, src_stride_v,
                      dst_y, dst_stride_y,
                      dst_u, dst_stride_u,
                      dst_v, dst_stride_v,
                      width, height);
    case kRotate90:
      RotatePlane90(src_y, src_stride_y,
                    dst_y, dst_stride_y,
                    width, height);
      RotatePlane90(src_u, src_stride_u,
                    dst_u, dst_stride_u,
                    halfwidth, halfheight);
      RotatePlane90(src_v, src_stride_v,
                    dst_v, dst_stride_v,
                    halfwidth, halfheight);
      return 0;
    case kRotate270:
      RotatePlane270(src_y, src_stride_y,
                     dst_y, dst_stride_y,
                     width, height);
      RotatePlane270(src_u, src_stride_u,
                     dst_u, dst_stride_u,
                     halfwidth, halfheight);
      RotatePlane270(src_v, src_stride_v,
                     dst_v, dst_stride_v,
                     halfwidth, halfheight);
      return 0;
    case kRotate180:
      RotatePlane180(src_y, src_stride_y,
                     dst_y, dst_stride_y,
                     width, height);
      RotatePlane180(src_u, src_stride_u,
                     dst_u, dst_stride_u,
                     halfwidth, halfheight);
      RotatePlane180(src_v, src_stride_v,
                     dst_v, dst_stride_v,
                     halfwidth, halfheight);
      return 0;
    default:
      break;
  }
  return -1;
}

int NV12ToI420Rotate(const uint8* src_y, int src_stride_y,
                     const uint8* src_uv, int src_stride_uv,
                     uint8* dst_y, int dst_stride_y,
                     uint8* dst_u, int dst_stride_u,
                     uint8* dst_v, int dst_stride_v,
                     int width, int height,
                     RotationMode mode) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;

  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    halfheight = (height + 1) >> 1;
    src_y = src_y + (height - 1) * src_stride_y;
    src_uv = src_uv + (halfheight - 1) * src_stride_uv;
    src_stride_y = -src_stride_y;
    src_stride_uv = -src_stride_uv;
  }

  switch (mode) {
    case kRotate0:
      // copy frame
      return NV12ToI420(src_y, src_stride_y,
                        src_uv, src_stride_uv,
                        dst_y, dst_stride_y,
                        dst_u, dst_stride_u,
                        dst_v, dst_stride_v,
                        width, height);
    case kRotate90:
      RotatePlane90(src_y, src_stride_y,
                    dst_y, dst_stride_y,
                    width, height);
      RotateUV90(src_uv, src_stride_uv,
                 dst_u, dst_stride_u,
                 dst_v, dst_stride_v,
                 halfwidth, halfheight);
      return 0;
    case kRotate270:
      RotatePlane270(src_y, src_stride_y,
                     dst_y, dst_stride_y,
                     width, height);
      RotateUV270(src_uv, src_stride_uv,
                  dst_u, dst_stride_u,
                  dst_v, dst_stride_v,
                  halfwidth, halfheight);
      return 0;
    case kRotate180:
      RotatePlane180(src_y, src_stride_y,
                     dst_y, dst_stride_y,
                     width, height);
      RotateUV180(src_uv, src_stride_uv,
                  dst_u, dst_stride_u,
                  dst_v, dst_stride_v,
                  halfwidth, halfheight);
      return 0;
    default:
      break;
  }
  return -1;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
