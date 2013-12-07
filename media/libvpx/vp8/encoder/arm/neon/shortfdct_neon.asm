;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_short_fdct4x4_neon|
    EXPORT  |vp8_short_fdct8x4_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=4


    ALIGN 16    ; enable use of @128 bit aligned loads
coeff
    DCW      5352,  5352,  5352, 5352
    DCW      2217,  2217,  2217, 2217
    DCD     14500, 14500, 14500, 14500
    DCD      7500,  7500,  7500, 7500
    DCD     12000, 12000, 12000, 12000
    DCD     51000, 51000, 51000, 51000

;void vp8_short_fdct4x4_c(short *input, short *output, int pitch)
|vp8_short_fdct4x4_neon| PROC

    ; Part one
    vld1.16         {d0}, [r0@64], r2
    adr             r12, coeff
    vld1.16         {d1}, [r0@64], r2
    vld1.16         {q8}, [r12@128]!        ; d16=5352,  d17=2217
    vld1.16         {d2}, [r0@64], r2
    vld1.32         {q9, q10}, [r12@128]!   ;  q9=14500, q10=7500
    vld1.16         {d3}, [r0@64], r2

    ; transpose d0=ip[0], d1=ip[1], d2=ip[2], d3=ip[3]
    vtrn.32         d0, d2
    vtrn.32         d1, d3
    vld1.32         {q11,q12}, [r12@128]    ; q11=12000, q12=51000
    vtrn.16         d0, d1
    vtrn.16         d2, d3

    vadd.s16        d4, d0, d3      ; a1 = ip[0] + ip[3]
    vadd.s16        d5, d1, d2      ; b1 = ip[1] + ip[2]
    vsub.s16        d6, d1, d2      ; c1 = ip[1] - ip[2]
    vsub.s16        d7, d0, d3      ; d1 = ip[0] - ip[3]

    vshl.s16        q2, q2, #3      ; (a1, b1) << 3
    vshl.s16        q3, q3, #3      ; (c1, d1) << 3

    vadd.s16        d0, d4, d5      ; op[0] = a1 + b1
    vsub.s16        d2, d4, d5      ; op[2] = a1 - b1

    vmlal.s16       q9, d7, d16     ; d1*5352 + 14500
    vmlal.s16       q10, d7, d17    ; d1*2217 + 7500
    vmlal.s16       q9, d6, d17     ; c1*2217 + d1*5352 + 14500
    vmlsl.s16       q10, d6, d16    ; d1*2217 - c1*5352 + 7500

    vshrn.s32       d1, q9, #12     ; op[1] = (c1*2217 + d1*5352 + 14500)>>12
    vshrn.s32       d3, q10, #12    ; op[3] = (d1*2217 - c1*5352 +  7500)>>12


    ; Part two

    ; transpose d0=ip[0], d1=ip[4], d2=ip[8], d3=ip[12]
    vtrn.32         d0, d2
    vtrn.32         d1, d3
    vtrn.16         d0, d1
    vtrn.16         d2, d3

    vmov.s16        d26, #7

    vadd.s16        d4, d0, d3      ; a1 = ip[0] + ip[12]
    vadd.s16        d5, d1, d2      ; b1 = ip[4] + ip[8]
    vsub.s16        d6, d1, d2      ; c1 = ip[4] - ip[8]
    vadd.s16        d4, d4, d26     ; a1 + 7
    vsub.s16        d7, d0, d3      ; d1 = ip[0] - ip[12]

    vadd.s16        d0, d4, d5      ; op[0] = a1 + b1 + 7
    vsub.s16        d2, d4, d5      ; op[8] = a1 - b1 + 7

    vmlal.s16       q11, d7, d16    ; d1*5352 + 12000
    vmlal.s16       q12, d7, d17    ; d1*2217 + 51000

    vceq.s16        d4, d7, #0

    vshr.s16        d0, d0, #4
    vshr.s16        d2, d2, #4

    vmlal.s16       q11, d6, d17    ; c1*2217 + d1*5352 + 12000
    vmlsl.s16       q12, d6, d16    ; d1*2217 - c1*5352 + 51000

    vmvn            d4, d4
    vshrn.s32       d1, q11, #16    ; op[4] = (c1*2217 + d1*5352 + 12000)>>16
    vsub.s16        d1, d1, d4      ; op[4] += (d1!=0)
    vshrn.s32       d3, q12, #16    ; op[12]= (d1*2217 - c1*5352 + 51000)>>16

    vst1.16         {q0, q1}, [r1@128]

    bx              lr

    ENDP

;void vp8_short_fdct8x4_c(short *input, short *output, int pitch)
|vp8_short_fdct8x4_neon| PROC

    ; Part one

    vld1.16         {q0}, [r0@128], r2
    adr             r12, coeff
    vld1.16         {q1}, [r0@128], r2
    vld1.16         {q8}, [r12@128]!        ; d16=5352,  d17=2217
    vld1.16         {q2}, [r0@128], r2
    vld1.32         {q9, q10}, [r12@128]!   ;  q9=14500, q10=7500
    vld1.16         {q3}, [r0@128], r2

    ; transpose q0=ip[0], q1=ip[1], q2=ip[2], q3=ip[3]
    vtrn.32         q0, q2          ; [A0|B0]
    vtrn.32         q1, q3          ; [A1|B1]
    vtrn.16         q0, q1          ; [A2|B2]
    vtrn.16         q2, q3          ; [A3|B3]

    vadd.s16        q11, q0, q3     ; a1 = ip[0] + ip[3]
    vadd.s16        q12, q1, q2     ; b1 = ip[1] + ip[2]
    vsub.s16        q13, q1, q2     ; c1 = ip[1] - ip[2]
    vsub.s16        q14, q0, q3     ; d1 = ip[0] - ip[3]

    vshl.s16        q11, q11, #3    ; a1 << 3
    vshl.s16        q12, q12, #3    ; b1 << 3
    vshl.s16        q13, q13, #3    ; c1 << 3
    vshl.s16        q14, q14, #3    ; d1 << 3

    vadd.s16        q0, q11, q12    ; [A0 | B0] = a1 + b1
    vsub.s16        q2, q11, q12    ; [A2 | B2] = a1 - b1

    vmov.s16        q11, q9         ; 14500
    vmov.s16        q12, q10        ; 7500

    vmlal.s16       q9, d28, d16    ; A[1] = d1*5352 + 14500
    vmlal.s16       q10, d28, d17   ; A[3] = d1*2217 + 7500
    vmlal.s16       q11, d29, d16   ; B[1] = d1*5352 + 14500
    vmlal.s16       q12, d29, d17   ; B[3] = d1*2217 + 7500

    vmlal.s16       q9, d26, d17    ; A[1] = c1*2217 + d1*5352 + 14500
    vmlsl.s16       q10, d26, d16   ; A[3] = d1*2217 - c1*5352 + 7500
    vmlal.s16       q11, d27, d17   ; B[1] = c1*2217 + d1*5352 + 14500
    vmlsl.s16       q12, d27, d16   ; B[3] = d1*2217 - c1*5352 + 7500

    vshrn.s32       d2, q9, #12     ; A[1] = (c1*2217 + d1*5352 + 14500)>>12
    vshrn.s32       d6, q10, #12    ; A[3] = (d1*2217 - c1*5352 +  7500)>>12
    vshrn.s32       d3, q11, #12    ; B[1] = (c1*2217 + d1*5352 + 14500)>>12
    vshrn.s32       d7, q12, #12    ; B[3] = (d1*2217 - c1*5352 +  7500)>>12


    ; Part two
    vld1.32         {q9,q10}, [r12@128]    ; q9=12000, q10=51000

    ; transpose q0=ip[0], q1=ip[4], q2=ip[8], q3=ip[12]
    vtrn.32         q0, q2          ; q0=[A0 | B0]
    vtrn.32         q1, q3          ; q1=[A4 | B4]
    vtrn.16         q0, q1          ; q2=[A8 | B8]
    vtrn.16         q2, q3          ; q3=[A12|B12]

    vmov.s16        q15, #7

    vadd.s16        q11, q0, q3     ; a1 = ip[0] + ip[12]
    vadd.s16        q12, q1, q2     ; b1 = ip[4] + ip[8]
    vadd.s16        q11, q11, q15   ; a1 + 7
    vsub.s16        q13, q1, q2     ; c1 = ip[4] - ip[8]
    vsub.s16        q14, q0, q3     ; d1 = ip[0] - ip[12]

    vadd.s16        q0, q11, q12    ; a1 + b1 + 7
    vsub.s16        q1, q11, q12    ; a1 - b1 + 7

    vmov.s16        q11, q9         ; 12000
    vmov.s16        q12, q10        ; 51000

    vshr.s16        d0, d0, #4      ; A[0] = (a1 + b1 + 7)>>4
    vshr.s16        d4, d1, #4      ; B[0] = (a1 + b1 + 7)>>4
    vshr.s16        d2, d2, #4      ; A[8] = (a1 + b1 + 7)>>4
    vshr.s16        d6, d3, #4      ; B[8] = (a1 + b1 + 7)>>4


    vmlal.s16       q9, d28, d16    ; A[4]  = d1*5352 + 12000
    vmlal.s16       q10, d28, d17   ; A[12] = d1*2217 + 51000
    vmlal.s16       q11, d29, d16   ; B[4]  = d1*5352 + 12000
    vmlal.s16       q12, d29, d17   ; B[12] = d1*2217 + 51000

    vceq.s16        q14, q14, #0

    vmlal.s16       q9, d26, d17    ; A[4]  = c1*2217 + d1*5352 + 12000
    vmlsl.s16       q10, d26, d16   ; A[12] = d1*2217 - c1*5352 + 51000
    vmlal.s16       q11, d27, d17   ; B[4]  = c1*2217 + d1*5352 + 12000
    vmlsl.s16       q12, d27, d16   ; B[12] = d1*2217 - c1*5352 + 51000

    vmvn            q14, q14

    vshrn.s32       d1, q9, #16     ; A[4] = (c1*2217 + d1*5352 + 12000)>>16
    vshrn.s32       d3, q10, #16    ; A[12]= (d1*2217 - c1*5352 + 51000)>>16
    vsub.s16        d1, d1, d28     ; A[4] += (d1!=0)

    vshrn.s32       d5, q11, #16    ; B[4] = (c1*2217 + d1*5352 + 12000)>>16
    vshrn.s32       d7, q12, #16    ; B[12]= (d1*2217 - c1*5352 + 51000)>>16
    vsub.s16        d5, d5, d29     ; B[4] += (d1!=0)

    vst1.16         {q0, q1}, [r1@128]! ; block A
    vst1.16         {q2, q3}, [r1@128]! ; block B

    bx              lr

    ENDP

    END

