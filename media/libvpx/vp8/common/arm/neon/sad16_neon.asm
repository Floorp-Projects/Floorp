;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_sad16x16_neon|
    EXPORT  |vp8_sad16x8_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int  src_stride
; r2    unsigned char *ref_ptr
; r3    int  ref_stride
|vp8_sad16x16_neon| PROC
;;
    vld1.8          {q0}, [r0], r1
    vld1.8          {q4}, [r2], r3

    vld1.8          {q1}, [r0], r1
    vld1.8          {q5}, [r2], r3

    vabdl.u8        q12, d0, d8
    vabdl.u8        q13, d1, d9

    vld1.8          {q2}, [r0], r1
    vld1.8          {q6}, [r2], r3

    vabal.u8        q12, d2, d10
    vabal.u8        q13, d3, d11

    vld1.8          {q3}, [r0], r1
    vld1.8          {q7}, [r2], r3

    vabal.u8        q12, d4, d12
    vabal.u8        q13, d5, d13

;;
    vld1.8          {q0}, [r0], r1
    vld1.8          {q4}, [r2], r3

    vabal.u8        q12, d6, d14
    vabal.u8        q13, d7, d15

    vld1.8          {q1}, [r0], r1
    vld1.8          {q5}, [r2], r3

    vabal.u8        q12, d0, d8
    vabal.u8        q13, d1, d9

    vld1.8          {q2}, [r0], r1
    vld1.8          {q6}, [r2], r3

    vabal.u8        q12, d2, d10
    vabal.u8        q13, d3, d11

    vld1.8          {q3}, [r0], r1
    vld1.8          {q7}, [r2], r3

    vabal.u8        q12, d4, d12
    vabal.u8        q13, d5, d13

;;
    vld1.8          {q0}, [r0], r1
    vld1.8          {q4}, [r2], r3

    vabal.u8        q12, d6, d14
    vabal.u8        q13, d7, d15

    vld1.8          {q1}, [r0], r1
    vld1.8          {q5}, [r2], r3

    vabal.u8        q12, d0, d8
    vabal.u8        q13, d1, d9

    vld1.8          {q2}, [r0], r1
    vld1.8          {q6}, [r2], r3

    vabal.u8        q12, d2, d10
    vabal.u8        q13, d3, d11

    vld1.8          {q3}, [r0], r1
    vld1.8          {q7}, [r2], r3

    vabal.u8        q12, d4, d12
    vabal.u8        q13, d5, d13

;;
    vld1.8          {q0}, [r0], r1
    vld1.8          {q4}, [r2], r3

    vabal.u8        q12, d6, d14
    vabal.u8        q13, d7, d15

    vld1.8          {q1}, [r0], r1
    vld1.8          {q5}, [r2], r3

    vabal.u8        q12, d0, d8
    vabal.u8        q13, d1, d9

    vld1.8          {q2}, [r0], r1
    vld1.8          {q6}, [r2], r3

    vabal.u8        q12, d2, d10
    vabal.u8        q13, d3, d11

    vld1.8          {q3}, [r0]
    vld1.8          {q7}, [r2]

    vabal.u8        q12, d4, d12
    vabal.u8        q13, d5, d13

    vabal.u8        q12, d6, d14
    vabal.u8        q13, d7, d15

    vadd.u16        q0, q12, q13

    vpaddl.u16      q1, q0
    vpaddl.u32      q0, q1

    vadd.u32        d0, d0, d1

    vmov.32         r0, d0[0]

    bx              lr

    ENDP

;==============================
;unsigned int vp8_sad16x8_c(
;    unsigned char *src_ptr,
;    int  src_stride,
;    unsigned char *ref_ptr,
;    int  ref_stride)
|vp8_sad16x8_neon| PROC
    vld1.8          {q0}, [r0], r1
    vld1.8          {q4}, [r2], r3

    vld1.8          {q1}, [r0], r1
    vld1.8          {q5}, [r2], r3

    vabdl.u8        q12, d0, d8
    vabdl.u8        q13, d1, d9

    vld1.8          {q2}, [r0], r1
    vld1.8          {q6}, [r2], r3

    vabal.u8        q12, d2, d10
    vabal.u8        q13, d3, d11

    vld1.8          {q3}, [r0], r1
    vld1.8          {q7}, [r2], r3

    vabal.u8        q12, d4, d12
    vabal.u8        q13, d5, d13

    vld1.8          {q0}, [r0], r1
    vld1.8          {q4}, [r2], r3

    vabal.u8        q12, d6, d14
    vabal.u8        q13, d7, d15

    vld1.8          {q1}, [r0], r1
    vld1.8          {q5}, [r2], r3

    vabal.u8        q12, d0, d8
    vabal.u8        q13, d1, d9

    vld1.8          {q2}, [r0], r1
    vld1.8          {q6}, [r2], r3

    vabal.u8        q12, d2, d10
    vabal.u8        q13, d3, d11

    vld1.8          {q3}, [r0], r1
    vld1.8          {q7}, [r2], r3

    vabal.u8        q12, d4, d12
    vabal.u8        q13, d5, d13

    vabal.u8        q12, d6, d14
    vabal.u8        q13, d7, d15

    vadd.u16        q0, q12, q13

    vpaddl.u16      q1, q0
    vpaddl.u32      q0, q1

    vadd.u32        d0, d0, d1

    vmov.32         r0, d0[0]

    bx              lr

    ENDP

    END
