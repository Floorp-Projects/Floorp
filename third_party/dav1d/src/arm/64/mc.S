/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
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

.macro avg dst, t0, t1
        ld1             {\t0\().8h},   [x2],  16
        ld1             {\t1\().8h},   [x3],  16
        add             \t0\().8h,   \t0\().8h,   \t1\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #5
.endm

.macro avg16 dst, t0, t1, t2, t3
        ld1             {\t0\().8h,\t1\().8h},   [x2],  32
        ld1             {\t2\().8h,\t3\().8h},   [x3],  32
        add             \t0\().8h,   \t0\().8h,   \t2\().8h
        add             \t1\().8h,   \t1\().8h,   \t3\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #5
        sqrshrun2       \dst\().16b, \t1\().8h,   #5
.endm

.macro w_avg dst, t0, t1
        ld1             {\t0\().8h},   [x2],  16
        ld1             {\t1\().8h},   [x3],  16
        sub             \t0\().8h,   \t1\().8h,   \t0\().8h
        sqdmulh         \t0\().8h,   \t0\().8h,   v30.8h
        add             \t0\().8h,   \t1\().8h,   \t0\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #4
.endm

.macro w_avg16 dst, t0, t1, t2, t3
        ld1             {\t0\().8h,\t1\().8h},   [x2],  32
        ld1             {\t2\().8h,\t3\().8h},   [x3],  32
        sub             \t0\().8h,   \t2\().8h,   \t0\().8h
        sub             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqdmulh         \t0\().8h,   \t0\().8h,   v30.8h
        sqdmulh         \t1\().8h,   \t1\().8h,   v30.8h
        add             \t0\().8h,   \t2\().8h,   \t0\().8h
        add             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #4
        sqrshrun2       \dst\().16b, \t1\().8h,   #4
.endm

.macro mask dst, t0, t1
        ld1             {v30.8b},      [x6],  8
        ld1             {\t0\().8h},   [x2],  16
        mul             v30.8b, v30.8b, v31.8b
        ld1             {\t1\().8h},   [x3],  16
        shll            v30.8h, v30.8b, #8
        sub             \t0\().8h,   \t1\().8h,   \t0\().8h
        sqdmulh         \t0\().8h,   \t0\().8h,   v30.8h
        add             \t0\().8h,   \t1\().8h,   \t0\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #4
.endm

.macro mask16 dst, t0, t1, t2, t3
        ld1             {v30.16b}, [x6],  16
        ld1             {\t0\().8h,\t1\().8h},   [x2],  32
        mul             v30.16b, v30.16b, v31.16b
        ld1             {\t2\().8h,\t3\().8h},   [x3],  32
        shll            v28.8h, v30.8b,  #8
        shll2           v29.8h, v30.16b, #8
        sub             \t0\().8h,   \t2\().8h,   \t0\().8h
        sub             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqdmulh         \t0\().8h,   \t0\().8h,   v28.8h
        sqdmulh         \t1\().8h,   \t1\().8h,   v29.8h
        add             \t0\().8h,   \t2\().8h,   \t0\().8h
        add             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #4
        sqrshrun2       \dst\().16b, \t1\().8h,   #4
.endm

.macro bidir_fn type
function \type\()_8bpc_neon, export=1
        clz             w4,  w4
.ifc \type, w_avg
        dup             v30.8h, w6
        neg             v30.8h, v30.8h
        shl             v30.8h, v30.8h, #11
.endif
.ifc \type, mask
        movi            v31.16b, #256-2
.endif
        adr             x7,  L(\type\()_tbl)
        sub             w4,  w4,  #24
        \type           v4,  v0,  v1
        ldrh            w4,  [x7, x4, lsl #1]
        \type           v5,  v2,  v3
        sub             x7,  x7,  w4, uxtw
        br              x7
4:
        cmp             w5,  #4
        st1             {v4.s}[0],  [x0], x1
        st1             {v4.s}[1],  [x0], x1
        st1             {v5.s}[0],  [x0], x1
        st1             {v5.s}[1],  [x0], x1
        b.eq            0f
        \type           v6,  v0,  v1
        \type           v7,  v2,  v3
        cmp             w5,  #8
        st1             {v6.s}[0],  [x0], x1
        st1             {v6.s}[1],  [x0], x1
        st1             {v7.s}[0],  [x0], x1
        st1             {v7.s}[1],  [x0], x1
        b.eq            0f
        \type           v4,  v0,  v1
        \type           v5,  v2,  v3
        st1             {v4.s}[0],  [x0], x1
        st1             {v4.s}[1],  [x0], x1
        \type           v6,  v0,  v1
        st1             {v5.s}[0],  [x0], x1
        st1             {v5.s}[1],  [x0], x1
        \type           v7,  v2,  v3
        st1             {v6.s}[0],  [x0], x1
        st1             {v6.s}[1],  [x0], x1
        st1             {v7.s}[0],  [x0], x1
        st1             {v7.s}[1],  [x0], x1
        ret
8:
        st1             {v4.8b},  [x0], x1
        \type           v6,  v0,  v1
        st1             {v5.8b},  [x0], x1
        \type           v7,  v0,  v1
        st1             {v6.8b},  [x0], x1
        subs            w5,  w5,  #4
        st1             {v7.8b},  [x0], x1
        b.le            0f
        \type           v4,  v0,  v1
        \type           v5,  v2,  v3
        b               8b
160:
        trn1            v4.2d,  v4.2d,  v5.2d
16:
        \type\()16      v5, v0, v1, v2, v3
        st1             {v4.16b}, [x0], x1
        \type\()16      v6, v0, v1, v2, v3
        st1             {v5.16b}, [x0], x1
        \type\()16      v7, v0, v1, v2, v3
        st1             {v6.16b}, [x0], x1
        subs            w5,  w5,  #4
        st1             {v7.16b}, [x0], x1
        b.le            0f
        \type\()16      v4, v0, v1, v2, v3
        b               16b
320:
        trn1            v4.2d,  v4.2d,  v5.2d
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
32:
        \type\()16      v5, v0, v1, v2, v3
        \type\()16      v6, v0, v1, v2, v3
        st1             {v4.16b,v5.16b}, [x0], x1
        \type\()16      v7, v0, v1, v2, v3
        subs            w5,  w5,  #2
        st1             {v6.16b,v7.16b}, [x7], x1
        b.le            0f
        \type\()16      v4, v0, v1, v2, v3
        b               32b
640:
        trn1            v4.2d,  v4.2d,  v5.2d
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
64:
        \type\()16      v5,  v0, v1, v2, v3
        \type\()16      v6,  v0, v1, v2, v3
        \type\()16      v7,  v0, v1, v2, v3
        \type\()16      v16, v0, v1, v2, v3
        \type\()16      v17, v0, v1, v2, v3
        st1             {v4.16b,v5.16b,v6.16b,v7.16b}, [x0], x1
        \type\()16      v18, v0, v1, v2, v3
        \type\()16      v19, v0, v1, v2, v3
        subs            w5,  w5,  #2
        st1             {v16.16b,v17.16b,v18.16b,v19.16b}, [x7], x1
        b.le            0f
        \type\()16      v4, v0, v1, v2, v3
        b               64b
1280:
        trn1            v4.2d,  v4.2d,  v5.2d
        add             x7,  x0,  #64
128:
        \type\()16      v5,  v0, v1, v2, v3
        \type\()16      v6,  v0, v1, v2, v3
        \type\()16      v7,  v0, v1, v2, v3
        \type\()16      v16, v0, v1, v2, v3
        \type\()16      v17, v0, v1, v2, v3
        st1             {v4.16b,v5.16b,v6.16b,v7.16b}, [x0], x1
        \type\()16      v18, v0, v1, v2, v3
        \type\()16      v19, v0, v1, v2, v3
        subs            w5,  w5,  #1
        st1             {v16.16b,v17.16b,v18.16b,v19.16b}, [x7], x1
        b.le            0f
        \type\()16      v4, v0, v1, v2, v3
        b               128b
0:
        ret
L(\type\()_tbl):
        .hword L(\type\()_tbl) - 1280b
        .hword L(\type\()_tbl) -  640b
        .hword L(\type\()_tbl) -  320b
        .hword L(\type\()_tbl) -  160b
        .hword L(\type\()_tbl) -    8b
        .hword L(\type\()_tbl) -    4b
endfunc
.endm

bidir_fn avg
bidir_fn w_avg
bidir_fn mask
