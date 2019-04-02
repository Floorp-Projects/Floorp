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

#include "src/x86/cpu.h"

void dav1d_cpu_cpuid(uint32_t *info, int leaf);
uint64_t dav1d_cpu_xgetbv(int xcr);

unsigned dav1d_get_cpu_flags_x86(void) {
    uint32_t info[4] = {0}, n_ids;
    unsigned flags = 0;

    dav1d_cpu_cpuid(info, 0);
    n_ids = info[0];

    if (n_ids >= 1) {
        dav1d_cpu_cpuid(info, 1);
        if (info[3] & (1 << 25)) flags |= DAV1D_X86_CPU_FLAG_SSE;
        if (info[3] & (1 << 26)) flags |= DAV1D_X86_CPU_FLAG_SSE2;
        if (info[2] & (1 <<  0)) flags |= DAV1D_X86_CPU_FLAG_SSE3;
        if (info[2] & (1 <<  9)) flags |= DAV1D_X86_CPU_FLAG_SSSE3;
        if (info[2] & (1 << 19)) flags |= DAV1D_X86_CPU_FLAG_SSE41;
        if (info[2] & (1 << 20)) flags |= DAV1D_X86_CPU_FLAG_SSE42;
#if ARCH_X86_64
        /* We only support >128-bit SIMD on x86-64. */
        if (info[2] & (1 << 27)) /* OSXSAVE */ {
            uint64_t xcr = dav1d_cpu_xgetbv(0);
            if ((xcr & 0x00000006) == 0x00000006) /* XMM/YMM */ {
                if (info[2] & (1 << 28)) flags |= DAV1D_X86_CPU_FLAG_AVX;
                if (n_ids >= 7) {
                    dav1d_cpu_cpuid(info, 7);
                    if ((info[1] & 0x00000128) == 0x00000128)
                        flags |= DAV1D_X86_CPU_FLAG_AVX2;
                    if ((xcr & 0x000000e0) == 0x000000e0) /* ZMM/OPMASK */ {
                        if ((info[1] & 0xd0030000) == 0xd0030000)
                            flags |= DAV1D_X86_CPU_FLAG_AVX512;
                    }
                }
            }
        }
#endif
    }

    return flags;
}
