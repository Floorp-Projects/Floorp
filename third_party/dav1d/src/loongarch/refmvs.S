/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Loongson Technology Corporation Limited
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

#include "src/loongarch/loongson_asm.S"

/*
static void splat_mv_c(refmvs_block **rr, const refmvs_block *const rmv,
                       const int bx4, const int bw4, int bh4)
*/

function splat_mv_lsx
    vld           vr0,      a1,       0          // 0 1 ... 11 ...
    clz.w         t4,       a3
    vaddi.bu      vr1,      vr0,      0
    addi.w        t4,       t4,       -26
    vextrins.w    vr1,      vr0,      0x30       // 0 1 2 ... 11 0 1 2 3
    la.local      t5,       .SPLAT_LSX_JRTABLE
    vbsrl.v       vr2,      vr1,      4          // 4 5 6 7...11 0 1 2 3 0 0 0 0
    alsl.d        t6,       t4,       t5,     1
    vextrins.w    vr2,      vr0,      0x31       // 4 5 6 7...11 0 1 2 3 4 5 6 7
    ld.h          t7,       t6,       0
    vbsrl.v       vr3,      vr2,      4          // 8 9 10 11 0 1 2 3 4 5 6 7 0 0 0 0
    add.d         t8,       t5,       t7
    alsl.d        a2,       a2,       a2,     1
    vextrins.w    vr3,      vr0,      0x32       // 8 9 10 11 0 1 2 3 4 5 6 7 8 9 10 11
    slli.w        a2,       a2,       2
    jirl          $r0,      t8,       0

.SPLAT_LSX_JRTABLE:
    .hword .SPLAT_W32_LSX - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W16_LSX - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W8_LSX  - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W4_LSX  - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W2_LSX  - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W1_LSX  - .SPLAT_LSX_JRTABLE

.SPLAT_W1_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    fst.d         f1,       t3,       0
    fst.s         f3,       t3,       8
    blt           zero,     a4,       .SPLAT_W1_LSX
    b             .splat_end
.SPLAT_W2_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    vst           vr1,      t3,       0
    fst.d         f2,       t3,       16
    blt           zero,     a4,       .SPLAT_W2_LSX
    b             .splat_end

.SPLAT_W4_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32
    blt           zero,     a4,       .SPLAT_W4_LSX
    b             .splat_end

.SPLAT_W8_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32

    vst           vr1,      t3,       48
    vst           vr2,      t3,       64
    vst           vr3,      t3,       80
    blt           zero,     a4,       .SPLAT_W8_LSX
    b             .splat_end

.SPLAT_W16_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

.rept 2
    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32

    vst           vr1,      t3,       48
    vst           vr2,      t3,       64
    vst           vr3,      t3,       80

    addi.d        t3,       t3,       96
.endr

    blt           zero,     a4,       .SPLAT_W16_LSX
    b             .splat_end

.SPLAT_W32_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

.rept 4
    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32

    vst           vr1,      t3,       48
    vst           vr2,      t3,       64
    vst           vr3,      t3,       80

    addi.d        t3,       t3,       96
.endr

    blt           zero,     a4,       .SPLAT_W32_LSX

.splat_end:
endfunc
