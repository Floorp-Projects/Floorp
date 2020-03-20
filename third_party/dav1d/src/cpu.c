/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
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
#include "config.h"

#include <stdint.h>

#include "src/cpu.h"

static unsigned flags = 0;
#if ARCH_X86
/* Disable AVX-512 by default for the time being */
static unsigned flags_mask = ~DAV1D_X86_CPU_FLAG_AVX512ICL;
#else
static unsigned flags_mask = -1;
#endif

COLD void dav1d_init_cpu(void) {
#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    flags = dav1d_get_cpu_flags_arm();
#elif ARCH_PPC64LE
    flags = dav1d_get_cpu_flags_ppc();
#elif ARCH_X86
    flags = dav1d_get_cpu_flags_x86();
#endif
#endif
}

COLD unsigned dav1d_get_cpu_flags(void) {
    return flags & flags_mask;
}

COLD void dav1d_set_cpu_flags_mask(const unsigned mask) {
    flags_mask = mask;
}
