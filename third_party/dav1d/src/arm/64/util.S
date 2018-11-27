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

#ifndef __DAVID_SRC_ARM_64_UTIL_S__
#define __DAVID_SRC_ARM_64_UTIL_S__

#include "config.h"
#include "src/arm/asm.S"

.macro  movrel rd, val, offset=0
#if defined(__APPLE__)
  .if \offset < 0
    adrp        \rd, \val@PAGE
    add         \rd, \rd, \val@PAGEOFF
    sub         \rd, \rd, -(\offset)
  .else
    adrp        \rd, \val+(\offset)@PAGE
    add         \rd, \rd, \val+(\offset)@PAGEOFF
  .endif
#elif defined(PIC) && defined(_WIN32)
  .if \offset < 0
    adrp        \rd, \val
    add         \rd, \rd, :lo12:\val
    sub         \rd, \rd, -(\offset)
  .else
    adrp        \rd, \val+(\offset)
    add         \rd, \rd, :lo12:\val+(\offset)
  .endif
#elif defined(PIC)
    adrp        \rd, \val+(\offset)
    add         \rd, \rd, :lo12:\val+(\offset)
#else
    ldr         \rd, =\val+\offset
#endif
.endm

#endif /* __DAVID_SRC_ARM_64_UTIL_S__ */
