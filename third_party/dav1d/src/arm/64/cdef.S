/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

.macro pad_top_bottom s1, s2, w, stride, rn, rw, ret
        tst             w6,  #1 // CDEF_HAVE_LEFT
        b.eq            2f
        // CDEF_HAVE_LEFT
        sub             \s1,  \s1,  #2
        sub             \s2,  \s2,  #2
        tst             w6,  #2 // CDEF_HAVE_RIGHT
        b.eq            1f
        // CDEF_HAVE_LEFT+CDEF_HAVE_RIGHT
        ldr             \rn\()0, [\s1]
        ldr             s1,      [\s1, #\w]
        ldr             \rn\()2, [\s2]
        ldr             s3,      [\s2, #\w]
        uxtl            v0.8h,   v0.8b
        uxtl            v1.8h,   v1.8b
        uxtl            v2.8h,   v2.8b
        uxtl            v3.8h,   v3.8b
        str             \rw\()0, [x0]
        str             d1,      [x0, #2*\w]
        add             x0,  x0,  #2*\stride
        str             \rw\()2, [x0]
        str             d3,      [x0, #2*\w]
.if \ret
        ret
.else
        add             x0,  x0,  #2*\stride
        b               3f
.endif

1:
        // CDEF_HAVE_LEFT+!CDEF_HAVE_RIGHT
        ldr             \rn\()0, [\s1]
        ldr             h1,      [\s1, #\w]
        ldr             \rn\()2, [\s2]
        ldr             h3,      [\s2, #\w]
        uxtl            v0.8h,   v0.8b
        uxtl            v1.8h,   v1.8b
        uxtl            v2.8h,   v2.8b
        uxtl            v3.8h,   v3.8b
        str             \rw\()0, [x0]
        str             s1,      [x0, #2*\w]
        str             s31,     [x0, #2*\w+4]
        add             x0,  x0,  #2*\stride
        str             \rw\()2, [x0]
        str             s3,      [x0, #2*\w]
        str             s31,     [x0, #2*\w+4]
.if \ret
        ret
.else
        add             x0,  x0,  #2*\stride
        b               3f
.endif

2:
        // !CDEF_HAVE_LEFT
        tst             w6,  #2 // CDEF_HAVE_RIGHT
        b.eq            1f
        // !CDEF_HAVE_LEFT+CDEF_HAVE_RIGHT
        ldr             \rn\()0, [\s1]
        ldr             h1,      [\s1, #\w]
        ldr             \rn\()2, [\s2]
        ldr             h3,      [\s2, #\w]
        uxtl            v0.8h,  v0.8b
        uxtl            v1.8h,  v1.8b
        uxtl            v2.8h,  v2.8b
        uxtl            v3.8h,  v3.8b
        str             s31, [x0]
        stur            \rw\()0, [x0, #4]
        str             s1,      [x0, #4+2*\w]
        add             x0,  x0,  #2*\stride
        str             s31, [x0]
        stur            \rw\()2, [x0, #4]
        str             s3,      [x0, #4+2*\w]
.if \ret
        ret
.else
        add             x0,  x0,  #2*\stride
        b               3f
.endif

1:
        // !CDEF_HAVE_LEFT+!CDEF_HAVE_RIGHT
        ldr             \rn\()0, [\s1]
        ldr             \rn\()1, [\s2]
        uxtl            v0.8h,  v0.8b
        uxtl            v1.8h,  v1.8b
        str             s31,     [x0]
        stur            \rw\()0, [x0, #4]
        str             s31,     [x0, #4+2*\w]
        add             x0,  x0,  #2*\stride
        str             s31,     [x0]
        stur            \rw\()1, [x0, #4]
        str             s31,     [x0, #4+2*\w]
.if \ret
        ret
.else
        add             x0,  x0,  #2*\stride
.endif
3:
.endm

// void dav1d_cdef_paddingX_neon(uint16_t *tmp, const pixel *src,
//                               ptrdiff_t src_stride, const pixel (*left)[2],
//                               /*const*/ pixel *const top[2], int h,
//                               enum CdefEdgeFlags edges);

.macro padding_func w, stride, rn, rw
function cdef_padding\w\()_neon, export=1
        movi            v30.8h,  #0x80, lsl #8
        mov             v31.16b, v30.16b
        sub             x0,  x0,  #2*(2*\stride+2)
        tst             w6,  #4 // CDEF_HAVE_TOP
        b.ne            1f
        // !CDEF_HAVE_TOP
        st1             {v30.8h, v31.8h}, [x0], #32
.if \w == 8
        st1             {v30.8h, v31.8h}, [x0], #32
.endif
        b               3f
1:
        // CDEF_HAVE_TOP
        ldr             x8,  [x4]
        ldr             x9,  [x4, #8]
        pad_top_bottom  x8,  x9, \w, \stride, \rn, \rw, 0

        // Middle section
3:
        tst             w6,  #1 // CDEF_HAVE_LEFT
        b.eq            2f
        // CDEF_HAVE_LEFT
        tst             w6,  #2 // CDEF_HAVE_RIGHT
        b.eq            1f
        // CDEF_HAVE_LEFT+CDEF_HAVE_RIGHT
0:
        ld1             {v0.h}[0], [x3], #2
        ldr             \rn\()1, [x1]
        ldr             h2,      [x1, #\w]
        add             x1,  x1,  x2
        subs            w5,  w5,  #1
        uxtl            v0.8h,  v0.8b
        uxtl            v1.8h,  v1.8b
        uxtl            v2.8h,  v2.8b
        str             s0,      [x0]
        stur            \rw\()1, [x0, #4]
        str             s2,      [x0, #4+2*\w]
        add             x0,  x0,  #2*\stride
        b.gt            0b
        b               3f
1:
        // CDEF_HAVE_LEFT+!CDEF_HAVE_RIGHT
        ld1             {v0.h}[0], [x3], #2
.if \w == 8
        ld1             {v1.8b},   [x1], x2
.else
        ld1             {v1.s}[0], [x1], x2
.endif
        subs            w5,  w5,  #1
        uxtl            v0.8h,  v0.8b
        uxtl            v1.8h,  v1.8b
        str             s0,      [x0]
        stur            \rw\()1, [x0, #4]
        str             s31,     [x0, #4+2*\w]
        add             x0,  x0,  #2*\stride
        b.gt            1b
        b               3f
2:
        tst             w6,  #2 // CDEF_HAVE_RIGHT
        b.eq            1f
        // !CDEF_HAVE_LEFT+CDEF_HAVE_RIGHT
0:
        ldr             \rn\()0, [x1]
        ldr             h1,      [x1, #\w]
        add             x1,  x1,  x2
        subs            w5,  w5,  #1
        uxtl            v0.8h,  v0.8b
        uxtl            v1.8h,  v1.8b
        str             s31,     [x0]
        stur            \rw\()0, [x0, #4]
        str             s1,      [x0, #4+2*\w]
        add             x0,  x0,  #2*\stride
        b.gt            0b
        b               3f
1:
        // !CDEF_HAVE_LEFT+!CDEF_HAVE_RIGHT
.if \w == 8
        ld1             {v0.8b},   [x1], x2
.else
        ld1             {v0.s}[0], [x1], x2
.endif
        subs            w5,  w5,  #1
        uxtl            v0.8h,  v0.8b
        str             s31,     [x0]
        stur            \rw\()0, [x0, #4]
        str             s31,     [x0, #4+2*\w]
        add             x0,  x0,  #2*\stride
        b.gt            1b

3:
        tst             w6,  #8 // CDEF_HAVE_BOTTOM
        b.ne            1f
        // !CDEF_HAVE_BOTTOM
        st1             {v30.8h, v31.8h}, [x0], #32
.if \w == 8
        st1             {v30.8h, v31.8h}, [x0], #32
.endif
        ret
1:
        // CDEF_HAVE_BOTTOM
        add             x9,  x1,  x2
        pad_top_bottom  x1,  x9, \w, \stride, \rn, \rw, 1
endfunc
.endm

padding_func 8, 16, d, q
padding_func 4, 8,  s, d

.macro dir_table w, stride
const directions\w
        .byte           -1 * \stride + 1, -2 * \stride + 2
        .byte            0 * \stride + 1, -1 * \stride + 2
        .byte            0 * \stride + 1,  0 * \stride + 2
        .byte            0 * \stride + 1,  1 * \stride + 2
        .byte            1 * \stride + 1,  2 * \stride + 2
        .byte            1 * \stride + 0,  2 * \stride + 1
        .byte            1 * \stride + 0,  2 * \stride + 0
        .byte            1 * \stride + 0,  2 * \stride - 1
// Repeated, to avoid & 7
        .byte           -1 * \stride + 1, -2 * \stride + 2
        .byte            0 * \stride + 1, -1 * \stride + 2
        .byte            0 * \stride + 1,  0 * \stride + 2
        .byte            0 * \stride + 1,  1 * \stride + 2
        .byte            1 * \stride + 1,  2 * \stride + 2
        .byte            1 * \stride + 0,  2 * \stride + 1
endconst
.endm

dir_table 8, 16
dir_table 4, 8

const pri_taps
        .byte           4, 2, 3, 3
endconst

.macro load_px d1, d2, w
.if \w == 8
        add             x6,  x2,  w9, sxtb #1       // x + off
        sub             x9,  x2,  w9, sxtb #1       // x - off
        ld1             {\d1\().8h}, [x6]           // p0
        ld1             {\d2\().8h}, [x9]           // p1
.else
        add             x6,  x2,  w9, sxtb #1       // x + off
        sub             x9,  x2,  w9, sxtb #1       // x - off
        ld1             {\d1\().4h}, [x6]           // p0
        add             x6,  x6,  #2*8              // += stride
        ld1             {\d2\().4h}, [x9]           // p1
        add             x9,  x9,  #2*8              // += stride
        ld1             {\d1\().d}[1], [x6]         // p0
        ld1             {\d2\().d}[1], [x9]         // p1
.endif
.endm
.macro handle_pixel s1, s2, threshold, thresh_vec, shift, tap
        umin            v2.8h,   v2.8h,  \s1\().8h
        smax            v3.8h,   v3.8h,  \s1\().8h
        umin            v2.8h,   v2.8h,  \s2\().8h
        smax            v3.8h,   v3.8h,  \s2\().8h

        cbz             \threshold, 3f
        uabd            v16.8h, v0.8h,  \s1\().8h   // abs(diff)
        uabd            v20.8h, v0.8h,  \s2\().8h   // abs(diff)
        ushl            v17.8h, v16.8h, \shift      // abs(diff) >> shift
        ushl            v21.8h, v20.8h, \shift      // abs(diff) >> shift
        uqsub           v17.8h, \thresh_vec, v17.8h // imax(0, threshold - (abs(diff) >> shift))
        uqsub           v21.8h, \thresh_vec, v21.8h // imax(0, threshold - (abs(diff) >> shift))
        cmhi            v18.8h, v0.8h,  \s1\().8h   // px > p0
        cmhi            v22.8h, v0.8h,  \s2\().8h   // px > p1
        umin            v17.8h, v17.8h, v16.8h      // imin(abs(diff), imax())
        umin            v21.8h, v21.8h, v20.8h      // imin(abs(diff), imax())
        dup             v19.8h, \tap                // taps[k]
        neg             v16.8h, v17.8h              // -imin()
        neg             v20.8h, v21.8h              // -imin()
        bsl             v18.16b, v16.16b, v17.16b   // constrain() = apply_sign()
        bsl             v22.16b, v20.16b, v21.16b   // constrain() = apply_sign()
        mla             v1.8h,  v18.8h, v19.8h      // sum += taps[k] * constrain()
        mla             v1.8h,  v22.8h, v19.8h      // sum += taps[k] * constrain()
3:
.endm

// void dav1d_cdef_filterX_neon(pixel *dst, ptrdiff_t dst_stride,
//                              const uint16_t *tmp, int pri_strength,
//                              int sec_strength, int dir, int damping, int h);
.macro filter w
function cdef_filter\w\()_neon, export=1
        movrel          x8,  pri_taps
        and             w9,  w3,  #1
        add             x8,  x8,  w9, uxtw #1
        movrel          x9,  directions\w
        add             x5,  x9,  w5, uxtw #1
        movi            v30.8h,   #15
        dup             v28.8h,   w6                // damping

        dup             v25.8h, w3                  // threshold
        dup             v27.8h, w4                  // threshold
        clz             v24.8h, v25.8h              // clz(threshold)
        clz             v26.8h, v27.8h              // clz(threshold)
        sub             v24.8h, v30.8h, v24.8h      // ulog2(threshold)
        sub             v26.8h, v30.8h, v26.8h      // ulog2(threshold)
        uqsub           v24.8h, v28.8h, v24.8h      // shift = imax(0, damping - ulog2(threshold))
        uqsub           v26.8h, v28.8h, v26.8h      // shift = imax(0, damping - ulog2(threshold))
        neg             v24.8h, v24.8h              // -shift
        neg             v26.8h, v26.8h              // -shift

1:
.if \w == 8
        ld1             {v0.8h}, [x2]               // px
.else
        add             x12, x2,  #2*8
        ld1             {v0.4h},   [x2]             // px
        ld1             {v0.d}[1], [x12]            // px
.endif

        movi            v1.8h,  #0                  // sum
        mov             v2.16b, v0.16b              // min
        mov             v3.16b, v0.16b              // max

        // Instead of loading sec_taps 2, 1 from memory, just set it
        // to 2 initially and decrease for the second round.
        mov             w11, #2                     // sec_taps[0]

2:
        ldrb            w9,  [x5]                   // off1

        load_px         v4,  v5, \w

        add             x5,  x5,  #4                // +2*2
        ldrb            w9,  [x5]                   // off2
        load_px         v6,  v7,  \w

        ldrb            w10, [x8]                   // *pri_taps

        handle_pixel    v4,  v5,  w3,  v25.8h, v24.8h, w10

        add             x5,  x5,  #8                // +2*4
        ldrb            w9,  [x5]                   // off3
        load_px         v4,  v5,  \w

        handle_pixel    v6,  v7,  w4,  v27.8h, v26.8h, w11

        handle_pixel    v4,  v5,  w4,  v27.8h, v26.8h, w11

        sub             x5,  x5,  #11               // x8 -= 2*(2+4); x8 += 1;
        subs            w11, w11, #1                // sec_tap-- (value)
        add             x8,  x8,  #1                // pri_taps++ (pointer)
        b.ne            2b

        sshr            v4.8h,  v1.8h,  #15         // -(sum < 0)
        add             v1.8h,  v1.8h,  v4.8h       // sum - (sum < 0)
        srshr           v1.8h,  v1.8h,  #4          // (8 + sum - (sum < 0)) >> 4
        add             v0.8h,  v0.8h,  v1.8h       // px + (8 + sum ...) >> 4
        smin            v0.8h,  v0.8h,  v3.8h
        smax            v0.8h,  v0.8h,  v2.8h       // iclip(px + .., min, max)
        xtn             v0.8b,  v0.8h
.if \w == 8
        add             x2,  x2,  #2*16             // tmp += tmp_stride
        subs            w7,  w7,  #1                // h--
        st1             {v0.8b}, [x0], x1
.else
        st1             {v0.s}[0], [x0], x1
        add             x2,  x2,  #2*16             // tmp += 2*tmp_stride
        subs            w7,  w7,  #2                // h -= 2
        st1             {v0.s}[1], [x0], x1
.endif

        // Reset pri_taps/sec_taps back to the original point
        sub             x5,  x5,  #2
        sub             x8,  x8,  #2

        b.gt            1b
        ret
endfunc
.endm

filter 8
filter 4

const div_table
        .short         840, 420, 280, 210, 168, 140, 120, 105
endconst

const alt_fact
        .short         420, 210, 140, 105, 105, 105, 105, 105, 140, 210, 420, 0
endconst

// int dav1d_cdef_find_dir_neon(const pixel *img, const ptrdiff_t stride,
//                              unsigned *const var)
function cdef_find_dir_neon, export=1
        sub             sp,  sp,  #32 // cost
        mov             w3,  #8
        movi            v31.16b, #128
        movi            v30.16b, #0
        movi            v1.8h,   #0 // v0-v1 sum_diag[0]
        movi            v3.8h,   #0 // v2-v3 sum_diag[1]
        movi            v5.8h,   #0 // v4-v5 sum_hv[0-1]
        movi            v7.8h,   #0 // v6-v7 sum_alt[0]
        movi            v17.8h,  #0 // v16-v17 sum_alt[1]
        movi            v18.8h,  #0 // v18-v19 sum_alt[2]
        movi            v19.8h,  #0
        movi            v21.8h,  #0 // v20-v21 sum_alt[3]

.irpc i, 01234567
        ld1             {v26.8b}, [x0], x1
        usubl           v26.8h,  v26.8b, v31.8b

        addv            h25,     v26.8h               // [y]
        rev64           v27.8h,  v26.8h
        addp            v28.8h,  v26.8h,  v30.8h      // [(x >> 1)]
        add             v5.8h,   v5.8h,   v26.8h      // sum_hv[1]
        ext             v27.16b, v27.16b, v27.16b, #8 // [-x]
        rev64           v29.4h,  v28.4h               // [-(x >> 1)]
        ins             v4.h[\i], v25.h[0]            // sum_hv[0]

.if \i == 0
        mov             v0.16b,  v26.16b              // sum_diag[0]
        mov             v2.16b,  v27.16b              // sum_diag[1]
        mov             v6.16b,  v28.16b              // sum_alt[0]
        mov             v16.16b, v29.16b              // sum_alt[1]
.else
        ext             v22.16b, v30.16b, v26.16b, #(16-2*\i)
        ext             v23.16b, v26.16b, v30.16b, #(16-2*\i)
        ext             v24.16b, v30.16b, v27.16b, #(16-2*\i)
        ext             v25.16b, v27.16b, v30.16b, #(16-2*\i)
        add             v0.8h,   v0.8h,   v22.8h      // sum_diag[0]
        add             v1.8h,   v1.8h,   v23.8h      // sum_diag[0]
        add             v2.8h,   v2.8h,   v24.8h      // sum_diag[1]
        add             v3.8h,   v3.8h,   v25.8h      // sum_diag[1]
        ext             v22.16b, v30.16b, v28.16b, #(16-2*\i)
        ext             v23.16b, v28.16b, v30.16b, #(16-2*\i)
        ext             v24.16b, v30.16b, v29.16b, #(16-2*\i)
        ext             v25.16b, v29.16b, v30.16b, #(16-2*\i)
        add             v6.8h,   v6.8h,   v22.8h      // sum_alt[0]
        add             v7.8h,   v7.8h,   v23.8h      // sum_alt[0]
        add             v16.8h,  v16.8h,  v24.8h      // sum_alt[1]
        add             v17.8h,  v17.8h,  v25.8h      // sum_alt[1]
.endif
.if \i < 6
        ext             v22.16b, v30.16b, v26.16b, #(16-2*(3-(\i/2)))
        ext             v23.16b, v26.16b, v30.16b, #(16-2*(3-(\i/2)))
        add             v18.8h,  v18.8h,  v22.8h      // sum_alt[2]
        add             v19.8h,  v19.8h,  v23.8h      // sum_alt[2]
.else
        add             v18.8h,  v18.8h,  v26.8h      // sum_alt[2]
.endif
.if \i == 0
        mov             v20.16b, v26.16b              // sum_alt[3]
.elseif \i == 1
        add             v20.8h,  v20.8h,  v26.8h      // sum_alt[3]
.else
        ext             v24.16b, v30.16b, v26.16b, #(16-2*(\i/2))
        ext             v25.16b, v26.16b, v30.16b, #(16-2*(\i/2))
        add             v20.8h,  v20.8h,  v24.8h      // sum_alt[3]
        add             v21.8h,  v21.8h,  v25.8h      // sum_alt[3]
.endif
.endr

        movi            v31.4s,  #105

        smull           v26.4s,  v4.4h,   v4.4h       // sum_hv[0]*sum_hv[0]
        smlal2          v26.4s,  v4.8h,   v4.8h
        smull           v27.4s,  v5.4h,   v5.4h       // sum_hv[1]*sum_hv[1]
        smlal2          v27.4s,  v5.8h,   v5.8h
        mul             v26.4s,  v26.4s,  v31.4s      // cost[2] *= 105
        mul             v27.4s,  v27.4s,  v31.4s      // cost[6] *= 105
        addv            s4,  v26.4s                   // cost[2]
        addv            s5,  v27.4s                   // cost[6]

        rev64           v1.8h,   v1.8h
        rev64           v3.8h,   v3.8h
        ext             v1.16b,  v1.16b,  v1.16b, #8  // sum_diag[0][15-n]
        ext             v3.16b,  v3.16b,  v3.16b, #8  // sum_diag[1][15-n]
        ext             v1.16b,  v1.16b,  v1.16b, #2  // sum_diag[0][14-n]
        ext             v3.16b,  v3.16b,  v3.16b, #2  // sum_diag[1][14-n]

        str             s4,  [sp, #2*4]               // cost[2]
        str             s5,  [sp, #6*4]               // cost[6]

        movrel          x4,  div_table
        ld1             {v31.8h}, [x4]

        smull           v22.4s,  v0.4h,   v0.4h       // sum_diag[0]*sum_diag[0]
        smull2          v23.4s,  v0.8h,   v0.8h
        smlal           v22.4s,  v1.4h,   v1.4h
        smlal2          v23.4s,  v1.8h,   v1.8h
        smull           v24.4s,  v2.4h,   v2.4h       // sum_diag[1]*sum_diag[1]
        smull2          v25.4s,  v2.8h,   v2.8h
        smlal           v24.4s,  v3.4h,   v3.4h
        smlal2          v25.4s,  v3.8h,   v3.8h
        uxtl            v30.4s,  v31.4h               // div_table
        uxtl2           v31.4s,  v31.8h
        mul             v22.4s,  v22.4s,  v30.4s      // cost[0]
        mla             v22.4s,  v23.4s,  v31.4s      // cost[0]
        mul             v24.4s,  v24.4s,  v30.4s      // cost[4]
        mla             v24.4s,  v25.4s,  v31.4s      // cost[4]
        addv            s0,  v22.4s                   // cost[0]
        addv            s2,  v24.4s                   // cost[4]

        movrel          x5,  alt_fact
        ld1             {v29.4h, v30.4h, v31.4h}, [x5]// div_table[2*m+1] + 105

        str             s0,  [sp, #0*4]               // cost[0]
        str             s2,  [sp, #4*4]               // cost[4]

        uxtl            v29.4s,  v29.4h               // div_table[2*m+1] + 105
        uxtl            v30.4s,  v30.4h
        uxtl            v31.4s,  v31.4h

.macro cost_alt d1, d2, s1, s2, s3, s4
        smull           v22.4s,  \s1\().4h, \s1\().4h // sum_alt[n]*sum_alt[n]
        smull2          v23.4s,  \s1\().8h, \s1\().8h
        smull           v24.4s,  \s2\().4h, \s2\().4h
        smull           v25.4s,  \s3\().4h, \s3\().4h // sum_alt[n]*sum_alt[n]
        smull2          v26.4s,  \s3\().8h, \s3\().8h
        smull           v27.4s,  \s4\().4h, \s4\().4h
        mul             v22.4s,  v22.4s,  v29.4s      // sum_alt[n]^2*fact
        mla             v22.4s,  v23.4s,  v30.4s
        mla             v22.4s,  v24.4s,  v31.4s
        mul             v25.4s,  v25.4s,  v29.4s      // sum_alt[n]^2*fact
        mla             v25.4s,  v26.4s,  v30.4s
        mla             v25.4s,  v27.4s,  v31.4s
        addv            \d1, v22.4s                   // *cost_ptr
        addv            \d2, v25.4s                   // *cost_ptr
.endm
        cost_alt        s6,  s16, v6,  v7,  v16, v17  // cost[1], cost[3]
        str             s6,  [sp, #1*4]               // cost[1]
        str             s16, [sp, #3*4]               // cost[3]
        cost_alt        s18, s20, v18, v19, v20, v21  // cost[5], cost[7]
        str             s18, [sp, #5*4]               // cost[5]
        str             s20, [sp, #7*4]               // cost[7]

        mov             w0,  #0                       // best_dir
        mov             w1,  v0.s[0]                  // best_cost
        mov             w3,  #1                       // n

        mov             w4,  v6.s[0]

.macro find_best s1, s2, s3
.ifnb \s2
        mov             w5,  \s2\().s[0]
.endif
        cmp             w4,  w1                       // cost[n] > best_cost
        csel            w0,  w3,  w0,  gt             // best_dir = n
        csel            w1,  w4,  w1,  gt             // best_cost = cost[n]
.ifnb \s2
        add             w3,  w3,  #1                  // n++
        cmp             w5,  w1                       // cost[n] > best_cost
        mov             w4,  \s3\().s[0]
        csel            w0,  w3,  w0,  gt             // best_dir = n
        csel            w1,  w5,  w1,  gt             // best_cost = cost[n]
        add             w3,  w3,  #1                  // n++
.endif
.endm
        find_best       v6,  v4, v16
        find_best       v16, v2, v18
        find_best       v18, v5, v20
        find_best       v20

        eor             w3,  w0,  #4                  // best_dir ^4
        ldr             w4,  [sp, w3, uxtw #2]
        sub             w1,  w1,  w4                  // best_cost - cost[best_dir ^ 4]
        lsr             w1,  w1,  #10
        str             w1,  [x2]                     // *var

        add             sp,  sp,  #32
        ret
endfunc
