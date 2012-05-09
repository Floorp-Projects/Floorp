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
;void vp8_short_walsh4x4_neon(short *input, short *output, int pitch)
; r0   short *input,
; r1   short *output,
; r2   int pitch
|vp8_short_walsh4x4_neon| PROC

    vld1.16         {d0}, [r0@64], r2   ; load input
    vld1.16         {d1}, [r0@64], r2
    vld1.16         {d2}, [r0@64], r2
    vld1.16         {d3}, [r0@64]

    ;First for-loop
    ;transpose d0, d1, d2, d3. Then, d0=ip[0], d1=ip[1], d2=ip[2], d3=ip[3]
    vtrn.32         d0, d2
    vtrn.32         d1, d3

    vmov.s32        q15, #3             ; add 3 to all values

    vtrn.16         d0, d1
    vtrn.16         d2, d3

    vadd.s16        d4, d0, d2          ; ip[0] + ip[2]
    vadd.s16        d5, d1, d3          ; ip[1] + ip[3]
    vsub.s16        d6, d1, d3          ; ip[1] - ip[3]
    vsub.s16        d7, d0, d2          ; ip[0] - ip[2]

    vshl.s16        d4, d4, #2          ; a1 = (ip[0] + ip[2]) << 2
    vshl.s16        d5, d5, #2          ; d1 = (ip[1] + ip[3]) << 2
    vshl.s16        d6, d6, #2          ; c1 = (ip[1] - ip[3]) << 2
    vceq.s16        d16, d4, #0         ; a1 == 0
    vshl.s16        d7, d7, #2          ; b1 = (ip[0] - ip[2]) << 2

    vadd.s16        d0, d4, d5          ; a1 + d1
    vmvn            d16, d16            ; a1 != 0
    vsub.s16        d3, d4, d5          ; op[3] = a1 - d1
    vadd.s16        d1, d7, d6          ; op[1] = b1 + c1
    vsub.s16        d2, d7, d6          ; op[2] = b1 - c1
    vsub.s16        d0, d0, d16         ; op[0] = a1 + d1 + (a1 != 0)

    ;Second for-loop
    ;transpose d0, d1, d2, d3, Then, d0=ip[0], d1=ip[4], d2=ip[8], d3=ip[12]
    vtrn.32         d1, d3
    vtrn.32         d0, d2
    vtrn.16         d2, d3
    vtrn.16         d0, d1

    vaddl.s16       q8, d0, d2          ; a1 = ip[0]+ip[8]
    vaddl.s16       q9, d1, d3          ; d1 = ip[4]+ip[12]
    vsubl.s16       q10, d1, d3         ; c1 = ip[4]-ip[12]
    vsubl.s16       q11, d0, d2         ; b1 = ip[0]-ip[8]

    vadd.s32        q0, q8, q9          ; a2 = a1 + d1
    vadd.s32        q1, q11, q10        ; b2 = b1 + c1
    vsub.s32        q2, q11, q10        ; c2 = b1 - c1
    vsub.s32        q3, q8, q9          ; d2 = a1 - d1

    vclt.s32        q8, q0, #0
    vclt.s32        q9, q1, #0
    vclt.s32        q10, q2, #0
    vclt.s32        q11, q3, #0

    ; subtract -1 (or 0)
    vsub.s32        q0, q0, q8          ; a2 += a2 < 0
    vsub.s32        q1, q1, q9          ; b2 += b2 < 0
    vsub.s32        q2, q2, q10         ; c2 += c2 < 0
    vsub.s32        q3, q3, q11         ; d2 += d2 < 0

    vadd.s32        q8, q0, q15         ; a2 + 3
    vadd.s32        q9, q1, q15         ; b2 + 3
    vadd.s32        q10, q2, q15        ; c2 + 3
    vadd.s32        q11, q3, q15        ; d2 + 3

    ; vrshrn? would add 1 << 3-1 = 2
    vshrn.s32       d0, q8, #3
    vshrn.s32       d1, q9, #3
    vshrn.s32       d2, q10, #3
    vshrn.s32       d3, q11, #3

    vst1.16         {q0, q1}, [r1@128]

    bx              lr

    ENDP

    END
