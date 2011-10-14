;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_short_walsh4x4_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;void vp8_short_walsh4x4_c(short *input, short *output, int pitch)

|vp8_short_walsh4x4_neon| PROC
    vld1.16         {d2}, [r0], r2              ;load input
    vld1.16         {d3}, [r0], r2
    vld1.16         {d4}, [r0], r2
    vld1.16         {d5}, [r0], r2

    ;First for-loop
    ;transpose d2, d3, d4, d5. Then, d2=ip[0], d3=ip[1], d4=ip[2], d5=ip[3]
    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vadd.s16        d6, d2, d5              ;a1 = ip[0]+ip[3]
    vadd.s16        d7, d3, d4              ;b1 = ip[1]+ip[2]
    vsub.s16        d8, d3, d4              ;c1 = ip[1]-ip[2]
    vsub.s16        d9, d2, d5              ;d1 = ip[0]-ip[3]

    vadd.s16        d2, d6, d7             ;op[0] = a1 + b1
    vsub.s16        d4, d6, d7             ;op[2] = a1 - b1
    vadd.s16        d3, d8, d9             ;op[1] = c1 + d1
    vsub.s16        d5, d9, d8             ;op[3] = d1 - c1

    ;Second for-loop
    ;transpose d2, d3, d4, d5. Then, d2=ip[0], d3=ip[4], d4=ip[8], d5=ip[12]
    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vadd.s16        d6, d2, d5              ;a1 = ip[0]+ip[12]
    vadd.s16        d7, d3, d4              ;b1 = ip[4]+ip[8]
    vsub.s16        d8, d3, d4              ;c1 = ip[4]-ip[8]
    vsub.s16        d9, d2, d5              ;d1 = ip[0]-ip[12]

    vadd.s16        d2, d6, d7              ;a2 = a1 + b1;
    vsub.s16        d4, d6, d7              ;c2 = a1 - b1;
    vadd.s16        d3, d8, d9              ;b2 = c1 + d1;
    vsub.s16        d5, d9, d8              ;d2 = d1 - c1;

    vcgt.s16        q3, q1, #0
    vcgt.s16        q4, q2, #0

    vsub.s16        q1, q1, q3
    vsub.s16        q2, q2, q4

    vshr.s16        q1, q1, #1
    vshr.s16        q2, q2, #1

    vst1.16         {q1, q2}, [r1]

    bx              lr

    ENDP

    END
