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

#ifndef DAV1D_SRC_ARM_32_UTIL_S
#define DAV1D_SRC_ARM_32_UTIL_S

#include "config.h"
#include "src/arm/asm.S"

.macro movrel_local rd, val, offset=0
#if defined(PIC)
        ldr             \rd,  90001f
        b               90002f
90001:
        .word           \val + \offset - (90002f + 8 - 4 * CONFIG_THUMB)
90002:
        add             \rd,  \rd,  pc
#else
        movw            \rd, #:lower16:\val+\offset
        movt            \rd, #:upper16:\val+\offset
#endif
.endm

.macro movrel rd, val, offset=0
#if defined(PIC) && defined(__APPLE__)
        ldr             \rd,  1f
        b               2f
1:
        .word           3f - (2f + 8 - 4 * CONFIG_THUMB)
2:
        ldr             \rd,  [pc, \rd]
.if \offset < 0
        sub             \rd,  \rd,  #-(\offset)
.elseif \offset > 0
        add             \rd,  \rd,  #\offset
.endif
        .non_lazy_symbol_pointer
3:
        .indirect_symbol \val
        .word       0
        .text
#else
        movrel_local    \rd, \val, \offset
#endif
.endm

.macro transpose_8x8b q0, q1, q2, q3, r0, r1, r2, r3, r4, r5, r6, r7
        vtrn.32         \q0,  \q2
        vtrn.32         \q1,  \q3

        vtrn.16         \r0,  \r2
        vtrn.16         \r1,  \r3
        vtrn.16         \r4,  \r6
        vtrn.16         \r5,  \r7

        vtrn.8          \r0,  \r1
        vtrn.8          \r2,  \r3
        vtrn.8          \r4,  \r5
        vtrn.8          \r6,  \r7
.endm

.macro transpose_4x8b q0, q1, r0, r1, r2, r3
        vtrn.16         \q0,  \q1

        vtrn.8          \r0,  \r1
        vtrn.8          \r2,  \r3
.endm

#endif /* DAV1D_SRC_ARM_32_UTIL_S */
