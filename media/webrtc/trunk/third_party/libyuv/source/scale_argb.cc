/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>  // For getenv()

#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"  // For CopyARGB
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// Bilinear SSE2 is disabled.
#define SSE2_DISABLED 1

// ARGB scaling uses bilinear or point, but not box filter.
/**
 * SSE2 downscalers with bilinear interpolation.
 */

#if !defined(YUV_DISABLE_ASM) && defined(_M_IX86)

#define HAS_SCALEARGBROWDOWN2_SSE2
// Reads 8 pixels, throws half away and writes 4 even pixels (0, 2, 4, 6)
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 16 byte aligned.
__declspec(naked) __declspec(align(16))
static void ScaleARGBRowDown2_SSE2(const uint8* src_ptr,
                                   ptrdiff_t /* src_stride */,
                                   uint8* dst_ptr, int dst_width) {
  __asm {
    mov        eax, [esp + 4]        // src_ptr
                                     // src_stride ignored
    mov        edx, [esp + 12]       // dst_ptr
    mov        ecx, [esp + 16]       // dst_width

    align      16
  wloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    shufps     xmm0, xmm1, 0x88
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    ret
  }
}

// Blends 8x2 rectangle to 4x1.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 16 byte aligned.
__declspec(naked) __declspec(align(16))
static void ScaleARGBRowDown2Int_SSE2(const uint8* src_ptr,
                                      ptrdiff_t src_stride,
                                      uint8* dst_ptr, int dst_width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]    // src_ptr
    mov        esi, [esp + 4 + 8]    // src_stride
    mov        edx, [esp + 4 + 12]   // dst_ptr
    mov        ecx, [esp + 4 + 16]   // dst_width

    align      16
  wloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2            // average rows
    pavgb      xmm1, xmm3
    movdqa     xmm2, xmm0            // average columns (8 to 4 pixels)
    shufps     xmm0, xmm1, 0x88      // even pixels
    shufps     xmm2, xmm1, 0xdd      // odd pixels
    pavgb      xmm0, xmm2
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    pop        esi
    ret
  }
}

#define HAS_SCALEARGBROWDOWNEVEN_SSE2
// Reads 4 pixels at a time.
// Alignment requirement: dst_ptr 16 byte aligned.
__declspec(naked) __declspec(align(16))
void ScaleARGBRowDownEven_SSE2(const uint8* src_ptr, ptrdiff_t src_stride,
                               int src_stepx,
                               uint8* dst_ptr, int dst_width) {
  __asm {
    push       ebx
    push       edi
    mov        eax, [esp + 8 + 4]    // src_ptr
                                     // src_stride ignored
    mov        ebx, [esp + 8 + 12]   // src_stepx
    mov        edx, [esp + 8 + 16]   // dst_ptr
    mov        ecx, [esp + 8 + 20]   // dst_width
    lea        ebx, [ebx * 4]
    lea        edi, [ebx + ebx * 2]

    align      16
  wloop:
    movd       xmm0, [eax]
    movd       xmm1, [eax + ebx]
    punpckldq  xmm0, xmm1
    movd       xmm2, [eax + ebx * 2]
    movd       xmm3, [eax + edi]
    lea        eax,  [eax + ebx * 4]
    punpckldq  xmm2, xmm3
    punpcklqdq xmm0, xmm2
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    pop        edi
    pop        ebx
    ret
  }
}

// Blends four 2x2 to 4x1.
// Alignment requirement: dst_ptr 16 byte aligned.
__declspec(naked) __declspec(align(16))
static void ScaleARGBRowDownEvenInt_SSE2(const uint8* src_ptr,
                                         ptrdiff_t src_stride,
                                         int src_stepx,
                                         uint8* dst_ptr, int dst_width) {
  __asm {
    push       ebx
    push       esi
    push       edi
    mov        eax, [esp + 12 + 4]    // src_ptr
    mov        esi, [esp + 12 + 8]    // src_stride
    mov        ebx, [esp + 12 + 12]   // src_stepx
    mov        edx, [esp + 12 + 16]   // dst_ptr
    mov        ecx, [esp + 12 + 20]   // dst_width
    lea        esi, [eax + esi]      // row1 pointer
    lea        ebx, [ebx * 4]
    lea        edi, [ebx + ebx * 2]

    align      16
  wloop:
    movq       xmm0, qword ptr [eax] // row0 4 pairs
    movhps     xmm0, qword ptr [eax + ebx]
    movq       xmm1, qword ptr [eax + ebx * 2]
    movhps     xmm1, qword ptr [eax + edi]
    lea        eax,  [eax + ebx * 4]
    movq       xmm2, qword ptr [esi] // row1 4 pairs
    movhps     xmm2, qword ptr [esi + ebx]
    movq       xmm3, qword ptr [esi + ebx * 2]
    movhps     xmm3, qword ptr [esi + edi]
    lea        esi,  [esi + ebx * 4]
    pavgb      xmm0, xmm2            // average rows
    pavgb      xmm1, xmm3
    movdqa     xmm2, xmm0            // average columns (8 to 4 pixels)
    shufps     xmm0, xmm1, 0x88      // even pixels
    shufps     xmm2, xmm1, 0xdd      // odd pixels
    pavgb      xmm0, xmm2
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         wloop

    pop        edi
    pop        esi
    pop        ebx
    ret
  }
}

// Bilinear row filtering combines 4x2 -> 4x1. SSE2 version.
#ifndef SSE2_DISABLED
#define HAS_SCALEARGBFILTERROWS_SSE2_DISABLED
__declspec(naked) __declspec(align(16))
void ScaleARGBFilterRows_SSE2(uint8* dst_ptr, const uint8* src_ptr,
                              ptrdiff_t src_stride, int dst_width,
                              int source_y_fraction) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]   // dst_ptr
    mov        esi, [esp + 8 + 8]   // src_ptr
    mov        edx, [esp + 8 + 12]  // src_stride
    mov        ecx, [esp + 8 + 16]  // dst_width
    mov        eax, [esp + 8 + 20]  // source_y_fraction (0..255)
    sub        edi, esi
    cmp        eax, 0
    je         xloop1
    cmp        eax, 128
    je         xloop2

    movd       xmm5, eax            // xmm5 = y fraction
    punpcklbw  xmm5, xmm5
    punpcklwd  xmm5, xmm5
    pshufd     xmm5, xmm5, 0
    pxor       xmm4, xmm4

    // f * row1 + (1 - frac) row0
    // frac * (row1 - row0) + row0
    align      16
  xloop:
    movdqa     xmm0, [esi]  // row0
    movdqa     xmm2, [esi + edx]  // row1
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    punpcklbw  xmm2, xmm4
    punpckhbw  xmm3, xmm4
    punpcklbw  xmm0, xmm4
    punpckhbw  xmm1, xmm4
    psubw      xmm2, xmm0  // row1 - row0
    psubw      xmm3, xmm1
    pmulhw     xmm2, xmm5  // scale diff
    pmulhw     xmm3, xmm5
    paddw      xmm0, xmm2  // sum rows
    paddw      xmm1, xmm3
    packuswb   xmm0, xmm1
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop

    shufps     xmm0, xmm0, 0xff
    movdqa     [esi + edi], xmm0    // duplicate last pixel for filtering
    pop        edi
    pop        esi
    ret

    align      16
  xloop1:
    movdqa     xmm0, [esi]
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop1

    shufps     xmm0, xmm0, 0xff
    movdqa     [esi + edi], xmm0
    pop        edi
    pop        esi
    ret

    align      16
  xloop2:
    movdqa     xmm0, [esi]
    pavgb      xmm0, [esi + edx]
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop2

    shufps     xmm0, xmm0, 0xff
    movdqa     [esi + edi], xmm0
    pop        edi
    pop        esi
    ret
  }
}
#endif  // SSE2_DISABLED

// Bilinear row filtering combines 4x2 -> 4x1. SSSE3 version.
#define HAS_SCALEARGBFILTERROWS_SSSE3
__declspec(naked) __declspec(align(16))
void ScaleARGBFilterRows_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                               ptrdiff_t src_stride, int dst_width,
                               int source_y_fraction) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]   // dst_ptr
    mov        esi, [esp + 8 + 8]   // src_ptr
    mov        edx, [esp + 8 + 12]  // src_stride
    mov        ecx, [esp + 8 + 16]  // dst_width
    mov        eax, [esp + 8 + 20]  // source_y_fraction (0..255)
    sub        edi, esi
    shr        eax, 1
    cmp        eax, 0
    je         xloop1
    cmp        eax, 64
    je         xloop2
    movd       xmm0, eax  // high fraction 0..127
    neg        eax
    add        eax, 128
    movd       xmm5, eax  // low fraction 128..1
    punpcklbw  xmm5, xmm0
    punpcklwd  xmm5, xmm5
    pshufd     xmm5, xmm5, 0

    align      16
  xloop:
    movdqa     xmm0, [esi]
    movdqa     xmm2, [esi + edx]
    movdqa     xmm1, xmm0
    punpcklbw  xmm0, xmm2
    punpckhbw  xmm1, xmm2
    pmaddubsw  xmm0, xmm5
    pmaddubsw  xmm1, xmm5
    psrlw      xmm0, 7
    psrlw      xmm1, 7
    packuswb   xmm0, xmm1
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop

    shufps     xmm0, xmm0, 0xff
    movdqa     [esi + edi], xmm0    // duplicate last pixel for filtering
    pop        edi
    pop        esi
    ret

    align      16
  xloop1:
    movdqa     xmm0, [esi]
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop1

    shufps     xmm0, xmm0, 0xff
    movdqa     [esi + edi], xmm0
    pop        edi
    pop        esi
    ret

    align      16
  xloop2:
    movdqa     xmm0, [esi]
    pavgb      xmm0, [esi + edx]
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop2

    shufps     xmm0, xmm0, 0xff
    movdqa     [esi + edi], xmm0
    pop        edi
    pop        esi
    ret
  }
}

#elif !defined(YUV_DISABLE_ASM) && (defined(__x86_64__) || defined(__i386__))

// GCC versions of row functions are verbatim conversions from Visual C.
// Generated using gcc disassembly on Visual C object file:
// objdump -D yuvscaler.obj >yuvscaler.txt
#define HAS_SCALEARGBROWDOWN2_SSE2
static void ScaleARGBRowDown2_SSE2(const uint8* src_ptr,
                                   ptrdiff_t /* src_stride */,
                                   uint8* dst_ptr, int dst_width) {
  asm volatile (
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    0x10(%0),%%xmm1                 \n"
    "lea       0x20(%0),%0                     \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src_ptr),   // %0
    "+r"(dst_ptr),   // %1
    "+r"(dst_width)  // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}

static void ScaleARGBRowDown2Int_SSE2(const uint8* src_ptr,
                                      ptrdiff_t src_stride,
                                      uint8* dst_ptr, int dst_width) {
  asm volatile (
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%0),%%xmm0                     \n"
    "movdqa    0x10(%0),%%xmm1                 \n"
    "movdqa    (%0,%3,1),%%xmm2                \n"
    "movdqa    0x10(%0,%3,1),%%xmm3            \n"
    "lea       0x20(%0),%0                     \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm2             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1)                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
  : "+r"(src_ptr),    // %0
    "+r"(dst_ptr),    // %1
    "+r"(dst_width)   // %2
  : "r"(static_cast<intptr_t>(src_stride))   // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

#define HAS_SCALEARGBROWDOWNEVEN_SSE2
// Reads 4 pixels at a time.
// Alignment requirement: dst_ptr 16 byte aligned.
void ScaleARGBRowDownEven_SSE2(const uint8* src_ptr, ptrdiff_t src_stride,
                               int src_stepx,
                               uint8* dst_ptr, int dst_width) {
  intptr_t src_stepx_x4 = static_cast<intptr_t>(src_stepx);
  intptr_t src_stepx_x12 = 0;
  asm volatile (
    "lea       0x0(,%1,4),%1                   \n"
    "lea       (%1,%1,2),%4                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movd      (%0),%%xmm0                     \n"
    "movd      (%0,%1,1),%%xmm1                \n"
    "punpckldq %%xmm1,%%xmm0                   \n"
    "movd      (%0,%1,2),%%xmm2                \n"
    "movd      (%0,%4,1),%%xmm3                \n"
    "lea       (%0,%1,4),%0                    \n"
    "punpckldq %%xmm3,%%xmm2                   \n"
    "punpcklqdq %%xmm2,%%xmm0                  \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,(%2)                     \n"
    "lea       0x10(%2),%2                     \n"
    "jg        1b                              \n"
  : "+r"(src_ptr),       // %0
    "+r"(src_stepx_x4),  // %1
    "+r"(dst_ptr),       // %2
    "+r"(dst_width),     // %3
    "+r"(src_stepx_x12)  // %4
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

// Blends four 2x2 to 4x1.
// Alignment requirement: dst_ptr 16 byte aligned.
static void ScaleARGBRowDownEvenInt_SSE2(const uint8* src_ptr,
                                         ptrdiff_t src_stride, int src_stepx,
                                         uint8* dst_ptr, int dst_width) {
  intptr_t src_stepx_x4 = static_cast<intptr_t>(src_stepx);
  intptr_t src_stepx_x12 = 0;
  intptr_t row1 = static_cast<intptr_t>(src_stride);
  asm volatile (
    "lea       0x0(,%1,4),%1                   \n"
    "lea       (%1,%1,2),%4                    \n"
    "lea       (%0,%5,1),%5                    \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movq      (%0),%%xmm0                     \n"
    "movhps    (%0,%1,1),%%xmm0                \n"
    "movq      (%0,%1,2),%%xmm1                \n"
    "movhps    (%0,%4,1),%%xmm1                \n"
    "lea       (%0,%1,4),%0                    \n"
    "movq      (%5),%%xmm2                     \n"
    "movhps    (%5,%1,1),%%xmm2                \n"
    "movq      (%5,%1,2),%%xmm3                \n"
    "movhps    (%5,%4,1),%%xmm3                \n"
    "lea       (%5,%1,4),%5                    \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "pavgb     %%xmm3,%%xmm1                   \n"
    "movdqa    %%xmm0,%%xmm2                   \n"
    "shufps    $0x88,%%xmm1,%%xmm0             \n"
    "shufps    $0xdd,%%xmm1,%%xmm2             \n"
    "pavgb     %%xmm2,%%xmm0                   \n"
    "sub       $0x4,%3                         \n"
    "movdqa    %%xmm0,(%2)                     \n"
    "lea       0x10(%2),%2                     \n"
    "jg        1b                              \n"
  : "+r"(src_ptr),        // %0
    "+r"(src_stepx_x4),   // %1
    "+r"(dst_ptr),        // %2
    "+rm"(dst_width),     // %3
    "+r"(src_stepx_x12),  // %4
    "+r"(row1)            // %5
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3"
#endif
  );
}

#ifndef SSE2_DISABLED
// Bilinear row filtering combines 4x2 -> 4x1. SSE2 version
#define HAS_SCALEARGBFILTERROWS_SSE2_DISABLED
void ScaleARGBFilterRows_SSE2(uint8* dst_ptr, const uint8* src_ptr,
                              ptrdiff_t src_stride, int dst_width,
                              int source_y_fraction) {
  asm volatile (
    "sub       %1,%0                           \n"
    "cmp       $0x0,%3                         \n"
    "je        2f                              \n"
    "cmp       $0x80,%3                        \n"
    "je        3f                              \n"
    "movd      %3,%%xmm5                       \n"
    "punpcklbw %%xmm5,%%xmm5                   \n"
    "punpcklwd %%xmm5,%%xmm5                   \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    "pxor      %%xmm4,%%xmm4                   \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%1),%%xmm0                     \n"
    "movdqa    (%1,%4,1),%%xmm2                \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "movdqa    %%xmm2,%%xmm3                   \n"
    "punpcklbw %%xmm4,%%xmm2                   \n"
    "punpckhbw %%xmm4,%%xmm3                   \n"
    "punpcklbw %%xmm4,%%xmm0                   \n"
    "punpckhbw %%xmm4,%%xmm1                   \n"
    "psubw     %%xmm0,%%xmm2                   \n"
    "psubw     %%xmm1,%%xmm3                   \n"
    "pmulhw    %%xmm5,%%xmm2                   \n"
    "pmulhw    %%xmm5,%%xmm3                   \n"
    "paddw     %%xmm2,%%xmm0                   \n"
    "paddw     %%xmm3,%%xmm1                   \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
    "jmp       4f                              \n"
    ".p2align  4                               \n"
  "2:                                          \n"
    "movdqa    (%1),%%xmm0                     \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
    "lea       0x10(%1),%1                     \n"
    "jg        2b                              \n"
    "jmp       4f                              \n"
    ".p2align  4                               \n"
  "3:                                          \n"
    "movdqa    (%1),%%xmm0                     \n"
    "pavgb     (%1,%4,1),%%xmm0                \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
    "lea       0x10(%1),%1                     \n"
    "lea       0x10(%1),%1                     \n"
    "jg        3b                              \n"
    ".p2align  4                               \n"
  "4:                                          \n"
    "shufps    $0xff,%%xmm0,%%xmm0             \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
  : "+r"(dst_ptr),     // %0
    "+r"(src_ptr),     // %1
    "+r"(dst_width),   // %2
    "+r"(source_y_fraction)  // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif  // SSE2_DISABLED

// Bilinear row filtering combines 4x2 -> 4x1. SSSE3 version
#define HAS_SCALEARGBFILTERROWS_SSSE3
void ScaleARGBFilterRows_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                               ptrdiff_t src_stride, int dst_width,
                               int source_y_fraction) {
  asm volatile (
    "sub       %1,%0                           \n"
    "shr       %3                              \n"
    "cmp       $0x0,%3                         \n"
    "je        2f                              \n"
    "cmp       $0x40,%3                        \n"
    "je        3f                              \n"
    "movd      %3,%%xmm0                       \n"
    "neg       %3                              \n"
    "add       $0x80,%3                        \n"
    "movd      %3,%%xmm5                       \n"
    "punpcklbw %%xmm0,%%xmm5                   \n"
    "punpcklwd %%xmm5,%%xmm5                   \n"
    "pshufd    $0x0,%%xmm5,%%xmm5              \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqa    (%1),%%xmm0                     \n"
    "movdqa    (%1,%4,1),%%xmm2                \n"
    "movdqa    %%xmm0,%%xmm1                   \n"
    "punpcklbw %%xmm2,%%xmm0                   \n"
    "punpckhbw %%xmm2,%%xmm1                   \n"
    "pmaddubsw %%xmm5,%%xmm0                   \n"
    "pmaddubsw %%xmm5,%%xmm1                   \n"
    "psrlw     $0x7,%%xmm0                     \n"
    "psrlw     $0x7,%%xmm1                     \n"
    "packuswb  %%xmm1,%%xmm0                   \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
    "lea       0x10(%1),%1                     \n"
    "jg        1b                              \n"
    "jmp       4f                              \n"
    ".p2align  4                               \n"
  "2:                                          \n"
    "movdqa    (%1),%%xmm0                     \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
    "lea       0x10(%1),%1                     \n"
    "jg        2b                              \n"
    "jmp       4f                              \n"
    ".p2align  4                               \n"
  "3:                                          \n"
    "movdqa    (%1),%%xmm0                     \n"
    "pavgb     (%1,%4,1),%%xmm0                \n"
    "sub       $0x4,%2                         \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
    "lea       0x10(%1),%1                     \n"
    "jg        3b                              \n"
  "4:                                          \n"
    ".p2align  4                               \n"
    "shufps    $0xff,%%xmm0,%%xmm0             \n"
    "movdqa    %%xmm0,(%1,%0,1)                \n"
  : "+r"(dst_ptr),     // %0
    "+r"(src_ptr),     // %1
    "+r"(dst_width),   // %2
    "+r"(source_y_fraction)  // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm5"
#endif
  );
}
#endif  // defined(__x86_64__) || defined(__i386__)

static void ScaleARGBRowDown2_C(const uint8* src_ptr,
                                ptrdiff_t /* src_stride */,
                                uint8* dst_ptr, int dst_width) {
  const uint32* src = reinterpret_cast<const uint32*>(src_ptr);
  uint32* dst = reinterpret_cast<uint32*>(dst_ptr);

  for (int x = 0; x < dst_width - 1; x += 2) {
    dst[0] = src[0];
    dst[1] = src[2];
    src += 4;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[0];
  }
}

static void ScaleARGBRowDown2Int_C(const uint8* src_ptr, ptrdiff_t src_stride,
                                   uint8* dst_ptr, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    dst_ptr[0] = (src_ptr[0] + src_ptr[4] +
                  src_ptr[src_stride] + src_ptr[src_stride + 4] + 2) >> 2;
    dst_ptr[1] = (src_ptr[1] + src_ptr[5] +
                  src_ptr[src_stride + 1] + src_ptr[src_stride + 5] + 2) >> 2;
    dst_ptr[2] = (src_ptr[2] + src_ptr[6] +
                  src_ptr[src_stride + 2] + src_ptr[src_stride + 6] + 2) >> 2;
    dst_ptr[3] = (src_ptr[3] + src_ptr[7] +
                  src_ptr[src_stride + 3] + src_ptr[src_stride + 7] + 2) >> 2;
    src_ptr += 8;
    dst_ptr += 4;
  }
}

void ScaleARGBRowDownEven_C(const uint8* src_ptr, ptrdiff_t /* src_stride */,
                            int src_stepx,
                            uint8* dst_ptr, int dst_width) {
  const uint32* src = reinterpret_cast<const uint32*>(src_ptr);
  uint32* dst = reinterpret_cast<uint32*>(dst_ptr);

  for (int x = 0; x < dst_width - 1; x += 2) {
    dst[0] = src[0];
    dst[1] = src[src_stepx];
    src += src_stepx * 2;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[0];
  }
}

static void ScaleARGBRowDownEvenInt_C(const uint8* src_ptr,
                                      ptrdiff_t src_stride,
                                      int src_stepx,
                                      uint8* dst_ptr, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    dst_ptr[0] = (src_ptr[0] + src_ptr[4] +
                  src_ptr[src_stride] + src_ptr[src_stride + 4] + 2) >> 2;
    dst_ptr[1] = (src_ptr[1] + src_ptr[5] +
                  src_ptr[src_stride + 1] + src_ptr[src_stride + 5] + 2) >> 2;
    dst_ptr[2] = (src_ptr[2] + src_ptr[6] +
                  src_ptr[src_stride + 2] + src_ptr[src_stride + 6] + 2) >> 2;
    dst_ptr[3] = (src_ptr[3] + src_ptr[7] +
                  src_ptr[src_stride + 3] + src_ptr[src_stride + 7] + 2) >> 2;
    src_ptr += src_stepx * 4;
    dst_ptr += 4;
  }
}

// (1-f)a + fb can be replaced with a + f(b-a)

#define BLENDER1(a, b, f) (static_cast<int>(a) + \
    ((f) * (static_cast<int>(b) - static_cast<int>(a)) >> 16))

#define BLENDERC(a, b, f, s) static_cast<uint32>( \
    BLENDER1(((a) >> s) & 255, ((b) >> s) & 255, f) << s)

#define BLENDER(a, b, f) \
    BLENDERC(a, b, f, 24) | BLENDERC(a, b, f, 16) | \
    BLENDERC(a, b, f, 8) | BLENDERC(a, b, f, 0)

static void ScaleARGBFilterCols_C(uint8* dst_ptr, const uint8* src_ptr,
                                  int dst_width, int x, int dx) {
  const uint32* src = reinterpret_cast<const uint32*>(src_ptr);
  uint32* dst = reinterpret_cast<uint32*>(dst_ptr);
  for (int j = 0; j < dst_width - 1; j += 2) {
    int xi = x >> 16;
    uint32 a = src[xi];
    uint32 b = src[xi + 1];
    dst[0] = BLENDER(a, b, x & 0xffff);
    x += dx;
    xi = x >> 16;
    a = src[xi];
    b = src[xi + 1];
    dst[1] = BLENDER(a, b, x & 0xffff);
    x += dx;
    dst += 2;
  }
  if (dst_width & 1) {
    int xi = x >> 16;
    uint32 a = src[xi];
    uint32 b = src[xi + 1];
    dst[0] = BLENDER(a, b, x & 0xffff);
  }
}

static const int kMaxInputWidth = 2560;

// C version 2x2 -> 2x1
void ScaleARGBFilterRows_C(uint8* dst_ptr, const uint8* src_ptr,
                           ptrdiff_t src_stride,
                           int dst_width, int source_y_fraction) {
  assert(dst_width > 0);
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8* src_ptr1 = src_ptr + src_stride;
  uint8* end = dst_ptr + (dst_width << 2);
  do {
    dst_ptr[0] = (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction) >> 8;
    dst_ptr[1] = (src_ptr[1] * y0_fraction + src_ptr1[1] * y1_fraction) >> 8;
    dst_ptr[2] = (src_ptr[2] * y0_fraction + src_ptr1[2] * y1_fraction) >> 8;
    dst_ptr[3] = (src_ptr[3] * y0_fraction + src_ptr1[3] * y1_fraction) >> 8;
    dst_ptr[4] = (src_ptr[4] * y0_fraction + src_ptr1[4] * y1_fraction) >> 8;
    dst_ptr[5] = (src_ptr[5] * y0_fraction + src_ptr1[5] * y1_fraction) >> 8;
    dst_ptr[6] = (src_ptr[6] * y0_fraction + src_ptr1[6] * y1_fraction) >> 8;
    dst_ptr[7] = (src_ptr[7] * y0_fraction + src_ptr1[7] * y1_fraction) >> 8;
    src_ptr += 8;
    src_ptr1 += 8;
    dst_ptr += 8;
  } while (dst_ptr < end);
  // Duplicate the last pixel (4 bytes) for filtering.
  dst_ptr[0] = dst_ptr[-4];
  dst_ptr[1] = dst_ptr[-3];
  dst_ptr[2] = dst_ptr[-2];
  dst_ptr[3] = dst_ptr[-1];
}

/**
 * ScaleARGB ARGB, 1/2
 *
 * This is an optimized version for scaling down a ARGB to 1/2 of
 * its original size.
 *
 */
static void ScaleARGBDown2(int /* src_width */, int /* src_height */,
                           int dst_width, int dst_height,
                           int src_stride, int dst_stride,
                           const uint8* src_ptr, uint8* dst_ptr,
                           FilterMode filtering) {
  void (*ScaleARGBRowDown2)(const uint8* src_ptr, ptrdiff_t src_stride,
                            uint8* dst_ptr, int dst_width) =
      filtering ? ScaleARGBRowDown2Int_C : ScaleARGBRowDown2_C;
#if defined(HAS_SCALEARGBROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(dst_width, 4) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_ptr, 16) && IS_ALIGNED(dst_stride, 16)) {
    ScaleARGBRowDown2 = filtering ? ScaleARGBRowDown2Int_SSE2 :
        ScaleARGBRowDown2_SSE2;
  }
#endif

  // TODO(fbarchard): Loop through source height to allow odd height.
  for (int y = 0; y < dst_height; ++y) {
    ScaleARGBRowDown2(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 1);
    dst_ptr += dst_stride;
  }
}

/**
 * ScaleARGB ARGB Even
 *
 * This is an optimized version for scaling down a ARGB to even
 * multiple of its original size.
 *
 */
static void ScaleARGBDownEven(int src_width, int src_height,
                              int dst_width, int dst_height,
                              int src_stride, int dst_stride,
                              const uint8* src_ptr, uint8* dst_ptr,
                              FilterMode filtering) {
  assert(IS_ALIGNED(src_width, 2));
  assert(IS_ALIGNED(src_height, 2));
  void (*ScaleARGBRowDownEven)(const uint8* src_ptr, ptrdiff_t src_stride,
                               int src_step, uint8* dst_ptr, int dst_width) =
      filtering ? ScaleARGBRowDownEvenInt_C : ScaleARGBRowDownEven_C;
#if defined(HAS_SCALEARGBROWDOWNEVEN_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(dst_width, 4) &&
      IS_ALIGNED(dst_ptr, 16) && IS_ALIGNED(dst_stride, 16)) {
    ScaleARGBRowDownEven = filtering ? ScaleARGBRowDownEvenInt_SSE2 :
        ScaleARGBRowDownEven_SSE2;
  }
#endif
  int src_step = src_width / dst_width;
  // Adjust to point to center of box.
  int row_step = src_height / dst_height;
  int row_stride = row_step * src_stride;
  src_ptr += ((row_step >> 1) - 1) * src_stride + ((src_step >> 1) - 1) * 4;
  for (int y = 0; y < dst_height; ++y) {
    ScaleARGBRowDownEven(src_ptr, src_stride, src_step, dst_ptr, dst_width);
    src_ptr += row_stride;
    dst_ptr += dst_stride;
  }
}
/**
 * ScaleARGB ARGB to/from any dimensions, with bilinear
 * interpolation.
 */

static void ScaleARGBBilinear(int src_width, int src_height,
                              int dst_width, int dst_height,
                              int src_stride, int dst_stride,
                              const uint8* src_ptr, uint8* dst_ptr) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  assert(src_width <= kMaxInputWidth);
  SIMD_ALIGNED(uint8 row[kMaxInputWidth * 4 + 16]);
  void (*ScaleARGBFilterRows)(uint8* dst_ptr, const uint8* src_ptr,
                              ptrdiff_t src_stride,
                              int dst_width, int source_y_fraction) =
      ScaleARGBFilterRows_C;
#if defined(HAS_SCALEARGBFILTERROWS_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(src_stride, 16) && IS_ALIGNED(src_ptr, 16)) {
    ScaleARGBFilterRows = ScaleARGBFilterRows_SSE2;
  }
#endif
#if defined(HAS_SCALEARGBFILTERROWS_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      IS_ALIGNED(src_stride, 16) && IS_ALIGNED(src_ptr, 16)) {
    ScaleARGBFilterRows = ScaleARGBFilterRows_SSSE3;
  }
#endif
  int dx = (src_width << 16) / dst_width;
  int dy = (src_height << 16) / dst_height;
  int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
  int y = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);
  int maxy = (src_height > 1) ? ((src_height - 1) << 16) - 1 : 0;
  for (int j = 0; j < dst_height; ++j) {
    int yi = y >> 16;
    int yf = (y >> 8) & 255;
    const uint8* src = src_ptr + yi * src_stride;
    ScaleARGBFilterRows(row, src, src_stride, src_width, yf);
    ScaleARGBFilterCols_C(dst_ptr, row, dst_width, x, dx);
    dst_ptr += dst_stride;
    y += dy;
    if (y > maxy) {
      y = maxy;
    }
  }
}

// Scales a single row of pixels using point sampling.
// Code is adapted from libyuv bilinear yuv scaling, but with bilinear
//     interpolation off, and argb pixels instead of yuv.
static void ScaleARGBCols(uint8* dst_ptr, const uint8* src_ptr,
                          int dst_width, int x, int dx) {
  const uint32* src = reinterpret_cast<const uint32*>(src_ptr);
  uint32* dst = reinterpret_cast<uint32*>(dst_ptr);
  for (int j = 0; j < dst_width - 1; j += 2) {
    dst[0] = src[x >> 16];
    x += dx;
    dst[1] = src[x >> 16];
    x += dx;
    dst += 2;
  }
  if (dst_width & 1) {
    dst[0] = src[x >> 16];
  }
}

/**
 * ScaleARGB ARGB to/from any dimensions, without interpolation.
 * Fixed point math is used for performance: The upper 16 bits
 * of x and dx is the integer part of the source position and
 * the lower 16 bits are the fixed decimal part.
 */

static void ScaleARGBSimple(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const uint8* src_ptr, uint8* dst_ptr) {
  int dx = (src_width << 16) / dst_width;
  int dy = (src_height << 16) / dst_height;
  int x = (dx >= 65536) ? ((dx >> 1) - 32768) : (dx >> 1);
  int y = (dy >= 65536) ? ((dy >> 1) - 32768) : (dy >> 1);
  for (int i = 0; i < dst_height; ++i) {
    ScaleARGBCols(dst_ptr, src_ptr + (y >> 16) * src_stride, dst_width, x, dx);
    dst_ptr += dst_stride;
    y += dy;
  }
}

/**
 * ScaleARGB ARGB to/from any dimensions.
 */
static void ScaleARGBAnySize(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const uint8* src_ptr, uint8* dst_ptr,
                             FilterMode filtering) {
  if (!filtering || (src_width > kMaxInputWidth)) {
    ScaleARGBSimple(src_width, src_height, dst_width, dst_height,
                    src_stride, dst_stride, src_ptr, dst_ptr);
  } else {
    ScaleARGBBilinear(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src_ptr, dst_ptr);
  }
}

// ScaleARGB a ARGB.
//
// This function in turn calls a scaling function
// suitable for handling the desired resolutions.

static void ScaleARGB(const uint8* src, int src_stride,
                      int src_width, int src_height,
                      uint8* dst, int dst_stride,
                      int dst_width, int dst_height,
                      FilterMode filtering) {
#ifdef CPU_X86
  // environment variable overrides for testing.
  char *filter_override = getenv("LIBYUV_FILTER");
  if (filter_override) {
    filtering = (FilterMode)atoi(filter_override);  // NOLINT
  }
#endif
  if (dst_width == src_width && dst_height == src_height) {
    // Straight copy.
    ARGBCopy(src, src_stride, dst, dst_stride, dst_width, dst_height);
    return;
  }
  if (2 * dst_width == src_width && 2 * dst_height == src_height) {
    // Optimized 1/2.
    ScaleARGBDown2(src_width, src_height, dst_width, dst_height,
                   src_stride, dst_stride, src, dst, filtering);
    return;
  }
  int scale_down_x = src_width / dst_width;
  int scale_down_y = src_height / dst_height;
  if (dst_width * scale_down_x == src_width &&
      dst_height * scale_down_y == src_height) {
    if (!(scale_down_x & 1) && !(scale_down_y & 1)) {
      // Optimized even scale down. ie 4, 6, 8, 10x
      ScaleARGBDownEven(src_width, src_height, dst_width, dst_height,
                        src_stride, dst_stride, src, dst, filtering);
      return;
    }
    if ((scale_down_x & 1) && (scale_down_y & 1)) {
      filtering = kFilterNone;
    }
  }
  // Arbitrary scale up and/or down.
  ScaleARGBAnySize(src_width, src_height, dst_width, dst_height,
                   src_stride, dst_stride, src, dst, filtering);
}

// ScaleARGB an ARGB image.
LIBYUV_API
int ARGBScale(const uint8* src_argb, int src_stride_argb,
             int src_width, int src_height,
             uint8* dst_argb, int dst_stride_argb,
             int dst_width, int dst_height,
             FilterMode filtering) {
  if (!src_argb || src_width <= 0 || src_height == 0 ||
      !dst_argb || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    src_argb = src_argb + (src_height - 1) * src_stride_argb;
    src_stride_argb = -src_stride_argb;
  }
  ScaleARGB(src_argb, src_stride_argb, src_width, src_height,
            dst_argb, dst_stride_argb, dst_width, dst_height,
            filtering);
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
