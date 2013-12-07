;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;


    EXPORT  |vp9_dc_only_idct_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void vp9_dc_only_idct_add_neon(int input_dc, uint8_t *pred_ptr,
;                            uint8_t *dst_ptr, int pitch, int stride)
;
; r0  int input_dc
; r1  uint8_t *pred_ptr
; r2  uint8_t *dst_ptr
; r3  int pitch
; sp  int stride

|vp9_dc_only_idct_add_neon| PROC

    ; generate cospi_16_64 = 11585
    mov              r12, #0x2d00
    add              r12, #0x41

    ; dct_const_round_shift(input_dc * cospi_16_64)
    mul              r0, r0, r12               ; input_dc * cospi_16_64
    add              r0, r0, #0x2000           ; +(1 << ((DCT_CONST_BITS) - 1))
    asr              r0, r0, #14               ; >> DCT_CONST_BITS

    ; dct_const_round_shift(out * cospi_16_64)
    mul              r0, r0, r12               ; out * cospi_16_64
    add              r0, r0, #0x2000           ; +(1 << ((DCT_CONST_BITS) - 1))
    asr              r0, r0, #14               ; >> DCT_CONST_BITS

    ; ROUND_POWER_OF_TWO(out, 4)
    add              r0, r0, #8                ; + (1 <<((4) - 1))
    asr              r0, r0, #4                ; >> 4

    vdup.16         q0, r0;                   ; duplicate a1
    ldr              r12, [sp]                 ; load stride

    vld1.32         {d2[0]}, [r1], r3
    vld1.32         {d2[1]}, [r1], r3
    vld1.32         {d4[0]}, [r1], r3
    vld1.32         {d4[1]}, [r1]

    vaddw.u8        q1, q0, d2                ; a1 + pred_ptr[c]
    vaddw.u8        q2, q0, d4

    vqmovun.s16     d2, q1                    ; clip_pixel
    vqmovun.s16     d4, q2

    vst1.32         {d2[0]}, [r2], r12
    vst1.32         {d2[1]}, [r2], r12
    vst1.32         {d4[0]}, [r2], r12
    vst1.32         {d4[1]}, [r2]

    bx               lr
    ENDP             ; |vp9_dc_only_idct_add_neon|

    END
