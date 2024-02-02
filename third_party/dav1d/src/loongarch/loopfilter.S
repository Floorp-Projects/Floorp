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

.macro FILTER_W4 DIR, TYPE
.ifc \DIR, h
    addi.d           t5,     a0,    -2
    fld.s            f6,     t5,     0  //p1 p0 q0 q1
    fldx.s           f7,     t5,     a1
    alsl.d           t5,     a1,     t5,    1
    fld.s            f8,     t5,     0
    fldx.s           f9,     t5,     a1

    vilvl.b          vr6,    vr7,    vr6
    vilvl.b          vr7,    vr9,    vr8
    vilvl.h          vr6,    vr7,    vr6 //p1p1p1p1
    vbsrl.v          vr7,    vr6,    4   //p0p0p0p0
    vbsrl.v          vr8,    vr7,    4   //q0q0q0q0
    vbsrl.v          vr9,    vr8,    4   //q1q1q1q1
.else
    sub.d            t5,     a0,     a1
    fld.s            f7,     t5,     0
    sub.d            t5,     t5,     a1
    fld.s            f6,     t5,     0
    fld.s            f8,     a0,     0
    fldx.s           f9,     a0,     a1
.endif

    vabsd.bu         vr10,   vr6,    vr7 // (p1 - p0)
    vabsd.bu         vr11,   vr9,    vr8 // (q1 - q0)
    vabsd.bu         vr12,   vr7,    vr8 // (p0 - q0)
    vabsd.bu         vr13,   vr6,    vr9 // (p1 - q1)

    vmax.bu          vr14,   vr10,   vr11
    vsle.bu          vr15,   vr14,   vr4  //abs(p1 - p0) <= I && abs(q1 - q0) <= I
    vsadd.bu         vr16,   vr12,   vr12
    vsrli.b          vr17,   vr13,   1
    vsadd.bu         vr16,   vr16,   vr17 //abs(p0 - q0) * 2 + (abs(p1 - q1) >> 1)
    vsle.bu          vr16,   vr16,   vr3
    vand.v           vr20,   vr15,   vr16 //fm

    vpickve2gr.wu    t5,     vr20,   0
    beqz             t5,     .END_FILTER_\DIR\()\TYPE\()_W4

    vslt.bu          vr16,   vr2,    vr14 //hev

    vsllwil.h.b      vr30,   vr20,   0 //expand fm to w
    vsllwil.w.h      vr30,   vr30,   0

    vsllwil.hu.bu    vr17,   vr6,    0
    vsllwil.hu.bu    vr18,   vr9,    0
    vsub.h           vr17,   vr17,   vr18
    vssrarni.b.h     vr17,   vr17,   0    //f = iclip_diff(p1 - q1)

    vand.v           vr17,   vr17,   vr16
    vsllwil.h.b      vr18,   vr17,   0

    vsllwil.hu.bu    vr10,   vr8,    0
    vsllwil.hu.bu    vr11,   vr7,    0
    vsub.h           vr10,   vr10,   vr11

    vsadd.h          vr11,   vr10,   vr10
    vsadd.h          vr10,   vr10,   vr11 //3 * (q0 - p0)
    vsadd.h          vr10,   vr10,   vr18 //f = iclip_diff(3 * (q0 - p0) + f);
    vssrani.b.h      vr10,   vr10,   0
    vsllwil.h.b      vr10,   vr10,   0

    vaddi.hu         vr11,   vr10,   4
    vaddi.hu         vr12,   vr10,   3
    li.w             t5,     127
    vreplgr2vr.h     vr13,   t5
    vmin.h           vr11,   vr11,   vr13
    vmin.h           vr12,   vr12,   vr13
    vsrai.h          vr11,   vr11,   3 //f1
    vsrai.h          vr12,   vr12,   3 //f2

    vsllwil.hu.bu    vr13,   vr7,    0 //p0
    vsllwil.hu.bu    vr14,   vr8,    0 //q0
    vsadd.h          vr13,   vr13,   vr12
    vssub.h          vr14,   vr14,   vr11
    vssrani.bu.h     vr13,   vr13,   0 //dst-1
    vssrani.bu.h     vr14,   vr14,   0 //dst+0

    vsrari.h         vr15,   vr11,   1 //f
    vsllwil.hu.bu    vr18,   vr6,    0 //p1
    vsllwil.hu.bu    vr19,   vr9,    0 //q1
    vsadd.h          vr18,   vr18,   vr15
    vssub.h          vr19,   vr19,   vr15
    vssrani.bu.h     vr18,   vr18,   0 //dst-2
    vssrani.bu.h     vr19,   vr19,   0 //dst+1
    vbitsel.v        vr26,   vr18,   vr6,    vr16
    vbitsel.v        vr29,   vr19,   vr9,    vr16

    vbitsel.v        vr6,    vr6,    vr26,   vr20
    vbitsel.v        vr7,    vr7,    vr13,   vr20
    vbitsel.v        vr8,    vr8,    vr14,   vr20
    vbitsel.v        vr9,    vr9,    vr29,   vr20

.ifc \DIR, h
    vilvl.b          vr6,    vr7,    vr6
    vilvl.b          vr9,    vr9,    vr8
    vilvl.h          vr6,    vr9,    vr6

    addi.d           t5,     a0,    -2
    vstelm.w         vr6,    t5,     0,      0
    add.d            t5,     t5,     a1
    vstelm.w         vr6,    t5,     0,      1
    add.d            t5,     t5,     a1
    vstelm.w         vr6,    t5,     0,      2
    add.d            t5,     t5,     a1
    vstelm.w         vr6,    t5,     0,      3
.else
    fst.s            f8,     a0,     0
    fstx.s           f9,     a0,     a1
    sub.d            t5,     a0,     a1
    fst.s            f7,     t5,     0
    sub.d            t5,     t5,     a1
    fst.s            f6,     t5,     0
.endif
.END_FILTER_\DIR\()\TYPE\()_W4:
.endm

.macro FILTER_W6 DIR, TYPE
.ifc \DIR, h
    addi.d           t5,     a0,    -3
    fld.d            f6,     t5,     0 //p2 p1 p0 q0 q1 q2
    fldx.d           f7,     t5,     a1
    alsl.d           t5,     a1,     t5,    1
    fld.d            f8,     t5,     0
    fldx.d           f9,     t5,     a1

    vilvl.b          vr6,    vr7,    vr6
    vilvl.b          vr7,    vr9,    vr8
    vilvh.h          vr10,   vr7,    vr6
    vilvl.h          vr6,    vr7,    vr6

    vbsrl.v          vr7,    vr6,    4 //p1
    vbsrl.v          vr8,    vr7,    4 //p0
    vbsrl.v          vr9,    vr8,    4 //q0
    vbsrl.v          vr11,   vr10,   4 //q2
.else
    alsl.d           t5,     a1,     a1,    1
    sub.d            t5,     a0,     t5
    fld.d            f6,     t5,     0
    fldx.d           f7,     t5,     a1
    alsl.d           t5,     a1,     t5,    1
    fld.d            f8,     t5,     0
    fldx.d           f9,     t5,     a1
    alsl.d           t5,     a1,     t5,    1
    fld.d            f10,    t5,     0
    fldx.d           f11,    t5,     a1
.endif

    vabsd.bu         vr12,   vr7,    vr8 //abs(p1-p0)
    vabsd.bu         vr13,   vr10,   vr9 //abs(q1-q0)
    vmax.bu          vr14,   vr12,   vr13
    vslt.bu          vr2,    vr2,    vr14 //hev
    vabsd.bu         vr12,   vr6,    vr7 //abs(p2-p1)
    vmax.bu          vr12,   vr12,   vr14
    vabsd.bu         vr13,   vr11,   vr10 //abs(q2-q1)
    vmax.bu          vr12,   vr12,   vr13
    vsle.bu          vr0,    vr12,   vr4 // <=I

    vabsd.bu         vr13,   vr8,    vr9 //abs(p0-q0)
    vsadd.bu         vr13,   vr13,   vr13
    vabsd.bu         vr15,   vr7,    vr10
    vsrli.b          vr15,   vr15,   1
    vsadd.bu         vr13,   vr13,   vr15
    vsle.bu          vr13,   vr13,   vr3 //abs(p0 - q0) * 2 + (abs(p1 - q1) >> 1) <= E
    vand.v           vr0,    vr0,    vr13 //fm

    vpickve2gr.wu    t5,     vr0,    0
    beqz             t5,     .END_FILTER_\DIR\()\TYPE\()_W6

    vabsd.bu         vr12,   vr6,    vr8 //abs(p2-p0)
    vabsd.bu         vr13,   vr11,   vr9 //abs(q2-q0)
    vmax.bu          vr12,   vr12,   vr14
    vmax.bu          vr12,   vr12,   vr13
    vxor.v           vr13,   vr13,   vr13
    vaddi.bu         vr13,   vr13,   1
    vsle.bu          vr1,    vr12,   vr13 //flat8in

    //6789 10 11 --expand to h
    vsllwil.hu.bu    vr12,   vr6,    0
    vsllwil.hu.bu    vr13,   vr7,    0
    vsllwil.hu.bu    vr14,   vr8,    0
    vsllwil.hu.bu    vr15,   vr9,    0
    vsllwil.hu.bu    vr16,   vr10,   0
    vsllwil.hu.bu    vr17,   vr11,   0

    //dst-2
    vsadd.hu         vr18,   vr12,   vr12
    vsadd.hu         vr18,   vr18,   vr12
    vsadd.hu         vr18,   vr18,   vr13
    vsadd.hu         vr18,   vr18,   vr13
    vsadd.hu         vr18,   vr18,   vr14
    vsadd.hu         vr18,   vr18,   vr14
    vsadd.hu         vr18,   vr18,   vr15

    //dst-1
    vsadd.hu         vr19,   vr18,   vr15
    vsadd.hu         vr19,   vr19,   vr16
    vssub.hu         vr19,   vr19,   vr12
    vssub.hu         vr19,   vr19,   vr12

    //dst+0
    vsadd.hu         vr20,   vr19,   vr17
    vsadd.hu         vr20,   vr20,   vr16
    vssub.hu         vr20,   vr20,   vr12
    vssub.hu         vr20,   vr20,   vr13

    //dst+1
    vsadd.hu         vr21,   vr20,   vr17
    vsadd.hu         vr21,   vr21,   vr17
    vssub.hu         vr21,   vr21,   vr13
    vssub.hu         vr21,   vr21,   vr14

    vsrari.h         vr18,   vr18,   3
    vsrari.h         vr19,   vr19,   3
    vsrari.h         vr20,   vr20,   3
    vsrari.h         vr21,   vr21,   3

    vsub.h           vr22,   vr13,   vr16
    vssrani.b.h      vr22,   vr22,   0
    vand.v           vr22,   vr22,   vr2
    vsllwil.h.b      vr22,   vr22,   0 //f = iclip_diff(p1 - q1);

    vsub.h           vr23,   vr15,   vr14
    vsadd.h          vr24,   vr23,   vr23
    vsadd.h          vr23,   vr23,   vr24
    vsadd.h          vr23,   vr23,   vr22
    vssrani.b.h      vr23,   vr23,   0
    vsllwil.h.b      vr23,   vr23,   0 //f = iclip_diff(3 * (q0 - p0) + f);

    vaddi.hu         vr24,   vr23,   4
    vaddi.hu         vr25,   vr23,   3
    li.w             t5,     127
    vreplgr2vr.h     vr3,    t5
    vmin.h           vr24,   vr24,   vr3
    vmin.h           vr25,   vr25,   vr3
    vsrai.h          vr24,   vr24,   3 //f1
    vsrai.h          vr25,   vr25,   3 //f2

    vsadd.h          vr26,   vr14,   vr25 //dst-1
    vssub.h          vr27,   vr15,   vr24 //dst+0

    vsrari.h         vr24,   vr24,   1
    vsadd.h          vr28,   vr13,   vr24
    vssub.h          vr29,   vr16,   vr24
    vsllwil.h.b      vr2,    vr2,    0
    vbitsel.v        vr28,   vr28,   vr13,   vr2 //dst-2
    vbitsel.v        vr29,   vr29,   vr16,   vr2 //dst+1

    //flat8in
    vsllwil.h.b      vr1,    vr1,    0
    vbitsel.v        vr18,   vr28,   vr18,   vr1
    vbitsel.v        vr19,   vr26,   vr19,   vr1
    vbitsel.v        vr20,   vr27,   vr20,   vr1
    vbitsel.v        vr21,   vr29,   vr21,   vr1

    vssrani.bu.h     vr18,   vr18,   0
    vssrani.bu.h     vr19,   vr19,   0
    vssrani.bu.h     vr20,   vr20,   0
    vssrani.bu.h     vr21,   vr21,   0

    vbitsel.v        vr7,    vr7,    vr18,   vr0 //p1
    vbitsel.v        vr8,    vr8,    vr19,   vr0 //p0
    vbitsel.v        vr9,    vr9,    vr20,   vr0 //q0
    vbitsel.v        vr10,   vr10,   vr21,   vr0 //q1

.ifc \DIR, h
    vilvl.b          vr7,    vr8,    vr7
    vilvl.b          vr9,    vr10,   vr9
    vilvl.h          vr7,    vr9,    vr7

    addi.d           t5,     a0,    -2
    vstelm.w         vr7,    t5,     0,      0
    add.d            t5,     t5,     a1
    vstelm.w         vr7,    t5,     0,      1
    add.d            t5,     t5,     a1
    vstelm.w         vr7,    t5,     0,      2
    add.d            t5,     t5,     a1
    vstelm.w         vr7,    t5,     0,      3
.else
    fst.s            f9,     a0,     0
    fstx.s           f10,    a0,     a1
    sub.d            t5,     a0,     a1
    fst.s            f8,     t5,     0
    sub.d            t5,     t5,     a1
    fst.s            f7,     t5,     0
.endif
.END_FILTER_\DIR\()\TYPE\()_W6:
.endm

.macro FILTER_W8 DIR, TYPE
.ifc \DIR, h
    addi.d           t5,     a0,    -4
    fld.d            f6,     t5,     0 //p3 p2 p1 p0 q0 q1 q2 q3
    fldx.d           f7,     t5,     a1
    alsl.d           t5,     a1,     t5,     1
    fld.d            f8,     t5,     0
    fldx.d           f9,     t5,     a1

    vilvl.b          vr6,    vr7,    vr6
    vilvl.b          vr7,    vr9,    vr8
    vilvh.h          vr10,   vr7,    vr6 //q0
    vilvl.h          vr6,    vr7,    vr6 //p3
    vbsrl.v          vr7,    vr6,    4   //p2
    vbsrl.v          vr8,    vr6,    8   //p1
    vbsrl.v          vr9,    vr6,    12  //p0
    vbsrl.v          vr11,   vr10,   4   //q1
    vbsrl.v          vr12,   vr10,   8   //q2
    vbsrl.v          vr13,   vr10,   12  //q3
.else
    fld.s            f10,    a0,     0
    fldx.s           f11,    a0,     a1
    add.d            t5,     a0,     a1
    fldx.s           f12,    t5,     a1
    add.d            t5,     t5,     a1
    fldx.s           f13,    t5,     a1
    sub.d            t5,     a0,     a1
    fld.s            f9,     t5,     0
    sub.d            t5,     t5,     a1
    fld.s            f8,     t5,     0
    sub.d            t5,     t5,     a1
    fld.s            f7,     t5,     0
    sub.d            t5,     t5,     a1
    fld.s            f6,     t5,     0
.endif

    vabsd.bu         vr14,   vr8,    vr9  //p1-p0
    vabsd.bu         vr15,   vr11,   vr10 //q1-q0
    vabsd.bu         vr16,   vr9,    vr10 //p0-q0
    vabsd.bu         vr17,   vr8,    vr11 //p1-q1
    vabsd.bu         vr18,   vr7,    vr8  //p2-p1
    vabsd.bu         vr19,   vr12,   vr11 //q2-q1
    vabsd.bu         vr20,   vr6,    vr7  //p3-p2
    vabsd.bu         vr21,   vr13,   vr12 //q3-q2

    vmax.bu          vr22,   vr14,   vr15
    vsle.bu          vr23,   vr22,   vr4  //abs(p1 - p0) <= I && abs(q1 - q0) <= I
    vsadd.bu         vr16,   vr16,   vr16
    vsrli.b          vr17,   vr17,   1
    vsadd.bu         vr16,   vr16,   vr17
    vsle.bu          vr16,   vr16,   vr3  //abs(p0 - q0) * 2 + (abs(p1 - q1) >> 1) <= E
    vand.v           vr16,   vr16,   vr23 //fm

    vpickve2gr.wu    t5,     vr16,   0
    beqz             t5,     .END_FILTER_\DIR\()\TYPE\()_W8

    vmax.bu          vr23,   vr18,   vr19
    vmax.bu          vr23,   vr23,   vr20
    vmax.bu          vr23,   vr23,   vr21
    vsle.bu          vr23,   vr23,   vr4
    vand.v           vr16,   vr16,   vr23 //fm

    vabsd.bu         vr17,   vr7,    vr9  //abs(p2-p0)
    vabsd.bu         vr18,   vr12,   vr10 //abs(q2-q0)
    vmax.bu          vr17,   vr17,   vr14
    vmax.bu          vr17,   vr17,   vr15
    vmax.bu          vr17,   vr17,   vr18
    vabsd.bu         vr18,   vr6,    vr9  //abs(p3 - p0)
    vabsd.bu         vr19,   vr13,   vr10 //abs(q3 - q0)
    vmax.bu          vr17,   vr17,   vr18
    vmax.bu          vr17,   vr17,   vr19

    vxor.v           vr5,    vr5,    vr5
    vaddi.bu         vr5,    vr5,    1    //F
    vsle.bu          vr17,   vr17,   vr5  //flat8in

    vsllwil.hu.bu    vr0,    vr6,    0 //p3
    vsllwil.hu.bu    vr1,    vr7,    0 //p2
    vsllwil.hu.bu    vr27,   vr8,    0 //p1
    vsllwil.hu.bu    vr3,    vr9,    0 //p0
    vsllwil.hu.bu    vr4,    vr10,   0 //q0
    vsllwil.hu.bu    vr5,    vr11,   0 //q1
    vsllwil.hu.bu    vr14,   vr12,   0 //q2
    vsllwil.hu.bu    vr15,   vr13,   0 //q3

    vsadd.hu         vr18,   vr0,    vr0  //p3+p3
    vsadd.hu         vr19,   vr15,   vr15 //q3+q3
    vsadd.hu         vr20,   vr0,    vr1  //p3+p2
    vsadd.hu         vr21,   vr1,    vr27 //p2+p1
    vsadd.hu         vr28,   vr27,   vr3  //p1+p0
    vsadd.hu         vr23,   vr3,    vr4  //p0+q0
    vsadd.hu         vr24,   vr4,    vr5  //q0+q1
    vsadd.hu         vr25,   vr5,    vr14 //q1+q2
    vsadd.hu         vr26,   vr14,   vr15 //q2+q3

    // dst-3
    vsadd.hu         vr29,   vr18,   vr20
    vsadd.hu         vr29,   vr29,   vr21
    vsadd.hu         vr29,   vr29,   vr23

    // dst-2
    vsadd.hu         vr30,   vr18,   vr21
    vsadd.hu         vr30,   vr30,   vr28
    vsadd.hu         vr30,   vr30,   vr24

    // dst-1
    vsadd.hu         vr31,   vr20,   vr28
    vsadd.hu         vr31,   vr31,   vr23
    vsadd.hu         vr31,   vr31,   vr25

    // dst+0
    vsadd.hu         vr18,   vr21,   vr23
    vsadd.hu         vr18,   vr18,   vr24
    vsadd.hu         vr18,   vr18,   vr26

    //dst+1
    vsadd.hu         vr20,   vr28,   vr24
    vsadd.hu         vr20,   vr20,   vr25
    vsadd.hu         vr20,   vr20,   vr19

    //dst+2
    vsadd.hu         vr21,   vr23,   vr25
    vsadd.hu         vr21,   vr21,   vr26
    vsadd.hu         vr21,   vr21,   vr19

    vssrarni.bu.h    vr23,   vr29,   3
    vssrarni.bu.h    vr24,   vr30,   3
    vssrarni.bu.h    vr25,   vr31,   3
    vssrarni.bu.h    vr19,   vr18,   3
    vssrarni.bu.h    vr20,   vr20,   3
    vssrarni.bu.h    vr21,   vr21,   3

    // !flat8in
    vslt.bu          vr2,    vr2,    vr22 //hev

    vsub.h           vr30,   vr27,   vr5 //p1-q1
    vssrani.b.h      vr30,   vr30,   0
    vand.v           vr30,   vr30,   vr2
    vsllwil.h.b      vr30,   vr30,   0

    vsub.h           vr31,   vr4,    vr3
    vsadd.h          vr0,    vr31,   vr31
    vsadd.h          vr31,   vr31,   vr0
    vsadd.h          vr31,   vr31,   vr30
    vssrani.b.h      vr31,   vr31,   0
    vsllwil.h.b      vr31,   vr31,   0 //f = iclip_diff(3 * (q0 - p0) + f);

    vaddi.hu         vr14,   vr31,   4
    vaddi.hu         vr15,   vr31,   3
    li.w             t5,     127
    vreplgr2vr.h     vr18,   t5
    vmin.h           vr14,   vr14,   vr18
    vmin.h           vr15,   vr15,   vr18
    vsrai.h          vr14,   vr14,   3 //f1
    vsrai.h          vr15,   vr15,   3 //f2

    vsadd.h          vr3,    vr3,    vr15
    vssub.h          vr4,    vr4,    vr14
    vssrani.bu.h     vr3,    vr3,    0 //dst-1
    vssrani.bu.h     vr4,    vr4,    0 //dst+0

    vsrari.h         vr14,   vr14,   1
    vsadd.h          vr18,   vr27,   vr14
    vssub.h          vr26,   vr5,    vr14
    vssrani.bu.h     vr18,   vr18,   0 //dst-2
    vssrani.bu.h     vr26,   vr26,   0 //dst+1

    vbitsel.v        vr27,   vr18,   vr8,   vr2 //dst-2
    vbitsel.v        vr28,   vr26,   vr11,  vr2 //dst+1

    vbitsel.v        vr23,   vr7,    vr23,  vr17 //dst-3 (p2)
    vbitsel.v        vr24,   vr27,   vr24,  vr17 //dst-2
    vbitsel.v        vr25,   vr3,    vr25,  vr17 //dst-1
    vbitsel.v        vr19,   vr4,    vr19,  vr17 //dst+0
    vbitsel.v        vr20,   vr28,   vr20,  vr17 //dst+1
    vbitsel.v        vr21,   vr12,   vr21,  vr17 //dst+2

    vbitsel.v        vr7,    vr7,    vr23,  vr16 //-3
    vbitsel.v        vr8,    vr8,    vr24,  vr16 //-2
    vbitsel.v        vr9,    vr9,    vr25,  vr16 //-1
    vbitsel.v        vr10,   vr10,   vr19,  vr16 //+0
    vbitsel.v        vr11,   vr11,   vr20,  vr16 //+1
    vbitsel.v        vr12,   vr12,   vr21,  vr16 //+2

.ifc \DIR, h
    vilvl.b          vr6,    vr7,    vr6
    vilvl.b          vr8,    vr9,    vr8
    vilvl.b          vr10,   vr11,   vr10
    vilvl.b          vr12,   vr13,   vr12
    vilvl.h          vr6,    vr8,    vr6  //p3p2p1p0 -- -- --
    vilvl.h          vr10,   vr12,   vr10 //q0q1q2q3 -- -- --
    vilvl.w          vr0,    vr10,   vr6  //p3p2p1p0q0q1q2q3 --
    vilvh.w          vr1,    vr10,   vr6  //--

    addi.d           t5,     a0,     -4
    vstelm.d         vr0,    t5,     0,     0
    add.d            t5,     t5,     a1
    vstelm.d         vr0,    t5,     0,     1
    add.d            t5,     t5,     a1
    vstelm.d         vr1,    t5,     0,     0
    add.d            t5,     t5,     a1
    vstelm.d         vr1,    t5,     0,     1
.else
    alsl.d           t5,     a1,     a1,    1
    sub.d            t5,     a0,     t5
    fst.s            f7,     t5,     0
    fstx.s           f8,     t5,     a1
    add.d            t5,     t5,     a1
    fstx.s           f9,     t5,     a1

    fst.s            f10,    a0,     0
    add.d            t5,     a0,     a1
    fst.s            f11,    t5,     0
    fstx.s           f12,    t5,     a1
.endif
.END_FILTER_\DIR\()\TYPE\()_W8:
.endm

.macro FILTER_W16 DIR, TYPE
.ifc \DIR, h
    addi.d           t5,     a0,    -7
    vld              vr6,    t5,     0 //p6p5p4p3p2p1p0q0 q1q2q3q4q5q6
    vldx             vr7,    t5,     a1
    add.d            t5,     t5,     a1
    vldx             vr8,    t5,     a1
    add.d            t5,     t5,     a1
    vldx             vr9,    t5,     a1

    vilvl.b          vr10,   vr7,    vr6
    vilvh.b          vr11,   vr7,    vr6
    vilvl.b          vr12,   vr9,    vr8
    vilvh.b          vr13,   vr9,    vr8
    vilvl.h          vr6,    vr12,   vr10
    vilvh.h          vr10,   vr12,   vr10 //p2---
    vilvl.h          vr15,   vr13,   vr11 //q1---
    vilvh.h          vr19,   vr13,   vr11

    vbsrl.v          vr7,    vr6,    4    //p5---
    vbsrl.v          vr8,    vr6,    8    //p4---
    vbsrl.v          vr9,    vr6,    12   //p3---
    vbsrl.v          vr12,   vr10,   4    //p1---
    vbsrl.v          vr13,   vr10,   8    //p0---
    vbsrl.v          vr14,   vr10,   12   //q0---
    vbsrl.v          vr16,   vr15,   4    //q2---
    vbsrl.v          vr17,   vr15,   8    //q3---
    vbsrl.v          vr18,   vr15,   12   //q4---
    vbsrl.v          vr20,   vr19,   4    //q6---
.else
    slli.d           t5,     a1,     3
    sub.d            t5,     a0,     t5
    fldx.s           f6,     t5,     a1  //p6
    alsl.d           t5,     a1,     t5,    1
    fld.s            f7,     t5,     0   //p5
    fldx.s           f8,     t5,     a1  //p4
    alsl.d           t5,     a1,     t5,    1
    fld.s            f9,     t5,     0   //p3
    fldx.s           f10,    t5,     a1  //p2
    alsl.d           t5,     a1,     t5,    1
    fld.s            f12,    t5,     0   //p1
    fldx.s           f13,    t5,     a1  //p0
    alsl.d           t5,     a1,     t5,    1
    fld.s            f14,    t5,     0   //q0
    fldx.s           f15,    t5,     a1  //q1
    alsl.d           t5,     a1,     t5,    1
    fld.s            f16,    t5,     0   //q2
    fldx.s           f17,    t5,     a1  //q3
    alsl.d           t5,     a1,     t5,    1
    fld.s            f18,    t5,     0   //q4
    fldx.s           f19,    t5,     a1  //q5
    add.d            t5,     t5,     a1
    fldx.s           f20,    t5,     a1  //q6

    //temp store
    addi.d           sp,     sp,    -96
    fst.d            f7,     sp,     0
    fst.d            f8,     sp,     8
    fst.d            f9,     sp,     16
    fst.d            f10,    sp,     24
    fst.d            f12,    sp,     32
    fst.d            f13,    sp,     40
    fst.d            f14,    sp,     48
    fst.d            f15,    sp,     56
    fst.d            f16,    sp,     64
    fst.d            f17,    sp,     72
    fst.d            f18,    sp,     80
    fst.d            f19,    sp,     88
.endif

    vabsd.bu         vr21,   vr12,   vr13 //abs(p1-p0)
    vabsd.bu         vr22,   vr15,   vr14 //abs(q1-q0)
    vmax.bu          vr0,    vr21,   vr22
    vslt.bu          vr2,    vr2,    vr0  //hev
    vabsd.bu         vr1,    vr10,   vr12 //abs(p2-p1)
    vmax.bu          vr0,    vr0,    vr1
    vabsd.bu         vr1,    vr16,   vr15 //abs(q2-q1)
    vmax.bu          vr0,    vr0,    vr1
    vabsd.bu         vr1,    vr9,    vr10 //abs(p3-p2)
    vmax.bu          vr0,    vr0,    vr1
    vabsd.bu         vr1,    vr17,   vr16 //abs(q3-q2)
    vmax.bu          vr0,    vr0,    vr1
    vsle.bu          vr0,    vr0,    vr4  //vr4 released I
    vabsd.bu         vr1,    vr13,   vr14 //abs(p0-q0)
    vsadd.bu         vr1,    vr1,    vr1
    vabsd.bu         vr4,    vr12,   vr15 //abs(p1-q1)
    vsrli.b          vr4,    vr4,    1
    vsadd.bu         vr1,    vr1,    vr4  //abs(p0 - q0) * 2 + (abs(p1 - q1) >> 1)
    vsle.bu          vr1,    vr1,    vr3  //vr3 released E
    vand.v           vr0,    vr0,    vr1  //fm

    vpickve2gr.wu    t5,     vr0,    0
    beqz             t5,     .END_FILTER_\DIR\()\TYPE\()_W16

    vabsd.bu         vr1,    vr6,    vr13 //abs(p6-p0)
    vabsd.bu         vr4,    vr7,    vr13 //abs(p5-p0)
    vmax.bu          vr1,    vr1,    vr4
    vabsd.bu         vr4,    vr8,    vr13 //abs(p4-p0)
    vmax.bu          vr1,    vr1,    vr4
    vabsd.bu         vr4,    vr18,   vr14 //abs(q4-q0)
    vmax.bu          vr1,    vr1,    vr4
    vabsd.bu         vr4,    vr19,   vr14 //abs(q5-q0)
    vmax.bu          vr1,    vr1,    vr4
    vabsd.bu         vr4,    vr20,   vr14
    vmax.bu          vr1,    vr1,    vr4
    vxor.v           vr5,    vr5,    vr5
    vaddi.bu         vr5,    vr5,    1    //F
    vsle.bu          vr1,    vr1,    vr5  //flat8out

    vabsd.bu         vr3,    vr10,   vr13 //abs(p2-p0)
    vmax.bu          vr3,    vr3,    vr21
    vmax.bu          vr3,    vr3,    vr22
    vabsd.bu         vr4,    vr16,   vr14 //abs(q2-q0)
    vmax.bu          vr3,    vr3,    vr4
    vabsd.bu         vr4,    vr9,    vr13 //abs(p3-p0)
    vmax.bu          vr3,    vr3,    vr4
    vabsd.bu         vr4,    vr17,   vr14 //abs(q3-q0)
    vmax.bu          vr3,    vr3,    vr4
    vsle.bu          vr3,    vr3,    vr5  //flatin released vr5

    vsllwil.hu.bu    vr6,    vr6,    0    //p6
    vsllwil.hu.bu    vr7,    vr7,    0    //p5
    vsllwil.hu.bu    vr8,    vr8,    0    //p4
    vsllwil.hu.bu    vr9,    vr9,    0    //p3
    vsllwil.hu.bu    vr10,   vr10,   0    //p2
    vsllwil.hu.bu    vr12,   vr12,   0    //p1
    vsllwil.hu.bu    vr13,   vr13,   0    //p0
    vsllwil.hu.bu    vr14,   vr14,   0    //q0
    vsllwil.hu.bu    vr15,   vr15,   0    //q1
    vsllwil.hu.bu    vr16,   vr16,   0    //q2
    vsllwil.hu.bu    vr17,   vr17,   0    //q3
    vsllwil.hu.bu    vr18,   vr18,   0    //q4
    vsllwil.hu.bu    vr19,   vr19,   0    //q5
    vsllwil.hu.bu    vr20,   vr20,   0    //q6

    //dst-6
    vslli.w          vr21,   vr6,    3
    vssub.hu         vr21,   vr21,   vr6
    vsadd.hu         vr21,   vr21,   vr7
    vsadd.hu         vr21,   vr21,   vr7
    vsadd.hu         vr21,   vr21,   vr8
    vsadd.hu         vr21,   vr21,   vr8
    vsadd.hu         vr21,   vr21,   vr9
    vsadd.hu         vr21,   vr21,   vr10
    vsadd.hu         vr21,   vr21,   vr12
    vsadd.hu         vr21,   vr21,   vr13
    vsadd.hu         vr21,   vr21,   vr14

    //dst-5
    vsadd.hu         vr22,   vr21,   vr15
    vsadd.hu         vr22,   vr22,   vr9
    vssub.hu         vr22,   vr22,   vr6
    vssub.hu         vr22,   vr22,   vr6

    //dst-4
    vsadd.hu         vr23,   vr22,   vr16
    vsadd.hu         vr23,   vr23,   vr10
    vssub.hu         vr23,   vr23,   vr7
    vssub.hu         vr23,   vr23,   vr6

    //dst-3
    vsadd.hu         vr24,   vr23,   vr12
    vsadd.hu         vr24,   vr24,   vr17
    vssub.hu         vr24,   vr24,   vr6
    vssub.hu         vr24,   vr24,   vr8

    //dst-2
    vsadd.hu         vr25,   vr24,   vr18
    vsadd.hu         vr25,   vr25,   vr13
    vssub.hu         vr25,   vr25,   vr6
    vssub.hu         vr25,   vr25,   vr9

    //dst-1
    vsadd.hu         vr26,   vr25,   vr19
    vsadd.hu         vr26,   vr26,   vr14
    vssub.hu         vr26,   vr26,   vr6
    vssub.hu         vr26,   vr26,   vr10

    //dst+0
    vsadd.hu         vr27,   vr26,   vr20
    vsadd.hu         vr27,   vr27,   vr15
    vssub.hu         vr27,   vr27,   vr6
    vssub.hu         vr27,   vr27,   vr12

    //dst+1
    vsadd.hu         vr28,   vr27,   vr20
    vsadd.hu         vr28,   vr28,   vr16
    vssub.hu         vr28,   vr28,   vr7
    vssub.hu         vr28,   vr28,   vr13

    //dst+2
    vsadd.hu         vr29,   vr28,   vr20
    vsadd.hu         vr29,   vr29,   vr17
    vssub.hu         vr29,   vr29,   vr8
    vssub.hu         vr29,   vr29,   vr14

    //dst+3
    vsadd.hu         vr30,   vr29,   vr20
    vsadd.hu         vr30,   vr30,   vr18
    vssub.hu         vr30,   vr30,   vr9
    vssub.hu         vr30,   vr30,   vr15

    //dst+4
    vsadd.hu         vr31,   vr30,   vr20
    vsadd.hu         vr31,   vr31,   vr19
    vssub.hu         vr31,   vr31,   vr10
    vssub.hu         vr31,   vr31,   vr16

    //dst+5
    vsadd.hu         vr11,   vr31,   vr20
    vsadd.hu         vr11,   vr11,   vr20
    vssub.hu         vr11,   vr11,   vr12
    vssub.hu         vr11,   vr11,   vr17

    vsrari.h         vr21,   vr21,   4
    vsrari.h         vr22,   vr22,   4
    vsrari.h         vr23,   vr23,   4
    vsrari.h         vr24,   vr24,   4
    vsrari.h         vr25,   vr25,   4
    vsrari.h         vr26,   vr26,   4
    vsrari.h         vr27,   vr27,   4
    vsrari.h         vr28,   vr28,   4
    vsrari.h         vr29,   vr29,   4
    vsrari.h         vr30,   vr30,   4
    vsrari.h         vr31,   vr31,   4
    vsrari.h         vr11,   vr11,   4

    vand.v           vr1,    vr1,    vr3
    vsllwil.h.b      vr1,    vr1,    0 //expand to h
    //(flat8out & flat8in)
    vbitsel.v        vr21,   vr7,    vr21,    vr1  //dst-6
    vbitsel.v        vr22,   vr8,    vr22,    vr1  //dst-5
    vbitsel.v        vr23,   vr9,    vr23,    vr1  //dst-4
    vbitsel.v        vr30,   vr17,   vr30,    vr1  //dst+3
    vbitsel.v        vr31,   vr18,   vr31,    vr1  //dst+4
    vbitsel.v        vr11,   vr19,   vr11,    vr1  //dst+5

    //flat8in
    //dst-3
    vslli.h          vr4,    vr9,    1
    vsadd.hu         vr4,    vr4,    vr9 //p3*3
    vsadd.hu         vr4,    vr4,    vr10
    vsadd.hu         vr4,    vr4,    vr10
    vsadd.hu         vr4,    vr4,    vr12
    vsadd.hu         vr4,    vr4,    vr13
    vsadd.hu         vr4,    vr4,    vr14

    //dst-2
    vsadd.hu         vr5,    vr4,    vr12
    vsadd.hu         vr5,    vr5,    vr15
    vssub.hu         vr5,    vr5,    vr9
    vssub.hu         vr5,    vr5,    vr10

    //dst-1
    vsadd.hu         vr18,   vr5,    vr13
    vsadd.hu         vr18,   vr18,   vr16
    vssub.hu         vr18,   vr18,   vr9
    vssub.hu         vr18,   vr18,   vr12

    //dst+0
    vsadd.hu         vr7,    vr18,   vr14
    vsadd.hu         vr7,    vr7,    vr17
    vssub.hu         vr7,    vr7,    vr9
    vssub.hu         vr7,    vr7,    vr13

    //dst+1
    vsadd.hu         vr8,    vr7,    vr15
    vsadd.hu         vr8,    vr8,    vr17
    vssub.hu         vr8,    vr8,    vr10
    vssub.hu         vr8,    vr8,    vr14

    //dst+2
    vsadd.hu         vr9,    vr8,    vr16
    vsadd.hu         vr9,    vr9,    vr17
    vssub.hu         vr9,    vr9,    vr12
    vssub.hu         vr9,    vr9,    vr15

    vsrari.h         vr4,    vr4,    3
    vsrari.h         vr5,    vr5,    3
    vsrari.h         vr18,   vr18,   3
    vsrari.h         vr7,    vr7,    3
    vsrari.h         vr8,    vr8,    3
    vsrari.h         vr9,    vr9,    3

    //flat8out & flat8in
    vbitsel.v        vr24,   vr4,    vr24,    vr1 //dst-3
    vbitsel.v        vr25,   vr5,    vr25,    vr1 //dst-2
    vbitsel.v        vr26,   vr18,   vr26,    vr1 //dst-1
    vbitsel.v        vr27,   vr7,    vr27,    vr1 //dst+0
    vbitsel.v        vr28,   vr8,    vr28,    vr1 //dst+1
    vbitsel.v        vr29,   vr9,    vr29,    vr1 //dst+2

    //!flat8in
    vsub.h           vr17,   vr12,   vr15 //p1-q1
    vsllwil.h.b      vr2,    vr2,    0
    vand.v           vr17,   vr17,   vr2  //&hev
    vssrani.b.h      vr17,   vr17,   0
    vsllwil.h.b      vr17,   vr17,   0

    vsub.h           vr7,    vr14,   vr13
    vsadd.h          vr8,    vr7,    vr7
    vsadd.h          vr7,    vr7,    vr8
    vsadd.h          vr7,    vr7,    vr17
    vssrani.b.h      vr7,    vr7,    0
    vsllwil.h.b      vr17,   vr7,    0  //f = iclip_diff(3 * (q0 - p0) + f);

    vaddi.hu         vr7,    vr17,   4
    vaddi.hu         vr8,    vr17,   3
    li.w             t5,     127
    vreplgr2vr.h     vr9,    t5
    vmin.h           vr7,    vr7,    vr9
    vmin.h           vr8,    vr8,    vr9
    vsrai.h          vr7,    vr7,    3  //f1
    vsrai.h          vr8,    vr8,    3  //f2

    vsadd.h          vr4,    vr13,   vr8  //dst-1
    vssub.h          vr5,    vr14,   vr7  //dst+0

    vsrari.h         vr7,    vr7,    1
    vsadd.h          vr17,   vr12,   vr7
    vssub.h          vr7,    vr15,   vr7
    vbitsel.v        vr17,   vr17,   vr12,    vr2  //dst-2
    vbitsel.v        vr7,    vr7,    vr15,    vr2  //dst+1

    //flat8in or !flat8in
    vsllwil.h.b      vr3,    vr3,    0
    vbitsel.v        vr24,   vr10,   vr24,    vr3  //dst-3
    vbitsel.v        vr25,   vr17,   vr25,    vr3  //dst-2
    vbitsel.v        vr26,   vr4,    vr26,    vr3  //dst-1
    vbitsel.v        vr27,   vr5,    vr27,    vr3  //dst+0
    vbitsel.v        vr28,   vr7,    vr28,    vr3  //dst+1
    vbitsel.v        vr29,   vr16,   vr29,    vr3  //dst+2

.ifc \DIR, h
    //dst-6,dst-2,dst-5,dst-1
    vssrani.bu.h     vr25,   vr21,   0
    vssrani.bu.h     vr26,   vr22,   0
    vpermi.w         vr25,   vr25,   0xd8
    vpermi.w         vr26,   vr26,   0xd8
    vilvl.b          vr6,    vr26,   vr25 //65656565 21212121

    //dst-4,dst+0,dst-3,dst+1
    vssrani.bu.h     vr27,   vr23,   0
    vssrani.bu.h     vr28,   vr24,   0
    vpermi.w         vr27,   vr27,   0xd8
    vpermi.w         vr28,   vr28,   0xd8
    vilvl.b          vr26,   vr28,   vr27 //43434343 01010101

    vilvl.h          vr21,   vr26,   vr6  //6543 -- -- --
    vilvh.h          vr22,   vr26,   vr6  //2101 -- -- --
    vilvl.w          vr20,   vr22,   vr21 //65432101 --
    vilvh.w          vr22,   vr22,   vr21 //65432101 --
    vreplvei.d       vr21,   vr20,   1
    vreplvei.d       vr23,   vr22,   1

    //dst+2,dst+4,dst+3,dst+5
    vssrani.bu.h     vr31,   vr29,   0
    vssrani.bu.h     vr11,   vr30,   0
    vpermi.w         vr31,   vr31,   0xd8
    vpermi.w         vr11,   vr11,   0xd8
    vilvl.b          vr11,   vr11,   vr31 //23232323 45454545
    vshuf4i.w        vr11,   vr11,   0xd8
    vshuf4i.h        vr11,   vr11,   0xd8 //2345 -- -- --

    vextrins.w       vr20,   vr11,   0x20
    vextrins.w       vr21,   vr11,   0x21
    vextrins.w       vr22,   vr11,   0x22
    vextrins.w       vr23,   vr11,   0x23

    addi.d           t5,     a0,     -6
    vld              vr6,    t5,     0  //p6p5p4p3p2p1p0q0 q1q2q3q4q5q6
    vldx             vr7,    t5,     a1
    add.d            t5,     t5,     a1
    vldx             vr8,    t5,     a1
    add.d            t5,     t5,     a1
    vldx             vr9,    t5,     a1

    //expand fm to 128
    vreplvei.b       vr10,   vr0,    0
    vreplvei.b       vr11,   vr0,    1
    vreplvei.b       vr12,   vr0,    2
    vreplvei.b       vr13,   vr0,    3

    vbitsel.v        vr20,   vr6,    vr20,    vr10
    vbitsel.v        vr21,   vr7,    vr21,    vr11
    vbitsel.v        vr22,   vr8,    vr22,    vr12
    vbitsel.v        vr23,   vr9,    vr23,    vr13

    addi.d           t5,     a0,    -6
    vstelm.d         vr20,   t5,     0,       0
    vstelm.w         vr20,   t5,     8,       2
    add.d            t5,     t5,     a1
    vstelm.d         vr21,   t5,     0,       0
    vstelm.w         vr21,   t5,     8,       2
    add.d            t5,     t5,     a1
    vstelm.d         vr22,   t5,     0,       0
    vstelm.w         vr22,   t5,     8,       2
    add.d            t5,     t5,     a1
    vstelm.d         vr23,   t5,     0,       0
    vstelm.w         vr23,   t5,     8,       2
.else
    //reload
    fld.d            f7,     sp,     0
    fld.d            f8,     sp,     8
    fld.d            f9,     sp,     16
    fld.d            f10,    sp,     24
    fld.d            f12,    sp,     32
    fld.d            f13,    sp,     40
    fld.d            f14,    sp,     48
    fld.d            f15,    sp,     56
    fld.d            f16,    sp,     64
    fld.d            f17,    sp,     72
    fld.d            f18,    sp,     80
    fld.d            f19,    sp,     88

    vssrarni.bu.h    vr21,   vr21,   0
    vssrarni.bu.h    vr22,   vr22,   0
    vssrarni.bu.h    vr23,   vr23,   0
    vssrarni.bu.h    vr24,   vr24,   0
    vssrarni.bu.h    vr25,   vr25,   0
    vssrarni.bu.h    vr26,   vr26,   0
    vssrarni.bu.h    vr27,   vr27,   0
    vssrarni.bu.h    vr28,   vr28,   0
    vssrarni.bu.h    vr29,   vr29,   0
    vssrarni.bu.h    vr30,   vr30,   0
    vssrarni.bu.h    vr31,   vr31,   0
    vssrarni.bu.h    vr11,   vr11,   0

    vbitsel.v        vr7,    vr7,    vr21,   vr0 //p5
    vbitsel.v        vr8,    vr8,    vr22,   vr0 //p4
    vbitsel.v        vr9,    vr9,    vr23,   vr0 //p3
    vbitsel.v        vr10,   vr10,   vr24,   vr0 //p2
    vbitsel.v        vr12,   vr12,   vr25,   vr0 //p1
    vbitsel.v        vr13,   vr13,   vr26,   vr0 //p0
    vbitsel.v        vr14,   vr14,   vr27,   vr0 //q0
    vbitsel.v        vr15,   vr15,   vr28,   vr0 //q1
    vbitsel.v        vr16,   vr16,   vr29,   vr0 //q2
    vbitsel.v        vr17,   vr17,   vr30,   vr0 //q3
    vbitsel.v        vr18,   vr18,   vr31,   vr0 //q4
    vbitsel.v        vr19,   vr19,   vr11,   vr0 //q5

    fst.s            f14,    a0,     0
    fstx.s           f15,    a0,     a1
    alsl.d           t5,     a1,     a0,     1
    fst.s            f16,    t5,     0
    fstx.s           f17,    t5,     a1
    alsl.d           t5,     a1,     t5,     1
    fst.s            f18,    t5,     0
    fstx.s           f19,    t5,     a1

    slli.w           t5,     a1,     2
    alsl.d           t5,     a1,     t5,     1
    sub.d            t5,     a0,     t5
    fst.s            f7,     t5,     0
    fstx.s           f8,     t5,     a1
    alsl.d           t5,     a1,     t5,     1
    fst.s            f9,     t5,     0
    fstx.s           f10,    t5,     a1
    alsl.d           t5,     a1,     t5,     1
    fst.s            f12,    t5,     0
    fstx.s           f13,    t5,     a1
.endif
.END_FILTER_\DIR\()\TYPE\()_W16:
.ifc \DIR, v
    addi.d           sp,     sp,     96
.endif
.endm

.macro PUSH_REG
    addi.d           sp,     sp,    -64
    fst.d            f24,    sp,     0
    fst.d            f25,    sp,     8
    fst.d            f26,    sp,     16
    fst.d            f27,    sp,     24
    fst.d            f28,    sp,     32
    fst.d            f29,    sp,     40
    fst.d            f30,    sp,     48
    fst.d            f31,    sp,     56
.endm
.macro POP_REG
    fld.d            f24,    sp,     0
    fld.d            f25,    sp,     8
    fld.d            f26,    sp,     16
    fld.d            f27,    sp,     24
    fld.d            f28,    sp,     32
    fld.d            f29,    sp,     40
    fld.d            f30,    sp,     48
    fld.d            f31,    sp,     56
    addi.d           sp,     sp,     64
.endm

.macro LPF_FUNC DIR, TYPE
function lpf_\DIR\()_sb_\TYPE\()_8bpc_lsx
    PUSH_REG
    vld              vr0,    a2,     0 //vmask
    vpickve2gr.wu    t0,     vr0,    0
    vpickve2gr.wu    t1,     vr0,    1
    vpickve2gr.wu    t2,     vr0,    2
    li.w             t3,     1          //y
    or               t0,     t0,     t1
.ifc \TYPE, y
    or               t0,     t0,     t2 //vm
.endif
    addi.w           t8,     t3,    -1
    andn             t8,     t0,     t8
    beqz             t0,     .\DIR\()\TYPE\()_END
.\DIR\()\TYPE\()_LOOP:
    and              t4,     t0,     t3 //vm & y
    beqz             t4,     .\DIR\()\TYPE\()_LOOP_NEXT
    vldrepl.b        vr1,    a3,     0 //l[0][0]
.ifc \DIR, h
    addi.d           t5,     a3,    -4
.else
    slli.d           t5,     a4,     2
    sub.d            t5,     a3,     t5
.endif
    vldrepl.b        vr2,    t5,     0 //l[-1][0]
    vseqi.b          vr3,    vr1,    0
    vbitsel.v        vr1,    vr1,    vr2,    vr3 //L
    vpickve2gr.b     t5,     vr1,    0
    beqz             t5,     .\DIR\()\TYPE\()_LOOP_NEXT
    vsrai.b          vr2,    vr1,    4 //H
    add.d            t6,     a5,     t5
    vldrepl.b        vr3,    t6,     0 //E
    addi.d           t6,     t6,     64
    vldrepl.b        vr4,    t6,     0 //I
.ifc \TYPE, y
    and              t5,     t2,     t3
    bnez             t5,     .FILTER_\DIR\()\TYPE\()_16
.endif
    and              t5,     t1,     t3
.ifc \TYPE, y
    bnez             t5,     .FILTER_\DIR\()\TYPE\()_8
.else
    bnez             t5,     .FILTER_\DIR\()\TYPE\()_6
.endif
    FILTER_W4 \DIR, \TYPE
    b                .\DIR\()\TYPE\()_LOOP_NEXT
.ifc \TYPE, uv
.FILTER_\DIR\()\TYPE\()_6:
    FILTER_W6 \DIR, \TYPE
.endif
.ifc \TYPE, y
.FILTER_\DIR\()\TYPE\()_8:
    FILTER_W8 \DIR, \TYPE
    b                .\DIR\()\TYPE\()_LOOP_NEXT
.FILTER_\DIR\()\TYPE\()_16:
    FILTER_W16 \DIR, \TYPE
.endif
.\DIR\()\TYPE\()_LOOP_NEXT:
    slli.w           t3,     t3,     1
.ifc \DIR, h
    alsl.d           a0,     a1,     a0,    2
    slli.w           t8,     a4,     2
    add.d            a3,     a3,     t8
.else
    addi.d           a0,     a0,     4
    addi.d           a3,     a3,     4
.endif
    addi.w           t8,     t3,    -1
    andn             t8,     t0,     t8
    bnez             t8,     .\DIR\()\TYPE\()_LOOP
.\DIR\()\TYPE\()_END:
    POP_REG
endfunc
.endm

LPF_FUNC h, y
LPF_FUNC v, y
LPF_FUNC h, uv
LPF_FUNC v, uv
