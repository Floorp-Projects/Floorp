/*
 * Copyright © 2024, VideoLAN and dav1d authors
 * Copyright © 2024, Janne Grunau
 * Copyright © 2024, Martin Storsjo
 * Copyright © 2024, Arm Limited
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


#if HAVE_DOTPROD
ENABLE_DOTPROD

// No spaces in these expressions, due to gas-preprocessor. It is translated by
// -1 to save the negative offset at getting the address of `mc_subpel_filters`.
#define REGULAR1        (((0*15-1)<<7)|(3*15-1))
#define SMOOTH1         (((1*15-1)<<7)|(4*15-1))
#define SHARP1          (((2*15-1)<<7)|(3*15-1))

#define FUNC_ALIGN      2
#define JUMP_ALIGN      2
#define LOOP_ALIGN      2


// Lookup table used to help conversion of shifted 32-bit values to 8-bit.
        .align 4
L(hv_tbl_neon_dotprod):
        .byte  1,  2,  5,  6,   9, 10, 13, 14,  17, 18, 21, 22,  25, 26, 29, 30

// Shuffle indices to permute horizontal samples in preparation for input to
// SDOT instructions. The 8-tap horizontal convolution uses sample indices in the
// interval of [-3, 4] relative to the current sample position. We load samples
// from index value -4 to keep loads word aligned, so the shuffle bytes are
// translated by 1 to handle this.
        .align 4
L(h_tbl_neon_dotprod):
        .byte  1,  2,  3,  4,   2,  3,  4,  5,   3,  4,  5,  6,   4,  5,  6,  7
        .byte  5,  6,  7,  8,   6,  7,  8,  9,   7,  8,  9, 10,   8,  9, 10, 11
        .byte  9, 10, 11, 12,  10, 11, 12, 13,  11, 12, 13, 14,  12, 13, 14, 15
        .byte 13, 14, 15, 16,  14, 15, 16, 17,  15, 16, 17, 18,  16, 17, 18, 19

// Vertical convolutions are also using SDOT instructions, where a 128-bit
// register contains a transposed 4x4 matrix of values. Subsequent iterations of
// the vertical convolution can reuse the 3x4 sub-matrix from the previous loop
// iteration. These shuffle indices shift and merge this 4x4 matrix with the
// values of a new line.
        .align 4
L(v_tbl_neon_dotprod):
        .byte  1,  2,  3, 16,   5,  6,  7, 20,   9, 10, 11, 24,  13, 14, 15, 28
        .byte  1,  2,  3, 16,   5,  6,  7, 17,   9, 10, 11, 18,  13, 14, 15, 19
        .byte  1,  2,  3, 20,   5,  6,  7, 21,   9, 10, 11, 22,  13, 14, 15, 23
        .byte  1,  2,  3, 24,   5,  6,  7, 25,   9, 10, 11, 26,  13, 14, 15, 27
        .byte  1,  2,  3, 28,   5,  6,  7, 29,   9, 10, 11, 30,  13, 14, 15, 31


.macro make_8tap_fn op, type, type_h, type_v, isa, jump=1
function \op\()_8tap_\type\()_8bpc_\isa, export=1, align=FUNC_ALIGN
        mov             x9,  \type_h
        mov             x10, \type_v
    .if \jump
        b               \op\()_8tap_\isa
    .endif
endfunc
.endm

.macro filter_8tap_fn type, isa, dst, d_strd, src, s_strd, w, h, mx, my, xmx, xmy, ldst, lsrc, wd_strd
make_8tap_fn \type, sharp,          SHARP1,   SHARP1,   \isa
make_8tap_fn \type, sharp_smooth,   SHARP1,   SMOOTH1,  \isa
make_8tap_fn \type, sharp_regular,  SHARP1,   REGULAR1, \isa
make_8tap_fn \type, smooth_sharp,   SMOOTH1,  SHARP1,   \isa
make_8tap_fn \type, smooth,         SMOOTH1,  SMOOTH1,  \isa
make_8tap_fn \type, smooth_regular, SMOOTH1,  REGULAR1, \isa
make_8tap_fn \type, regular_sharp,  REGULAR1, SHARP1,   \isa
make_8tap_fn \type, regular_smooth, REGULAR1, SMOOTH1,  \isa
make_8tap_fn \type, regular,        REGULAR1, REGULAR1, \isa, jump=0

function \type\()_8tap_\isa, align=FUNC_ALIGN
        clz             w8, \w
        mov             w11,  #0x4081   // (1 << 14) | (1 << 7) | (1 << 0)
        sub             w8, w8, #24     // for jump tables
        movrel          x12, X(mc_subpel_filters)
        cbnz            \mx, L(\type\()_8tap_h_hv_\isa)
        cbnz            \my, L(\type\()_8tap_v_\isa)
.ifc \type, prep
        add             \wd_strd, \w, \w    // prep_neon needs w * 2 as stride
.endif
        b               X(\type\()_neon)

        .align JUMP_ALIGN
L(\type\()_8tap_v_\isa):
        madd            \my, \my, w11, w10
.ifc \type, prep
        mov             w8, 0x2002  // FILTER_WEIGHT * 128 + rounding
.endif
        sub             \src, \src, \s_strd
        ldr             q6, L(v_tbl_neon_dotprod)
.ifc \type, prep
        dup             v4.4s, w8
.endif
        ubfx            w11, \my, #7, #7
        and             \my, \my, #0x7F
        ldr             q28, L(v_tbl_neon_dotprod) + 16
        cmp             \h, #4
        csel            \my, \my, w11, le
        sub             \src, \src, \s_strd, lsl #1     // src - src_stride * 3
        ldr             q29, L(v_tbl_neon_dotprod) + 32
        add             \xmy, x12, \xmy, lsl #3         // subpel V filter address
        movi            v5.16b, #128
        ldr             d7, [\xmy]
        cmp             \w, #8
        b.eq            80f
        b.lt            40f

        // .align JUMP_ALIGN    // fallthrough
160:    // V - 16xN+
        ldr             q30, L(v_tbl_neon_dotprod) + 48
        ldr             q31, L(v_tbl_neon_dotprod) + 64
.ifc \type, prep
        add             \wd_strd, \w, \w
.endif
        .align LOOP_ALIGN
161:
        mov             \lsrc, \src
        mov             \ldst, \dst
        sub             w8, \h, #1

        ldr             q16, [\lsrc]
        ldr             q17, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1
        ldr             q18, [\lsrc]
        ldr             q19, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1

        zip1            v0.16b, v16.16b, v17.16b
        zip2            v1.16b, v16.16b, v17.16b
        zip1            v2.16b, v18.16b, v19.16b
        zip2            v3.16b, v18.16b, v19.16b

        ldr             q20, [\lsrc]
        ldr             q21, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1
        ldr             q22, [\lsrc]
        ldr             q23, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1

        zip1            v18.16b, v20.16b, v21.16b
        zip2            v21.16b, v20.16b, v21.16b
        zip1            v24.16b, v22.16b, v23.16b
        zip2            v27.16b, v22.16b, v23.16b

        zip1            v16.8h, v0.8h, v2.8h
        zip2            v19.8h, v0.8h, v2.8h
        zip1            v22.8h, v1.8h, v3.8h
        zip2            v25.8h, v1.8h, v3.8h

        zip1            v17.8h, v18.8h, v24.8h
        zip2            v20.8h, v18.8h, v24.8h
        zip1            v23.8h, v21.8h, v27.8h
        zip2            v26.8h, v21.8h, v27.8h

        sub             v16.16b, v16.16b, v5.16b
        sub             v19.16b, v19.16b, v5.16b
        sub             v22.16b, v22.16b, v5.16b
        sub             v25.16b, v25.16b, v5.16b

        sub             v17.16b, v17.16b, v5.16b
        sub             v20.16b, v20.16b, v5.16b
        sub             v23.16b, v23.16b, v5.16b
        sub             v26.16b, v26.16b, v5.16b

        .align LOOP_ALIGN
16:
        ldr             q27, [\lsrc]
        add             \lsrc, \lsrc, \s_strd
.ifc \type, prep
        mov             v0.16b, v4.16b
        mov             v1.16b, v4.16b
        mov             v2.16b, v4.16b
        mov             v3.16b, v4.16b
.else
        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8
        movi            v2.4s, #32, lsl 8
        movi            v3.4s, #32, lsl 8
.endif
        sub             v18.16b, v27.16b, v5.16b
        sub             v21.16b, v27.16b, v5.16b
        sub             v24.16b, v27.16b, v5.16b
        sub             v27.16b, v27.16b, v5.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v2.4s, v22.16b, v7.4b[0]
        sdot            v3.4s, v25.16b, v7.4b[0]

        tbl             v16.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v19.16b, {v19.16b, v20.16b}, v6.16b
        tbl             v22.16b, {v22.16b, v23.16b}, v6.16b
        tbl             v25.16b, {v25.16b, v26.16b}, v6.16b

        sdot            v0.4s, v17.16b, v7.4b[1]
        sdot            v1.4s, v20.16b, v7.4b[1]
        sdot            v2.4s, v23.16b, v7.4b[1]
        sdot            v3.4s, v26.16b, v7.4b[1]

        tbl             v17.16b, {v17.16b, v18.16b}, v28.16b
        tbl             v20.16b, {v20.16b, v21.16b}, v29.16b
        tbl             v23.16b, {v23.16b, v24.16b}, v30.16b
        tbl             v26.16b, {v26.16b, v27.16b}, v31.16b

        subs            w8, w8, #1
        uzp1            v0.8h, v0.8h, v1.8h
        uzp1            v2.8h, v2.8h, v3.8h
.ifc \type, prep
        sshr            v0.8h, v0.8h, #2
        sshr            v1.8h, v2.8h, #2
        st1             {v0.8h, v1.8h}, [\ldst], \d_strd
.else
        sqrshrun        v0.8b, v0.8h, #6
        sqrshrun2       v0.16b, v2.8h, #6
        st1             {v0.16b}, [\ldst], \d_strd
.endif
        b.gt            16b

.ifc \type, prep
        mov             v0.16b, v4.16b
        mov             v1.16b, v4.16b
        mov             v2.16b, v4.16b
        mov             v3.16b, v4.16b
.else
        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8
        movi            v2.4s, #32, lsl 8
        movi            v3.4s, #32, lsl 8
.endif
        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v2.4s, v22.16b, v7.4b[0]
        sdot            v3.4s, v25.16b, v7.4b[0]

        sdot            v0.4s, v17.16b, v7.4b[1]
        sdot            v1.4s, v20.16b, v7.4b[1]
        sdot            v2.4s, v23.16b, v7.4b[1]
        sdot            v3.4s, v26.16b, v7.4b[1]

        subs            \w, \w, #16
        uzp1            v0.8h, v0.8h, v1.8h
        uzp1            v2.8h, v2.8h, v3.8h
.ifc \type, prep
        sshr            v0.8h, v0.8h, #2
        sshr            v1.8h, v2.8h, #2
        stp             q0, q1, [\ldst]
        add             \dst, \dst, #32
.else
        sqrshrun        v0.8b, v0.8h, #6
        sqrshrun2       v0.16b, v2.8h, #6
        str             q0, [\ldst]
        add             \dst, \dst, #16
.endif
        add             \src, \src, #16
        b.gt            161b
        ret

        .align JUMP_ALIGN
80:     // V - 8xN
        ldr             d16, [\src]
        ldr             d17, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             d18, [\src]
        ldr             d19, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        ldr             d20, [\src]
        ldr             d21, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             d22, [\src]
        ldr             d23, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        subs            \h, \h, #2  // for prep: sub is enough

        zip1            v0.16b, v16.16b, v17.16b
        zip1            v2.16b, v18.16b, v19.16b
        zip1            v18.16b, v20.16b, v21.16b
        zip1            v24.16b, v22.16b, v23.16b

        zip1            v16.8h,  v0.8h,  v2.8h
        zip2            v19.8h,  v0.8h,  v2.8h
        zip1            v17.8h, v18.8h, v24.8h
        zip2            v20.8h, v18.8h, v24.8h

        sub             v16.16b, v16.16b, v5.16b
        sub             v19.16b, v19.16b, v5.16b
        sub             v17.16b, v17.16b, v5.16b
        sub             v20.16b, v20.16b, v5.16b
.ifc \type, put
        b.eq            82f
.endif

        .align LOOP_ALIGN
8:
        ldr             d21, [\src]
        ldr             d27, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
.ifc \type, prep
        mov             v0.16b, v4.16b
        mov             v1.16b, v4.16b
        mov             v2.16b, v4.16b
        mov             v3.16b, v4.16b
.else
        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8
        movi            v2.4s, #32, lsl 8
        movi            v3.4s, #32, lsl 8
.endif
        sub             v18.16b, v21.16b, v5.16b
        sub             v21.16b, v21.16b, v5.16b
        sub             v24.16b, v27.16b, v5.16b
        sub             v27.16b, v27.16b, v5.16b

        tbl             v22.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v25.16b, {v19.16b, v20.16b}, v6.16b
        tbl             v23.16b, {v17.16b, v18.16b}, v28.16b
        tbl             v26.16b, {v20.16b, v21.16b}, v29.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v0.4s, v17.16b, v7.4b[1]
        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v1.4s, v20.16b, v7.4b[1]

        tbl             v16.16b, {v22.16b, v23.16b}, v6.16b
        tbl             v19.16b, {v25.16b, v26.16b}, v6.16b
        tbl             v17.16b, {v23.16b, v24.16b}, v28.16b
        tbl             v20.16b, {v26.16b, v27.16b}, v29.16b

        sdot            v2.4s, v22.16b, v7.4b[0]
        sdot            v2.4s, v23.16b, v7.4b[1]
        sdot            v3.4s, v25.16b, v7.4b[0]
        sdot            v3.4s, v26.16b, v7.4b[1]

        subs            \h, \h, #2
        uzp1            v0.8h, v0.8h, v1.8h
        uzp1            v2.8h, v2.8h, v3.8h
.ifc \type, prep
        sshr            v0.8h, v0.8h, #2
        sshr            v1.8h, v2.8h, #2
        stp             q0, q1, [\dst], #32
.else
        sqrshrun        v0.8b, v0.8h, #6
        sqrshrun        v1.8b, v2.8h, #6
        str             d0, [\dst]
        str             d1, [\dst, \d_strd]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            8b

.ifc \type, put
        .align JUMP_ALIGN
82:
        ldr             d21, [\src]
        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8
        movi            v2.4s, #32, lsl 8
        movi            v3.4s, #32, lsl 8
.else
        ldr             d21, [\src]
        mov             v0.16b, v4.16b
        mov             v1.16b, v4.16b
        mov             v2.16b, v4.16b
        mov             v3.16b, v4.16b
.endif
        sub             v18.16b, v21.16b, v5.16b
        sub             v21.16b, v21.16b, v5.16b

        tbl             v22.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v25.16b, {v19.16b, v20.16b}, v6.16b
        tbl             v23.16b, {v17.16b, v18.16b}, v28.16b
        tbl             v26.16b, {v20.16b, v21.16b}, v29.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v0.4s, v17.16b, v7.4b[1]
        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v1.4s, v20.16b, v7.4b[1]

        sdot            v2.4s, v22.16b, v7.4b[0]
        sdot            v2.4s, v23.16b, v7.4b[1]
        sdot            v3.4s, v25.16b, v7.4b[0]
        sdot            v3.4s, v26.16b, v7.4b[1]

        uzp1            v0.8h, v0.8h, v1.8h
        uzp1            v2.8h, v2.8h, v3.8h
.ifc \type, prep
        sshr            v0.8h, v0.8h, #2
        sshr            v1.8h, v2.8h, #2
        stp             q0, q1, [\dst]
.else
        sqrshrun        v0.8b, v0.8h, #6
        sqrshrun        v1.8b, v2.8h, #6
        str             d0, [\dst]
        str             d1, [\dst, \d_strd]
.endif
        ret

        .align JUMP_ALIGN
40:     // V - 4xN or 2xN (put only)
.ifc \type, put
        cmp             \w, #2
        b.eq            20f
.endif
        ldr             s16, [\src]
        ldr             s17, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             s18, [\src]
        ldr             s19, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        ldr             s20, [\src]
        ldr             s21, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             s22, [\src]
        ldr             s23, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        subs            \h, \h, #2  // for prep: sub is enough

        zip1            v0.8b, v16.8b, v17.8b
        zip1            v2.8b, v18.8b, v19.8b
        zip1            v18.8b, v20.8b, v21.8b
        zip1            v24.8b, v22.8b, v23.8b

        zip1            v16.8h, v0.8h, v2.8h
        zip1            v17.8h, v18.8h, v24.8h

        sub             v16.16b, v16.16b, v5.16b
        sub             v17.16b, v17.16b, v5.16b
.ifc \type, put
        b.eq            42f
.endif

        .align LOOP_ALIGN
4:
        ldr             s18, [\src]
        ldr             s21, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
.ifc \type, prep
        mov             v0.16b, v4.16b
        mov             v1.16b, v4.16b
.else
        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8
.endif
        sub             v18.16b, v18.16b, v5.16b
        sub             v21.16b, v21.16b, v5.16b

        tbl             v19.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v20.16b, {v17.16b, v18.16b}, v28.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v0.4s, v17.16b, v7.4b[1]

        tbl             v16.16b, {v19.16b, v20.16b}, v6.16b
        tbl             v17.16b, {v20.16b, v21.16b}, v28.16b

        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v1.4s, v20.16b, v7.4b[1]
.ifc \type, prep
        subs            \h, \h, #2
        shrn            v0.4h, v0.4s, #2
        shrn2           v0.8h, v1.4s, #2
        str             q0, [\dst], #16
.else
        uzp1            v0.8h, v0.8h, v1.8h
        sqrshrun        v0.8b, v0.8h, #6
        subs            \h, \h, #2
        fmov            x8, d0
        lsr             x9, x8, #32
        str             w8, [\dst]
        str             w9, [\dst, \d_strd]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            4b

.ifc \type, put
        .align JUMP_ALIGN
42:
        ldr             s18, [\src]
        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8
.else
        ldr             s18, [\src]
        mov             v0.16b, v4.16b
        mov             v1.16b, v4.16b
.endif
        sub             v18.16b, v18.16b, v5.16b

        tbl             v19.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v20.16b, {v17.16b, v18.16b}, v28.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v0.4s, v17.16b, v7.4b[1]

        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v1.4s, v20.16b, v7.4b[1]
.ifc \type, prep
        shrn            v0.4h, v0.4s, #2
        shrn2           v0.8h, v1.4s, #2
        str             q0, [\dst]
        ret
.else
        uzp1            v0.8h, v0.8h, v1.8h
        sqrshrun        v0.8b, v0.8h, #6
        fmov            x8, d0
        lsr             x9, x8, #32
        str             w8, [\dst]
        str             w9, [\dst, \d_strd]
        ret

        .align JUMP_ALIGN
20:     // V - 2xN
        ldr             h16, [\src]
        ldr             h17, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             h18, [\src]
        ldr             h19, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        ldr             h20, [\src]
        ldr             h21, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             h22, [\src]
        ldr             h23, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        subs            \h, \h, #2

        zip1            v0.8b, v16.8b, v17.8b
        zip1            v2.8b, v18.8b, v19.8b
        zip1            v18.8b, v20.8b, v21.8b
        zip1            v24.8b, v22.8b, v23.8b

        zip1            v16.4h, v0.4h, v2.4h
        zip1            v17.4h, v18.4h, v24.4h

        sub             v16.8b, v16.8b, v5.8b
        sub             v17.8b, v17.8b, v5.8b

        b.eq            22f

        .align LOOP_ALIGN
2:
        ldr             h18, [\src]
        ldr             h21, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8

        sub             v18.8b, v18.8b, v5.8b
        sub             v21.8b, v21.8b, v5.8b

        tbl             v19.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v20.16b, {v17.16b, v18.16b}, v28.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v0.4s, v17.16b, v7.4b[1]

        tbl             v16.16b, {v19.16b, v20.16b}, v6.16b
        tbl             v17.16b, {v20.16b, v21.16b}, v28.16b

        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v1.4s, v20.16b, v7.4b[1]

        uzp1            v0.8h, v0.8h, v1.8h
        sqrshrun        v0.8b, v0.8h, #6

        subs            \h, \h, #2
        fmov            x8, d0
        lsr             x9, x8, #32
        strh            w8, [\dst]
        strh            w9, [\dst, \d_strd]
        add             \dst, \dst, \d_strd, lsl #1
        b.gt            2b

        .align JUMP_ALIGN
22:
        ldr             h18, [\src]

        movi            v0.4s, #32, lsl 8   // 64 * 128, bias for SDOT
        movi            v1.4s, #32, lsl 8

        sub             v18.8b, v18.8b, v5.8b

        tbl             v19.16b, {v16.16b, v17.16b}, v6.16b
        tbl             v20.16b, {v17.16b, v18.16b}, v28.16b

        sdot            v0.4s, v16.16b, v7.4b[0]
        sdot            v0.4s, v17.16b, v7.4b[1]

        sdot            v1.4s, v19.16b, v7.4b[0]
        sdot            v1.4s, v20.16b, v7.4b[1]

        uzp1            v0.8h, v0.8h, v1.8h
        sqrshrun        v0.8b, v0.8h, #6

        fmov            x8, d0
        lsr             x9, x8, #32
        strh            w8, [\dst]
        strh            w9, [\dst, \d_strd]
        ret
.endif

        .align JUMP_ALIGN
L(\type\()_8tap_h_hv_\isa):
        madd            \mx, \mx, w11, w9
        madd            w14, \my, w11, w10      // for HV
        ldr             q28, L(h_tbl_neon_dotprod)
        mov             w13, 0x2002             // FILTER_WEIGHT * 128 + rounding
        sub             \src, \src, #4          // src - 4
        dup             v27.4s, w13
        ubfx            w9, \mx, #7, #7
        and             \mx, \mx, #0x7F
        ubfx            w11, w14, #7, #7        // for HV
        and             w14, w14, #0x7F         // for HV
        cmp             \w, #4
        csel            \mx, \mx, w9, le
        add             \xmx, x12, \xmx, lsl #3 // subpel H filter address
        movi            v24.16b, #128
        cbz             \my, L(\type\()_8tap_h_\isa)

        // HV cases
        cmp             \h, #4
        csel            w14, w14, w11, le
        sub             \src, \src, \s_strd, lsl #1 // src - src_stride * 2 - 4
        add             \xmy, x12, x14, lsl #3      // subpel V filter address
        mov             x15, x30
        ldr             d7, [\xmy]
.ifc \type, put
        ldr             q25, L(hv_tbl_neon_dotprod)
.endif
        sxtl            v7.8h, v7.8b
        cmp             w10, SHARP1
        b.ne            L(\type\()_6tap_hv_\isa)    // vertical != SHARP1

        // HV 8-tap cases
        sub             \src, \src, \s_strd         // src - src_stride * 3 - 4
        cmp             \w, #4
        b.eq            40f
.ifc \type, put
        b.lt            20f
.endif

        // .align JUMP_ALIGN    // fallthrough
80:     // HV8 - 8xN+
        ldr             q29, L(h_tbl_neon_dotprod) + 16
        ldr             q30, L(h_tbl_neon_dotprod) + 32
        ldr             d26, [\xmx]
.ifc \type, prep
        add             \wd_strd, \w, \w
.endif

        .align LOOP_ALIGN
81:
        mov             \lsrc, \src
        mov             \ldst, \dst
        mov             w8, \h

        bl              L(\type\()_hv_filter8_\isa)
        mov             v16.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v17.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v18.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v19.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v20.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v21.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)

        .align LOOP_ALIGN
8:
        ldr             q23, [\lsrc]
        add             \lsrc, \lsrc, \s_strd

        smull           v0.4s, v16.4h, v7.h[0]
        smull2          v1.4s, v16.8h, v7.h[0]
        mov             v16.16b, v17.16b

        sub             v23.16b, v23.16b, v24.16b

        mov             v5.16b, v27.16b
        mov             v6.16b, v27.16b

        smlal           v0.4s, v17.4h, v7.h[1]
        smlal2          v1.4s, v17.8h, v7.h[1]
        mov             v17.16b, v18.16b

        tbl             v2.16b, {v23.16b}, v28.16b
        tbl             v3.16b, {v23.16b}, v29.16b
        tbl             v4.16b, {v23.16b}, v30.16b

        smlal           v0.4s, v18.4h, v7.h[2]
        smlal2          v1.4s, v18.8h, v7.h[2]
        mov             v18.16b, v19.16b

        sdot            v5.4s, v2.16b, v26.4b[0]
        sdot            v6.4s, v3.16b, v26.4b[0]

        smlal           v0.4s, v19.4h, v7.h[3]
        smlal2          v1.4s, v19.8h, v7.h[3]
        mov             v19.16b, v20.16b

        sdot            v5.4s, v3.16b, v26.4b[1]
        sdot            v6.4s, v4.16b, v26.4b[1]

        smlal           v0.4s, v20.4h, v7.h[4]
        smlal2          v1.4s, v20.8h, v7.h[4]
        mov             v20.16b, v21.16b

        smlal           v0.4s, v21.4h, v7.h[5]
        smlal2          v1.4s, v21.8h, v7.h[5]
.ifc \type, prep
        uzp1            v23.8h, v5.8h, v6.8h
.endif
        mov             v21.16b, v22.16b

        smlal           v0.4s, v22.4h, v7.h[6]
        smlal2          v1.4s, v22.8h, v7.h[6]
.ifc \type, prep
        sshr            v22.8h, v23.8h, #2
        smlal           v0.4s, v22.4h, v7.h[7]
        smlal2          v1.4s, v22.8h, v7.h[7]
        rshrn           v0.4h, v0.4s, #6
        rshrn2          v0.8h, v1.4s, #6
        subs            w8, w8, #1
        st1             {v0.8h}, [\ldst], \d_strd
        b.gt            8b
        add             \dst, \dst, #16
.else
        shrn            v22.4h, v5.4s, #2
        shrn2           v22.8h, v6.4s, #2
        smlal           v0.4s, v22.4h, v7.h[7]
        smlal2          v1.4s, v22.8h, v7.h[7]
        tbl             v0.16b, {v0.16b, v1.16b}, v25.16b
        subs            w8, w8, #1
        sqrshrun        v0.8b, v0.8h, #2
        st1             {v0.8b}, [\ldst], \d_strd
        b.gt            8b
        add             \dst, \dst, #8
.endif
        add             \src, \src, #8
        subs            \w, \w, #8
        b.gt            81b
        ret             x15

        .align JUMP_ALIGN
40:     // HV8 - 4xN
        ldr             s26, [\xmx, #2]
        add             \src, \src, #2

        bl              L(\type\()_hv_filter4_\isa)
        mov             v16.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v17.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v18.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v19.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v20.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v21.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)

        .align LOOP_ALIGN
4:
        ld1             {v4.8b}, [\src], \s_strd

        smull           v0.4s, v16.4h, v7.h[0]
        smlal           v0.4s, v17.4h, v7.h[1]
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b
        sub             v4.16b, v4.16b, v24.16b

        smlal           v0.4s, v18.4h, v7.h[2]
        smlal           v0.4s, v19.4h, v7.h[3]
        tbl             v2.16b, {v4.16b}, v28.16b
        mov             v5.16b, v27.16b

        mov             v18.16b, v19.16b
        mov             v19.16b, v20.16b

        smlal           v0.4s, v20.4h, v7.h[4]
        smlal           v0.4s, v21.4h, v7.h[5]

        sdot            v5.4s, v2.16b, v26.4b[0]
        mov             v20.16b, v21.16b
        mov             v21.16b, v22.16b
.ifc \type, put
        subs            \h, \h, #1
.endif
        smlal           v0.4s, v22.4h, v7.h[6]
        shrn            v22.4h, v5.4s, #2

        smlal           v0.4s, v22.4h, v7.h[7]
.ifc \type, prep
        rshrn           v0.4h, v0.4s, #6
        str             d0, [\dst], #8
        subs            \h, \h, #1
.else
        tbl             v0.16b, {v0.16b, v1.16b}, v25.16b
        sqrshrun        v0.8b, v0.8h, #2
        str             s0, [\dst]
        add             \dst, \dst, \d_strd
.endif
        b.gt            4b
        ret             x15

.ifc \type, put
        .align JUMP_ALIGN
20:     // HV8 - 2xN
        ldr             s26, [\xmx, #2]
        add             \src, \src, #2

        bl              L(\type\()_hv_filter4_\isa)
        mov             v16.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v17.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v18.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v19.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v20.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v21.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)

        .align LOOP_ALIGN
2:
        ld1             {v4.8b}, [\src], \s_strd

        smull           v0.4s, v16.4h, v7.h[0]
        smlal           v0.4s, v17.4h, v7.h[1]
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b
        sub             v4.16b, v4.16b, v24.16b

        smlal           v0.4s, v18.4h, v7.h[2]
        smlal           v0.4s, v19.4h, v7.h[3]
        tbl             v2.16b, {v4.16b}, v28.16b
        mov             v5.16b, v27.16b

        mov             v18.16b, v19.16b
        mov             v19.16b, v20.16b

        smlal           v0.4s, v20.4h, v7.h[4]
        smlal           v0.4s, v21.4h, v7.h[5]

        sdot            v5.4s, v2.16b, v26.4b[0]
        mov             v20.16b, v21.16b
        mov             v21.16b, v22.16b

        subs            \h, \h, #1
        smlal           v0.4s, v22.4h, v7.h[6]
        shrn            v22.4h, v5.4s, #2

        smlal           v0.4s, v22.4h, v7.h[7]
        tbl             v0.16b, {v0.16b, v1.16b}, v25.16b
        sqrshrun        v0.8b, v0.8h, #2

        str             h0, [\dst]
        add             \dst, \dst, \d_strd
        b.gt            2b
        ret             x15
.endif

        .align JUMP_ALIGN
L(\type\()_6tap_hv_\isa):
        cmp             \w, #4
        b.eq            40f
.ifc \type, put
        b.lt            20f
.endif

        // .align JUMP_ALIGN    // fallthrough
80:     // HV6 - 8xN+
        ldr             q29, L(h_tbl_neon_dotprod) + 16
        ldr             q30, L(h_tbl_neon_dotprod) + 32
        ldr             d26, [\xmx]
.ifc \type, prep
        add             \wd_strd, \w, \w
.endif

        .align LOOP_ALIGN
81:
        mov             \lsrc, \src
        mov             \ldst, \dst
        mov             w8, \h

        bl              L(\type\()_hv_filter8_\isa)
        mov             v16.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v17.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v18.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v19.16b, v22.16b
        bl              L(\type\()_hv_filter8_\isa)
        mov             v20.16b, v22.16b

        .align LOOP_ALIGN
8:
        ldr             q23, [\xmy]
        add             \xmy, \xmy, \s_strd

        smull           v0.4s, v16.4h, v7.h[1]
        smull2          v1.4s, v16.8h, v7.h[1]
        sub             v23.16b, v23.16b, v24.16b
        mov             v16.16b, v17.16b

        mov             v5.16b, v27.16b
        mov             v6.16b, v27.16b

        tbl             v2.16b, {v23.16b}, v28.16b
        tbl             v3.16b, {v23.16b}, v29.16b

        smlal           v0.4s, v17.4h, v7.h[2]
        smlal2          v1.4s, v17.8h, v7.h[2]
        tbl             v4.16b, {v23.16b}, v30.16b
        mov             v17.16b, v18.16b

        sdot            v5.4s, v2.16b, v26.4b[0]
        sdot            v6.4s, v3.16b, v26.4b[0]
        smlal           v0.4s, v18.4h, v7.h[3]
        smlal2          v1.4s, v18.8h, v7.h[3]
        mov             v18.16b, v19.16b

        sdot            v5.4s, v3.16b, v26.4b[1]
        sdot            v6.4s, v4.16b, v26.4b[1]
        smlal           v0.4s, v19.4h, v7.h[4]
        smlal2          v1.4s, v19.8h, v7.h[4]
        mov             v19.16b, v20.16b
        uzp1            v23.8h, v5.8h, v6.8h

        smlal           v0.4s, v20.4h, v7.h[5]
        smlal2          v1.4s, v20.8h, v7.h[5]
        sshr            v20.8h, v23.8h, #2
.ifc \type, prep
        smlal           v0.4s, v20.4h, v7.h[6]
        smlal2          v1.4s, v20.8h, v7.h[6]
        rshrn           v0.4h, v0.4s, #6
        rshrn2          v0.8h, v1.4s, #6
        st1             {v0.8h}, [\ldst], \d_strd
        subs            w8, w8, #1
        b.gt            8b
        add             \dst, \dst, #16
.else
        subs            w8, w8, #1
        smlal           v0.4s, v20.4h, v7.h[6]
        smlal2          v1.4s, v20.8h, v7.h[6]
        tbl             v0.16b, {v0.16b, v1.16b}, v25.16b
        sqrshrun        v0.8b, v0.8h, #2
        st1             {v0.8b}, [\ldst], \d_strd
        b.gt            8b
        add             \dst, \dst, #8
.endif
        add             \src, \src, #8
        subs            \w, \w, #8
        b.gt            81b
        ret             x15

        .align FUNC_ALIGN
L(\type\()_hv_filter8_\isa):
        ldr             q4, [\lsrc]
        add             \lsrc, \lsrc, \s_strd
        sub             v4.16b, v4.16b, v24.16b
        mov             v22.16b, v27.16b
        mov             v23.16b, v27.16b
        tbl             v2.16b, {v4.16b}, v28.16b
        tbl             v3.16b, {v4.16b}, v29.16b
        tbl             v4.16b, {v4.16b}, v30.16b
        sdot            v22.4s, v2.16b, v26.4b[0]
        sdot            v22.4s, v3.16b, v26.4b[1]
        sdot            v23.4s, v3.16b, v26.4b[0]
        sdot            v23.4s, v4.16b, v26.4b[1]
        shrn            v22.4h, v22.4s, #2
        shrn2           v22.8h, v23.4s, #2
        ret

        .align FUNC_ALIGN
L(\type\()_hv_filter4_\isa):
        mov             v22.16b, v27.16b
        ld1             {v4.8b}, [\src], \s_strd
        sub             v4.16b, v4.16b, v24.16b
        tbl             v2.16b, {v4.16b}, v28.16b
        sdot            v22.4s, v2.16b, v26.4b[0]
        shrn            v22.4h, v22.4s, #2
        ret

        .align JUMP_ALIGN
40:     // HV6 - 4xN
        ldr             s26, [\xmx, #2]
        add             \src, \src, #2

        bl              L(\type\()_hv_filter4_\isa)
        mov             v16.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v17.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v18.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v19.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v20.16b, v22.16b

        .align LOOP_ALIGN
4:
        ld1             {v4.8b}, [\src], \s_strd

        smull           v0.4s, v16.4h, v7.h[1]
        smlal           v0.4s, v17.4h, v7.h[2]
        sub             v4.16b, v4.16b, v24.16b
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b

        smlal           v0.4s, v18.4h, v7.h[3]
        smlal           v0.4s, v19.4h, v7.h[4]
        tbl             v2.16b, {v4.16b}, v28.16b
        mov             v5.16b, v27.16b

        mov             v18.16b, v19.16b
        mov             v19.16b, v20.16b
        sdot            v5.4s, v2.16b, v26.4b[0]

        smlal           v0.4s, v20.4h, v7.h[5]
        shrn            v20.4h, v5.4s, #2
.ifc \type, prep
        smlal           v0.4s, v20.4h, v7.h[6]
        rshrn           v0.4h, v0.4s, #6
        str             d0, [\dst], #8
        subs            \h, \h, #1
.else
        subs            \h, \h, #1
        smlal           v0.4s, v20.4h, v7.h[6]
        tbl             v0.16b, {v0.16b}, v25.16b
        sqrshrun        v0.8b, v0.8h, #2
        str             s0, [\dst]
        add             \dst, \dst, \d_strd
.endif
        b.gt            4b
        ret             x15

.ifc \type, put
        .align JUMP_ALIGN
20:     // HV6 - 2xN
        ldr             s26, [\xmx, #2]
        add             \src, \src, #2

        bl              L(\type\()_hv_filter4_\isa)
        mov             v16.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v17.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v18.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v19.16b, v22.16b
        bl              L(\type\()_hv_filter4_\isa)
        mov             v20.16b, v22.16b

        .align LOOP_ALIGN
2:
        ld1             {v4.8b}, [\src], \s_strd

        smull           v0.4s, v16.4h, v7.h[1]
        smlal           v0.4s, v17.4h, v7.h[2]
        sub             v4.16b, v4.16b, v24.16b
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b

        smlal           v0.4s, v18.4h, v7.h[3]
        smlal           v0.4s, v19.4h, v7.h[4]
        tbl             v2.16b, {v4.16b}, v28.16b
        mov             v5.16b, v27.16b

        mov             v18.16b, v19.16b
        mov             v19.16b, v20.16b
        sdot            v5.4s, v2.16b, v26.4b[0]

        smlal           v0.4s, v20.4h, v7.h[5]
        shrn            v20.4h, v5.4s, #2

        subs            \h, \h, #1
        smlal           v0.4s, v20.4h, v7.h[6]

        tbl             v0.16b, {v0.16b}, v25.16b
        sqrshrun        v0.8b, v0.8h, #2

        str             h0, [\dst]
        add             \dst, \dst, \d_strd
        b.gt            2b
        ret             x15
.endif

        .align JUMP_ALIGN
L(\type\()_8tap_h_\isa):
        adr             x9, L(\type\()_8tap_h_\isa\()_tbl)
        ldrh            w8, [x9, x8, lsl #1]
.ifc \type, put
        mov             w10, #0x2022    // 64 * 128 + 34, bias and rounding for SDOT
        dup             v27.4s, w10
.endif
        sub             x9, x9, x8
        br              x9

.ifc \type, put
        .align JUMP_ALIGN
20:     // H - 2xN
        AARCH64_VALID_JUMP_TARGET
        add             \src, \src, #2
        ldr             s6, [\xmx, #2]

        .align LOOP_ALIGN
2:
        ldr             d0, [\src]
        ldr             d1, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        sub             v0.8b, v0.8b, v24.8b
        sub             v1.8b, v1.8b, v24.8b

        mov             v4.16b, v27.16b
        mov             v5.16b, v27.16b

        tbl             v2.16b, {v0.16b}, v28.16b
        tbl             v3.16b, {v1.16b}, v28.16b

        sdot            v4.4s, v2.16b, v6.4b[0]
        sdot            v5.4s, v3.16b, v6.4b[0]

        uzp1            v4.8h, v4.8h, v5.8h
        sqshrun         v4.8b, v4.8h, #6

        subs            \h, \h, #2
        fmov            x8, d4
        lsr             x9, x8, #32
        strh            w8, [\dst]
        strh            w9, [\dst, \d_strd]
        add             \dst, \dst, \d_strd, lsl #1
        b.gt            2b
        ret

.endif

        .align JUMP_ALIGN
40:     // H - 4xN
        AARCH64_VALID_JUMP_TARGET
        add             \src, \src, #2
        ldr             s26, [\xmx, #2]

        .align LOOP_ALIGN
4:
        ldr             d0, [\src]
        ldr             d1, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        sub             v0.8b, v0.8b, v24.8b
        sub             v1.8b, v1.8b, v24.8b

        mov             v4.16b, v27.16b
        mov             v5.16b, v27.16b

        tbl             v2.16b, {v0.16b}, v28.16b
        tbl             v3.16b, {v1.16b}, v28.16b

        sdot            v4.4s, v2.16b, v26.4b[0]
        sdot            v5.4s, v3.16b, v26.4b[0]
.ifc \type, prep
        subs            \h, \h, #2
        shrn            v4.4h, v4.4s, #2
        shrn2           v4.8h, v5.4s, #2
        str             q4, [\dst], #16
.else
        uzp1            v4.8h, v4.8h, v5.8h
        sqshrun         v4.8b, v4.8h, #6
        subs            \h, \h, #2
        fmov            x8, d4
        lsr             x9, x8, #32
        str             w8, [\dst]
        str             w9, [\dst, \d_strd]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            4b
        ret

        .align JUMP_ALIGN
80:     // H - 8xN
        AARCH64_VALID_JUMP_TARGET
        ldr             q29, L(h_tbl_neon_dotprod) + 16
        ldr             q30, L(h_tbl_neon_dotprod) + 32
        ldr             d26, [\xmx]

        .align LOOP_ALIGN
8:
        ldr             q0, [\src]
        ldr             q16, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        sub             v0.16b, v0.16b, v24.16b
        sub             v16.16b, v16.16b, v24.16b

        mov             v4.16b, v27.16b
        mov             v5.16b, v27.16b
        mov             v20.16b, v27.16b
        mov             v21.16b, v27.16b

        tbl             v1.16b, {v0.16b}, v28.16b
        tbl             v2.16b, {v0.16b}, v29.16b
        tbl             v3.16b, {v0.16b}, v30.16b
        tbl             v17.16b, {v16.16b}, v28.16b
        tbl             v18.16b, {v16.16b}, v29.16b
        tbl             v19.16b, {v16.16b}, v30.16b

        sdot            v4.4s, v1.16b, v26.4b[0]
        sdot            v5.4s, v2.16b, v26.4b[0]
        sdot            v20.4s, v17.16b, v26.4b[0]
        sdot            v21.4s, v18.16b, v26.4b[0]
        sdot            v4.4s, v2.16b, v26.4b[1]
        sdot            v5.4s, v3.16b, v26.4b[1]
        sdot            v20.4s, v18.16b, v26.4b[1]
        sdot            v21.4s, v19.16b, v26.4b[1]

        uzp1            v4.8h, v4.8h, v5.8h
        uzp1            v20.8h, v20.8h, v21.8h
.ifc \type, prep
        sshr            v4.8h, v4.8h, #2
        sshr            v20.8h, v20.8h, #2
        subs            \h, \h, #2
        stp             q4, q20, [\dst], #32
.else
        sqshrun         v4.8b, v4.8h, #6
        sqshrun         v20.8b, v20.8h, #6
        subs            \h, \h, #2
        str             d4, [\dst]
        str             d20, [\dst, \d_strd]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            8b
        ret

        .align JUMP_ALIGN
160:    // H - 16xN
        AARCH64_VALID_JUMP_TARGET
        ldr             q29, L(h_tbl_neon_dotprod) + 16
        ldr             q30, L(h_tbl_neon_dotprod) + 32
        ldr             q31, L(h_tbl_neon_dotprod) + 48
        ldr             d26, [\xmx]

        .align LOOP_ALIGN
16:
        ldp             q16, q17, [\src]
        add             \src, \src, \s_strd

        sub             v16.16b, v16.16b, v24.16b
        sub             v17.16b, v17.16b, v24.16b

        mov             v6.16b, v27.16b
        mov             v7.16b, v27.16b
        mov             v22.16b, v27.16b
        mov             v23.16b, v27.16b

        tbl             v0.16b, {v16.16b}, v28.16b
        tbl             v1.16b, {v16.16b}, v29.16b
        tbl             v2.16b, {v16.16b}, v30.16b
        tbl             v3.16b, {v16.16b, v17.16b}, v31.16b
        tbl             v4.16b, {v17.16b}, v28.16b

        sdot            v6.4s, v0.16b, v26.4b[0]
        sdot            v7.4s, v1.16b, v26.4b[0]
        sdot            v22.4s, v2.16b, v26.4b[0]
        sdot            v23.4s, v3.16b, v26.4b[0]
        sdot            v6.4s, v1.16b, v26.4b[1]
        sdot            v7.4s, v2.16b, v26.4b[1]
        sdot            v22.4s, v3.16b, v26.4b[1]
        sdot            v23.4s, v4.16b, v26.4b[1]

        uzp1            v6.8h, v6.8h, v7.8h
        uzp1            v22.8h, v22.8h, v23.8h
.ifc \type, prep
        sshr            v6.8h, v6.8h, #2
        sshr            v22.8h, v22.8h, #2
        subs            \h, \h, #1
        stp             q6, q22, [\dst], #32
.else
        sqshrun         v6.8b, v6.8h, #6
        sqshrun2        v6.16b, v22.8h, #6
        subs            \h, \h, #1
        str             q6, [\dst]
        add             \dst, \dst, \d_strd
.endif
        b.gt            16b
        ret

        .align JUMP_ALIGN
320:    // H - 32xN+
640:
1280:
        AARCH64_VALID_JUMP_TARGET
        ldr             q29, L(h_tbl_neon_dotprod) + 16
        ldr             q30, L(h_tbl_neon_dotprod) + 32
        ldr             q31, L(h_tbl_neon_dotprod) + 48
        ldr             d26, [\xmx]
.ifc \type, put
        sub             \d_strd, \d_strd, \w, uxtw
.endif
        sub             \s_strd, \s_strd, \w, uxtw
        mov             w8, \w

        .align LOOP_ALIGN
32:
        ldp             q16, q17, [\src], #16

        sub             v16.16b, v16.16b, v24.16b
        sub             v17.16b, v17.16b, v24.16b

        mov             v6.16b, v27.16b
        mov             v7.16b, v27.16b
        mov             v22.16b, v27.16b
        mov             v23.16b, v27.16b

        tbl             v0.16b, {v16.16b}, v28.16b
        tbl             v1.16b, {v16.16b}, v29.16b
        tbl             v2.16b, {v16.16b}, v30.16b
        tbl             v3.16b, {v16.16b, v17.16b}, v31.16b
        tbl             v4.16b, {v17.16b}, v28.16b

        sdot            v6.4s, v0.16b, v26.4b[0]
        sdot            v7.4s, v1.16b, v26.4b[0]
        sdot            v22.4s, v2.16b, v26.4b[0]
        sdot            v23.4s, v3.16b, v26.4b[0]
        sdot            v6.4s, v1.16b, v26.4b[1]
        sdot            v7.4s, v2.16b, v26.4b[1]
        sdot            v22.4s, v3.16b, v26.4b[1]
        sdot            v23.4s, v4.16b, v26.4b[1]

        uzp1            v6.8h, v6.8h, v7.8h
        uzp1            v22.8h, v22.8h, v23.8h
.ifc \type, prep
        sshr            v6.8h, v6.8h, #2
        sshr            v22.8h, v22.8h, #2
        subs            w8, w8, #16
        stp             q6, q22, [\dst], #32
.else
        sqshrun         v6.8b, v6.8h, #6
        sqshrun2        v6.16b, v22.8h, #6
        subs            w8, w8, #16
        str             q6, [\dst], #16
.endif
        b.gt            32b

        add             \src, \src, \s_strd
.ifc \type, put
        add             \dst, \dst, \d_strd
.endif
        mov             w8, \w
        subs            \h, \h, #1
        b.gt            32b
        ret

L(\type\()_8tap_h_\isa\()_tbl):
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 1280b)
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 640b)
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 320b)
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 160b)
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 80b)
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 40b)
.ifc \type, put
        .hword (L(\type\()_8tap_h_\isa\()_tbl) - 20b)
.endif
endfunc
.endm

// dst(x0), d_strd(x7), src(x1), s_strd(x2), w(w3), h(w4), mx(w5), my(w6)
// xmx(x5), xmy(x6), ldst(x5), lsrc(x6), wd_strd(w7)
filter_8tap_fn prep, neon_dotprod, x0, x7, x1, x2, w3, w4, w5, w6, x5, x6, x5, x6, w7

// dst(x0) d_strd(x1) src(x2) s_strd(x3) w(w4) h(w5) mx(w6) my(w7)
// xmx(x6), xmy(x7), ldst(x6), lsrc(x7), wd_strd(w1)
filter_8tap_fn  put, neon_dotprod, x0, x1, x2, x3, w4, w5, w6, w7, x6, x7, x6, x7, w1

DISABLE_DOTPROD
#endif  // HAVE_DOTPROD
