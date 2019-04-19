/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2015 Martin Storsjo
 * Copyright © 2015 Janne Grunau
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
 *****************************************************************************/

#ifndef DAV1D_SRC_ARM_64_UTIL_S
#define DAV1D_SRC_ARM_64_UTIL_S

#include "config.h"
#include "src/arm/asm.S"

.macro  movrel rd, val, offset=0
#if defined(__APPLE__)
  .if \offset < 0
        adrp            \rd, \val@PAGE
        add             \rd, \rd, \val@PAGEOFF
        sub             \rd, \rd, -(\offset)
  .else
        adrp            \rd, \val+(\offset)@PAGE
        add             \rd, \rd, \val+(\offset)@PAGEOFF
  .endif
#elif defined(PIC) && defined(_WIN32)
  .if \offset < 0
        adrp            \rd, \val
        add             \rd, \rd, :lo12:\val
        sub             \rd, \rd, -(\offset)
  .else
        adrp            \rd, \val+(\offset)
        add             \rd, \rd, :lo12:\val+(\offset)
  .endif
#elif defined(PIC)
        adrp            \rd, \val+(\offset)
        add             \rd, \rd, :lo12:\val+(\offset)
#else
        ldr             \rd, =\val+\offset
#endif
.endm

.macro transpose_8x8b r0, r1, r2, r3, r4, r5, r6, r7, r8, r9
        trn1            \r8\().8b,  \r0\().8b,  \r1\().8b
        trn2            \r9\().8b,  \r0\().8b,  \r1\().8b
        trn1            \r1\().8b,  \r2\().8b,  \r3\().8b
        trn2            \r3\().8b,  \r2\().8b,  \r3\().8b
        trn1            \r0\().8b,  \r4\().8b,  \r5\().8b
        trn2            \r5\().8b,  \r4\().8b,  \r5\().8b
        trn1            \r2\().8b,  \r6\().8b,  \r7\().8b
        trn2            \r7\().8b,  \r6\().8b,  \r7\().8b

        trn1            \r4\().4h,  \r0\().4h,  \r2\().4h
        trn2            \r2\().4h,  \r0\().4h,  \r2\().4h
        trn1            \r6\().4h,  \r5\().4h,  \r7\().4h
        trn2            \r7\().4h,  \r5\().4h,  \r7\().4h
        trn1            \r5\().4h,  \r9\().4h,  \r3\().4h
        trn2            \r9\().4h,  \r9\().4h,  \r3\().4h
        trn1            \r3\().4h,  \r8\().4h,  \r1\().4h
        trn2            \r8\().4h,  \r8\().4h,  \r1\().4h

        trn1            \r0\().2s,  \r3\().2s,  \r4\().2s
        trn2            \r4\().2s,  \r3\().2s,  \r4\().2s
        trn1            \r1\().2s,  \r5\().2s,  \r6\().2s
        trn2            \r5\().2s,  \r5\().2s,  \r6\().2s
        trn2            \r6\().2s,  \r8\().2s,  \r2\().2s
        trn1            \r2\().2s,  \r8\().2s,  \r2\().2s
        trn1            \r3\().2s,  \r9\().2s,  \r7\().2s
        trn2            \r7\().2s,  \r9\().2s,  \r7\().2s
.endm

.macro transpose_8x16b r0, r1, r2, r3, r4, r5, r6, r7, r8, r9
        trn1            \r8\().16b, \r0\().16b, \r1\().16b
        trn2            \r9\().16b, \r0\().16b, \r1\().16b
        trn1            \r1\().16b, \r2\().16b, \r3\().16b
        trn2            \r3\().16b, \r2\().16b, \r3\().16b
        trn1            \r0\().16b, \r4\().16b, \r5\().16b
        trn2            \r5\().16b, \r4\().16b, \r5\().16b
        trn1            \r2\().16b, \r6\().16b, \r7\().16b
        trn2            \r7\().16b, \r6\().16b, \r7\().16b

        trn1            \r4\().8h,  \r0\().8h,  \r2\().8h
        trn2            \r2\().8h,  \r0\().8h,  \r2\().8h
        trn1            \r6\().8h,  \r5\().8h,  \r7\().8h
        trn2            \r7\().8h,  \r5\().8h,  \r7\().8h
        trn1            \r5\().8h,  \r9\().8h,  \r3\().8h
        trn2            \r9\().8h,  \r9\().8h,  \r3\().8h
        trn1            \r3\().8h,  \r8\().8h,  \r1\().8h
        trn2            \r8\().8h,  \r8\().8h,  \r1\().8h

        trn1            \r0\().4s,  \r3\().4s,  \r4\().4s
        trn2            \r4\().4s,  \r3\().4s,  \r4\().4s
        trn1            \r1\().4s,  \r5\().4s,  \r6\().4s
        trn2            \r5\().4s,  \r5\().4s,  \r6\().4s
        trn2            \r6\().4s,  \r8\().4s,  \r2\().4s
        trn1            \r2\().4s,  \r8\().4s,  \r2\().4s
        trn1            \r3\().4s,  \r9\().4s,  \r7\().4s
        trn2            \r7\().4s,  \r9\().4s,  \r7\().4s
.endm

.macro  transpose_4x16b r0, r1, r2, r3, t4, t5, t6, t7
        trn1            \t4\().16b, \r0\().16b, \r1\().16b
        trn2            \t5\().16b, \r0\().16b, \r1\().16b
        trn1            \t6\().16b, \r2\().16b, \r3\().16b
        trn2            \t7\().16b, \r2\().16b, \r3\().16b

        trn1            \r0\().8h,  \t4\().8h,  \t6\().8h
        trn2            \r2\().8h,  \t4\().8h,  \t6\().8h
        trn1            \r1\().8h,  \t5\().8h,  \t7\().8h
        trn2            \r3\().8h,  \t5\().8h,  \t7\().8h
.endm

#endif /* DAV1D_SRC_ARM_64_UTIL_S */
