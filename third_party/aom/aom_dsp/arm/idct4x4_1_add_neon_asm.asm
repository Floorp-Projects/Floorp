;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;



    EXPORT  |aom_idct4x4_1_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void aom_idct4x4_1_add_neon(int16_t *input, uint8_t *dest,
;                                  int dest_stride)
;
; r0  int16_t input
; r1  uint8_t *dest
; r2  int dest_stride)

|aom_idct4x4_1_add_neon| PROC
    ldrsh            r0, [r0]

    ; generate cospi_16_64 = 11585
    mov              r12, #0x2d00
    add              r12, #0x41

    ; out = dct_const_round_shift(input[0] * cospi_16_64)
    mul              r0, r0, r12               ; input[0] * cospi_16_64
    add              r0, r0, #0x2000           ; +(1 << ((DCT_CONST_BITS) - 1))
    asr              r0, r0, #14               ; >> DCT_CONST_BITS

    ; out = dct_const_round_shift(out * cospi_16_64)
    mul              r0, r0, r12               ; out * cospi_16_64
    mov              r12, r1                   ; save dest
    add              r0, r0, #0x2000           ; +(1 << ((DCT_CONST_BITS) - 1))
    asr              r0, r0, #14               ; >> DCT_CONST_BITS

    ; a1 = ROUND_POWER_OF_TWO(out, 4)
    add              r0, r0, #8                ; + (1 <<((4) - 1))
    asr              r0, r0, #4                ; >> 4

    vdup.s16         q0, r0                    ; duplicate a1

    vld1.32          {d2[0]}, [r1], r2
    vld1.32          {d2[1]}, [r1], r2
    vld1.32          {d4[0]}, [r1], r2
    vld1.32          {d4[1]}, [r1]

    vaddw.u8         q8, q0, d2                ; dest[x] + a1
    vaddw.u8         q9, q0, d4

    vqmovun.s16      d6, q8                    ; clip_pixel
    vqmovun.s16      d7, q9

    vst1.32          {d6[0]}, [r12], r2
    vst1.32          {d6[1]}, [r12], r2
    vst1.32          {d7[0]}, [r12], r2
    vst1.32          {d7[1]}, [r12]

    bx               lr
    ENDP             ; |aom_idct4x4_1_add_neon|

    END
