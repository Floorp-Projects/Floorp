/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Martin Storsjo
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

#include "src/arm/asm.S"
#include "util.S"

#define BUF_POS 0
#define BUF_END 8
#define DIF 16
#define RNG 24
#define CNT 28
#define ALLOW_UPDATE_CDF 32

const coeffs
        .short 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0
        .short 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0
endconst

const bits
        .short   0x1,   0x2,   0x4,   0x8,   0x10,   0x20,   0x40,   0x80
        .short 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
endconst

.macro ld1_n d0, d1, src, sz, n
.if \n <= 8
        ld1             {\d0\sz},  [\src]
.else
        ld1             {\d0\sz, \d1\sz},  [\src]
.endif
.endm

.macro st1_n s0, s1, dst, sz, n
.if \n <= 8
        st1             {\s0\sz},  [\dst]
.else
        st1             {\s0\sz, \s1\sz},  [\dst]
.endif
.endm

.macro ushr_n d0, d1, s0, s1, shift, sz, n
        ushr            \d0\sz,  \s0\sz,  \shift
.if \n == 16
        ushr            \d1\sz,  \s1\sz,  \shift
.endif
.endm

.macro add_n d0, d1, s0, s1, s2, s3, sz, n
        add             \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        add             \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro sub_n d0, d1, s0, s1, s2, s3, sz, n
        sub             \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        sub             \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro and_n d0, d1, s0, s1, s2, s3, sz, n
        and             \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        and             \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro cmhs_n d0, d1, s0, s1, s2, s3, sz, n
        cmhs            \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        cmhs            \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro urhadd_n d0, d1, s0, s1, s2, s3, sz, n
        urhadd          \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        urhadd          \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro sshl_n d0, d1, s0, s1, s2, s3, sz, n
        sshl            \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        sshl            \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro umull_n d0, d1, d2, d3, s0, s1, s2, s3, n
        umull           \d0\().4s, \s0\().4h,  \s2\().4h
.if \n >= 8
        umull2          \d1\().4s, \s0\().8h,  \s2\().8h
.endif
.if \n == 16
        umull           \d2\().4s, \s1\().4h,  \s3\().4h
        umull2          \d3\().4s, \s1\().8h,  \s3\().8h
.endif
.endm

.macro shrn_n d0, d1, s0, s1, s2, s3, shift, n
        shrn            \d0\().4h,  \s0\().4s, \shift
.if \n >= 8
        shrn2           \d0\().8h,  \s1\().4s, \shift
.endif
.if \n == 16
        shrn            \d1\().4h,  \s2\().4s, \shift
        shrn2           \d1\().8h,  \s3\().4s, \shift
.endif
.endm

.macro str_n            idx0, idx1, dstreg, dstoff, n
        str             q\idx0,  [\dstreg, \dstoff]
.if \n == 16
        str             q\idx1,  [\dstreg, \dstoff + 16]
.endif
.endm

// unsigned dav1d_msac_decode_symbol_adapt4_neon(MsacContext *s, uint16_t *cdf,
//                                               size_t n_symbols);

function msac_decode_symbol_adapt4_neon, export=1
.macro decode_update sz, szb, n
        sub             sp,  sp,  #48
        add             x8,  x0,  #RNG
        ld1_n           v0,  v1,  x1,  \sz, \n                    // cdf
        ld1r            {v4\sz},  [x8]                            // rng
        movrel          x9,  coeffs, 32
        sub             x9,  x9,  x2, lsl #1
        ushr_n          v2,  v3,  v0,  v1,  #6, \sz, \n           // cdf >> EC_PROB_SHIFT
        str             h4,  [sp, #14]                            // store original u = s->rng
        ushr            v4\sz,  v4\sz,  #8                        // r = rng >> 8

        umull_n         v16, v17, v18, v19, v4,  v4,  v2,  v3, \n // r * (cdf >> EC_PROB_SHIFT)
        ld1_n           v4,  v5,  x9,  \sz, \n                    // EC_MIN_PROB * (n_symbols - ret)
        shrn_n          v2,  v3,  v16, v17, v18, v19, #1, \n      // v >>= 7 - EC_PROB_SHIFT
        add             x8,  x0,  #DIF + 6

        add_n           v4,  v5,  v2,  v3,  v4,  v5, \sz, \n      // v += EC_MIN_PROB * (n_symbols - ret)

        ld1r            {v6.8h},  [x8]                            // dif >> (EC_WIN_SIZE - 16)
        movrel          x8,  bits
        str_n           4,   5,  sp, #16, \n                      // store v values to allow indexed access

        ld1_n           v16, v17, x8,  .8h, \n

        cmhs_n          v2,  v3,  v6,  v6,  v4,  v5,  .8h,  \n    // c >= v

        and_n           v6,  v7,  v2,  v3,  v16, v17, .16b, \n    // One bit per halfword set in the mask
.if \n == 16
        add             v6.8h,  v6.8h,  v7.8h
.endif
        addv            h6,  v6.8h                                // Aggregate mask bits
        ldr             w4,  [x0, #ALLOW_UPDATE_CDF]
        umov            w3,  v6.h[0]
        rbit            w3,  w3
        clz             w15, w3                                   // ret

        cbz             w4,  L(renorm)
        // update_cdf
        ldrh            w3,  [x1, x2, lsl #1]                     // count = cdf[n_symbols]
        movi            v5\szb, #0xff
        cmp             x2,  #4                                   // set C if n_symbols >= 4 (n_symbols > 3)
        mov             w14, #4
        lsr             w4,  w3,  #4                              // count >> 4
        urhadd_n        v4,  v5,  v5,  v5,  v2,  v3,  \sz, \n     // i >= val ? -1 : 32768
        adc             w4,  w4,  w14                             // (count >> 4) + (n_symbols > 3) + 4
        neg             w4,  w4                                   // -rate
        sub_n           v4,  v5,  v4,  v5,  v0,  v1,  \sz, \n     // (32768 - cdf[i]) or (-1 - cdf[i])
        dup             v6.8h,    w4                              // -rate

        sub             w3,  w3,  w3, lsr #5                      // count + (count >= 32)
        sub_n           v0,  v1,  v0,  v1,  v2,  v3,  \sz, \n     // cdf + (i >= val ? 1 : 0)
        sshl_n          v4,  v5,  v4,  v5,  v6,  v6,  \sz, \n     // ({32768,-1} - cdf[i]) >> rate
        add             w3,  w3,  #1                              // count + (count < 32)
        add_n           v0,  v1,  v0,  v1,  v4,  v5,  \sz, \n     // cdf + (32768 - cdf[i]) >> rate
        st1_n           v0,  v1,  x1,  \sz, \n
        strh            w3,  [x1, x2, lsl #1]
.endm

        decode_update   .4h, .8b, 4

L(renorm):
        add             x8,  sp,  #16
        add             x8,  x8,  w15, uxtw #1
        ldrh            w3,  [x8]              // v
        ldurh           w4,  [x8, #-2]         // u
        ldr             w6,  [x0, #CNT]
        ldr             x7,  [x0, #DIF]
        sub             w4,  w4,  w3           // rng = u - v
        clz             w5,  w4                // clz(rng)
        eor             w5,  w5,  #16          // d = clz(rng) ^ 16
        mvn             x7,  x7                // ~dif
        add             x7,  x7,  x3, lsl #48  // ~dif + (v << 48)
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (~dif + (v << 48)) << d
        str             w4,  [x0, #RNG]
        mvn             x7,  x7                // ~dif
        b.ge            9f

        // refill
        ldr             x3,  [x0, #BUF_POS]
        ldr             x4,  [x0, #BUF_END]
        add             x5,  x3,  #8
        cmp             x5,  x4
        b.gt            2f

        ldr             x3,  [x3]              // next_bits
        add             w8,  w6,  #23          // shift_bits = cnt + 23
        add             w6,  w6,  #16          // cnt += 16
        rev             x3,  x3                // next_bits = bswap(next_bits)
        sub             x5,  x5,  x8, lsr #3   // buf_pos -= shift_bits >> 3
        and             w8,  w8,  #24          // shift_bits &= 24
        lsr             x3,  x3,  x8           // next_bits >>= shift_bits
        sub             w8,  w8,  w6           // shift_bits -= 16 + cnt
        str             x5,  [x0, #BUF_POS]
        lsl             x3,  x3,  x8           // next_bits <<= shift_bits
        mov             w4,  #48
        sub             w6,  w4,  w8           // cnt = cnt + 64 - shift_bits
        eor             x7,  x7,  x3           // dif ^= next_bits
        b               9f

2:      // refill_eob
        mov             w14, #40
        sub             w5,  w14, w6           // c = 40 - cnt
3:
        cmp             x3,  x4
        b.ge            4f
        ldrb            w8,  [x3], #1
        lsl             x8,  x8,  x5
        eor             x7,  x7,  x8
        subs            w5,  w5,  #8
        b.ge            3b

4:      // refill_eob_end
        str             x3,  [x0, #BUF_POS]
        sub             w6,  w14, w5           // cnt = 40 - c

9:
        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]

        mov             w0,  w15
        add             sp,  sp,  #48
        ret
endfunc

function msac_decode_symbol_adapt8_neon, export=1
        decode_update   .8h, .16b, 8
        b               L(renorm)
endfunc

function msac_decode_symbol_adapt16_neon, export=1
        decode_update   .8h, .16b, 16
        b               L(renorm)
endfunc
