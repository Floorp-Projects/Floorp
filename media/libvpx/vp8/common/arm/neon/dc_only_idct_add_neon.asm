;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;


    EXPORT  |vp8_dc_only_idct_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void vp8_dc_only_idct_add_c(short input_dc, unsigned char *pred_ptr,
;                            int pred_stride, unsigned char *dst_ptr,
;                            int dst_stride)

; r0  input_dc
; r1  pred_ptr
; r2  pred_stride
; r3  dst_ptr
; sp  dst_stride

|vp8_dc_only_idct_add_neon| PROC
    add             r0, r0, #4
    asr             r0, r0, #3
    ldr             r12, [sp]
    vdup.16         q0, r0

    vld1.32         {d2[0]}, [r1], r2
    vld1.32         {d2[1]}, [r1], r2
    vld1.32         {d4[0]}, [r1], r2
    vld1.32         {d4[1]}, [r1]

    vaddw.u8        q1, q0, d2
    vaddw.u8        q2, q0, d4

    vqmovun.s16     d2, q1
    vqmovun.s16     d4, q2

    vst1.32         {d2[0]}, [r3], r12
    vst1.32         {d2[1]}, [r3], r12
    vst1.32         {d4[0]}, [r3], r12
    vst1.32         {d4[1]}, [r3]

    bx              lr

    ENDP

    END
