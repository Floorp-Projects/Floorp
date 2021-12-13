/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_SRC_ARM_ASM_S
#define DAV1D_SRC_ARM_ASM_S

#include "config.h"

#if ARCH_AARCH64
#define x18 do_not_use_x18
#define w18 do_not_use_w18

/* Support macros for the Armv8.5-A Branch Target Identification feature which
 * requires emitting a .note.gnu.property section with the appropriate
 * architecture-dependent feature bits set.
 * Read more: "ELF for the Arm® 64-bit Architecture"
 */
#if defined(__ARM_FEATURE_BTI_DEFAULT) && (__ARM_FEATURE_BTI_DEFAULT == 1)
#define GNU_PROPERTY_AARCH64_BTI (1 << 0)   // Has Branch Target Identification
#define AARCH64_VALID_JUMP_CALL_TARGET hint #38  // BTI 'jc'
#define AARCH64_VALID_CALL_TARGET      hint #34  // BTI 'c'
#define AARCH64_VALID_JUMP_TARGET      hint #36  // BTI 'j'
#else
#define GNU_PROPERTY_AARCH64_BTI 0          // No Branch Target Identification
#define AARCH64_VALID_JUMP_CALL_TARGET
#define AARCH64_VALID_CALL_TARGET
#define AARCH64_VALID_JUMP_TARGET
#endif

#if (GNU_PROPERTY_AARCH64_BTI != 0)
        .pushsection .note.gnu.property, "a"
        .balign 8
        .long 4
        .long 0x10
        .long 0x5
        .asciz "GNU"
        .long 0xc0000000 /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
        .long 4
        .long GNU_PROPERTY_AARCH64_BTI
        .long 0
        .popsection
#endif
#endif

#if ARCH_ARM
        .syntax unified
#ifdef __ELF__
        .arch armv7-a
        .fpu neon
        .eabi_attribute 10, 0           // suppress Tag_FP_arch
        .eabi_attribute 12, 0           // suppress Tag_Advanced_SIMD_arch
        .section .note.GNU-stack,"",%progbits // Mark stack as non-executable
#endif

#ifdef _WIN32
#define CONFIG_THUMB 1
#else
#define CONFIG_THUMB 0
#endif

#if CONFIG_THUMB
        .thumb
#define A @
#define T
#else
#define A
#define T @
#endif
#endif

#if !defined(PIC)
#if defined(__PIC__)
#define PIC __PIC__
#elif defined(__pic__)
#define PIC __pic__
#endif
#endif

#ifndef PRIVATE_PREFIX
#define PRIVATE_PREFIX dav1d_
#endif

#define PASTE(a,b) a ## b
#define CONCAT(a,b) PASTE(a,b)

#ifdef PREFIX
#define EXTERN CONCAT(_,PRIVATE_PREFIX)
#else
#define EXTERN PRIVATE_PREFIX
#endif

.macro function name, export=0, align=2
    .macro endfunc
#ifdef __ELF__
        .size   \name, . - \name
#endif
#if HAVE_AS_FUNC
        .endfunc
#endif
        .purgem endfunc
    .endm
        .text
        .align \align
    .if \export
        .global EXTERN\name
#ifdef __ELF__
        .type   EXTERN\name, %function
        .hidden EXTERN\name
#elif defined(__MACH__)
        .private_extern EXTERN\name
#endif
#if HAVE_AS_FUNC
        .func   EXTERN\name
#endif
EXTERN\name:
    .else
#ifdef __ELF__
        .type \name, %function
#endif
#if HAVE_AS_FUNC
        .func \name
#endif
    .endif
\name:
#if ARCH_AARCH64
    .if \export
         AARCH64_VALID_CALL_TARGET
    .endif
#endif
.endm

.macro  const   name, export=0, align=2
    .macro endconst
#ifdef __ELF__
        .size   \name, . - \name
#endif
        .purgem endconst
    .endm
#if defined(_WIN32)
        .section        .rdata
#elif !defined(__MACH__)
        .section        .rodata
#else
        .const_data
#endif
        .align          \align
    .if \export
        .global EXTERN\name
#ifdef __ELF__
        .hidden EXTERN\name
#elif defined(__MACH__)
        .private_extern EXTERN\name
#endif
EXTERN\name:
    .endif
\name:
.endm

#ifdef __APPLE__
#define L(x) L ## x
#else
#define L(x) .L ## x
#endif

#define X(x) CONCAT(EXTERN, x)


#endif /* DAV1D_SRC_ARM_ASM_S */
