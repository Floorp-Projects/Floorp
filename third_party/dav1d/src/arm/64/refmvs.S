/*
 * Copyright © 2021, VideoLAN and dav1d authors
 * Copyright © 2021, Martin Storsjo
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

// void dav1d_splat_mv_neon(refmvs_block **rr, const refmvs_block *rmv,
//                          int bx4, int bw4, int bh4)

function splat_mv_neon, export=1
        ld1             {v3.16b},  [x1]
        clz             w3,  w3
        adr             x5,  L(splat_tbl)
        sub             w3,  w3,  #26
        ext             v2.16b,  v3.16b,  v3.16b,  #12
        ldrh            w3,  [x5, w3, uxtw #1]
        add             w2,  w2,  w2,  lsl #1
        ext             v0.16b,  v2.16b,  v3.16b,  #4
        sub             x3,  x5,  w3, uxtw
        ext             v1.16b,  v2.16b,  v3.16b,  #8
        lsl             w2,  w2,  #2
        ext             v2.16b,  v2.16b,  v3.16b,  #12
1:
        ldr             x1,  [x0],  #8
        subs            w4,  w4,  #1
        add             x1,  x1,  x2
        br              x3

10:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8b}, [x1]
        str             s2,  [x1, #8]
        b.gt            1b
        ret
20:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b}, [x1]
        str             d1,  [x1, #16]
        b.gt            1b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
160:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
80:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
40:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1]
        b.gt            1b
        ret

L(splat_tbl):
        .hword L(splat_tbl) -  320b
        .hword L(splat_tbl) -  160b
        .hword L(splat_tbl) -   80b
        .hword L(splat_tbl) -   40b
        .hword L(splat_tbl) -   20b
        .hword L(splat_tbl) -   10b
endfunc

const mv_tbls, align=4
        .byte           255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
        .byte           0, 1, 2, 3, 8, 0, 1, 2, 3, 8, 0, 1, 2, 3, 8, 0
        .byte           4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4
        .byte           4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4
endconst

const mask_mult, align=4
        .byte           1, 2, 1, 2, 0, 0, 0, 0
endconst

// void dav1d_save_tmvs_neon(refmvs_temporal_block *rp, ptrdiff_t stride,
//                           refmvs_block **rr, const uint8_t *ref_sign,
//                           int col_end8, int row_end8,
//                           int col_start8, int row_start8)
function save_tmvs_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x29, x30, [sp, #-16]!
        mov             x29, sp

        movi            v30.8b,  #0
        ld1             {v31.8b}, [x3]
        adr             x8,  L(save_tmvs_tbl)
        movrel          x16, mask_mult
        movrel          x13, mv_tbls
        ld1             {v29.8b}, [x16]
        ext             v31.8b,  v30.8b,  v31.8b,  #7 // [0, ref_sign]
        mov             w15, #5
        mov             w14, #12*2
        sxtw            x4,  w4
        sxtw            x6,  w6
        mul             w1,  w1,  w15             // stride *= 5
        sub             w5,  w5,  w7              // h = row_end8 - row_start8
        lsl             w7,  w7,  #1              // row_start8 <<= 1
1:
        mov             w15, #5
        and             w9,  w7,  #30             // (y & 15) * 2
        ldr             x9,  [x2, w9, uxtw #3]    // b = rr[(y & 15) * 2]
        add             x9,  x9,  #12             // &b[... + 1]
        madd            x10, x4,  x14,  x9        // end_cand_b = &b[col_end8*2 + 1]
        madd            x9,  x6,  x14,  x9        // cand_b = &b[x*2 + 1]

        madd            x3,  x6,  x15,  x0        // &rp[x]

2:
        ldrb            w11, [x9, #10]            // cand_b->bs
        ld1             {v0.16b}, [x9]            // cand_b->mv
        add             x11, x8,  w11, uxtw #2
        ldr             h1,  [x9, #8]             // cand_b->ref
        ldrh            w12, [x11]                // bw8
        mov             x15, x8
        add             x9,  x9,  w12, uxtw #1    // cand_b += bw8*2
        cmp             x9,  x10
        mov             v2.8b,   v0.8b
        b.ge            3f

        ldrb            w15, [x9, #10]            // cand_b->bs
        add             x16, x9,  #8
        ld1             {v4.16b}, [x9]            // cand_b->mv
        add             x15, x8,  w15, uxtw #2
        ld1             {v1.h}[1], [x16]          // cand_b->ref
        ldrh            w12, [x15]                // bw8
        add             x9,  x9,  w12, uxtw #1    // cand_b += bw8*2
        trn1            v2.2d,   v0.2d,   v4.2d

3:
        abs             v2.8h,   v2.8h            // abs(mv[].xy)
        tbl             v1.8b, {v31.16b}, v1.8b   // ref_sign[ref]
        ushr            v2.8h,   v2.8h,   #12     // abs(mv[].xy) >> 12
        umull           v1.8h,   v1.8b,   v29.8b  // ref_sign[ref] * {1, 2}
        cmeq            v2.4s,   v2.4s,   #0      // abs(mv[].xy) <= 4096
        xtn             v2.4h,   v2.4s            // abs() condition to 16 bit
        and             v1.8b,   v1.8b,   v2.8b   // h[0-3] contains conditions for mv[0-1]
        addp            v1.4h,   v1.4h,   v1.4h   // Combine condition for [1] and [0]
        umov            w16, v1.h[0]              // Extract case for first block
        umov            w17, v1.h[1]
        ldrh            w11, [x11, #2]            // Fetch jump table entry
        ldrh            w15, [x15, #2]
        ldr             q1, [x13, w16, uxtw #4]   // Load permutation table base on case
        ldr             q5, [x13, w17, uxtw #4]
        sub             x11, x8,  w11, uxtw       // Find jump table target
        sub             x15, x8,  w15, uxtw
        tbl             v0.16b, {v0.16b}, v1.16b  // Permute cand_b to output refmvs_temporal_block
        tbl             v4.16b, {v4.16b}, v5.16b

        // v1 follows on v0, with another 3 full repetitions of the pattern.
        ext             v1.16b,  v0.16b,  v0.16b,  #1
        ext             v5.16b,  v4.16b,  v4.16b,  #1
        // v2 ends with 3 complete repetitions of the pattern.
        ext             v2.16b,  v0.16b,  v1.16b,  #4
        ext             v6.16b,  v4.16b,  v5.16b,  #4

        blr             x11
        b.ge            4f  // if (cand_b >= end)
        mov             v0.16b,  v4.16b
        mov             v1.16b,  v5.16b
        mov             v2.16b,  v6.16b
        cmp             x9,  x10
        blr             x15
        b.lt            2b  // if (cand_b < end)

4:
        subs            w5,  w5,  #1              // h--
        add             w7,  w7,  #2              // y += 2
        add             x0,  x0,  x1              // rp += stride
        b.gt            1b

        ldp             x29, x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret

10:
        AARCH64_VALID_CALL_TARGET
        add             x16, x3,  #4
        st1             {v0.s}[0], [x3]
        st1             {v0.b}[4], [x16]
        add             x3,  x3,  #5
        ret
20:
        AARCH64_VALID_CALL_TARGET
        add             x16, x3,  #8
        st1             {v0.d}[0], [x3]
        st1             {v0.h}[4], [x16]
        add             x3,  x3,  #2*5
        ret
40:
        AARCH64_VALID_CALL_TARGET
        st1             {v0.16b}, [x3]
        str             s1, [x3, #16]
        add             x3,  x3,  #4*5
        ret
80:
        AARCH64_VALID_CALL_TARGET
        // This writes 6 full entries plus 2 extra bytes
        st1             {v0.16b, v1.16b}, [x3]
        // Write the last few, overlapping with the first write.
        stur            q2, [x3, #(8*5-16)]
        add             x3,  x3,  #8*5
        ret
160:
        AARCH64_VALID_CALL_TARGET
        add             x16, x3,  #6*5
        add             x17, x3,  #12*5
        // This writes 6 full entries plus 2 extra bytes
        st1             {v0.16b, v1.16b}, [x3]
        // Write another 6 full entries, slightly overlapping with the first set
        st1             {v0.16b, v1.16b}, [x16]
        // Write 8 bytes (one full entry) after the first 12
        st1             {v0.8b}, [x17]
        // Write the last 3 entries
        str             q2, [x3, #(16*5-16)]
        add             x3,  x3,  #16*5
        ret

L(save_tmvs_tbl):
        .hword 16 * 12
        .hword L(save_tmvs_tbl) - 160b
        .hword 16 * 12
        .hword L(save_tmvs_tbl) - 160b
        .hword 8 * 12
        .hword L(save_tmvs_tbl) -  80b
        .hword 8 * 12
        .hword L(save_tmvs_tbl) -  80b
        .hword 8 * 12
        .hword L(save_tmvs_tbl) -  80b
        .hword 8 * 12
        .hword L(save_tmvs_tbl) -  80b
        .hword 4 * 12
        .hword L(save_tmvs_tbl) -  40b
        .hword 4 * 12
        .hword L(save_tmvs_tbl) -  40b
        .hword 4 * 12
        .hword L(save_tmvs_tbl) -  40b
        .hword 4 * 12
        .hword L(save_tmvs_tbl) -  40b
        .hword 2 * 12
        .hword L(save_tmvs_tbl) -  20b
        .hword 2 * 12
        .hword L(save_tmvs_tbl) -  20b
        .hword 2 * 12
        .hword L(save_tmvs_tbl) -  20b
        .hword 2 * 12
        .hword L(save_tmvs_tbl) -  20b
        .hword 2 * 12
        .hword L(save_tmvs_tbl) -  20b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
        .hword 1 * 12
        .hword L(save_tmvs_tbl) -  10b
endfunc
