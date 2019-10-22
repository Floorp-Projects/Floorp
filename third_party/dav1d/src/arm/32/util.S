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
#elif defined(PIC)
        ldr             \rd,  1f
        b               2f
1:
        .word           \val + \offset - (2f + 8 - 4 * CONFIG_THUMB)
2:
        add             \rd,  \rd,  pc
#else
        movw            \rd, #:lower16:\val+\offset
        movt            \rd, #:upper16:\val+\offset
#endif
.endm

#endif /* DAV1D_SRC_ARM_32_UTIL_S */
