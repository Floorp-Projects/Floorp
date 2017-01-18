;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;


    EXPORT  |vpx_idct16x16_1_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void vpx_idct16x16_1_add_neon(int16_t *input, uint8_t *dest, int stride)
;
; r0  int16_t input
; r1  uint8_t *dest
; r2  int stride)

|vpx_idct16x16_1_add_neon| PROC
    ldrsh            r0, [r0]

    ; cospi_16_64 = 11585
    movw             r12, #0x2d41

    ; out = dct_const_round_shift(input[0] * cospi_16_64)
    mul              r0, r0, r12               ; input[0] * cospi_16_64
    add              r0, r0, #0x2000           ; +(1 << ((DCT_CONST_BITS) - 1))
    asr              r0, r0, #14               ; >> DCT_CONST_BITS

    ; out = dct_const_round_shift(out * cospi_16_64)
    mul              r0, r0, r12               ; out * cospi_16_64
    mov              r12, r1                   ; save dest
    add              r0, r0, #0x2000           ; +(1 << ((DCT_CONST_BITS) - 1))
    asr              r0, r0, #14               ; >> DCT_CONST_BITS

    ; a1 = ROUND_POWER_OF_TWO(out, 6)
    add              r0, r0, #32               ; + (1 <<((6) - 1))
    asr              r0, r0, #6                ; >> 6

    vdup.s16         q0, r0                    ; duplicate a1
    mov              r0, #8
    sub              r2, #8

    ; load destination data row0 - row3
    vld1.64          {d2}, [r1], r0
    vld1.64          {d3}, [r1], r2
    vld1.64          {d4}, [r1], r0
    vld1.64          {d5}, [r1], r2
    vld1.64          {d6}, [r1], r0
    vld1.64          {d7}, [r1], r2
    vld1.64          {d16}, [r1], r0
    vld1.64          {d17}, [r1], r2

    vaddw.u8         q9, q0, d2                ; dest[x] + a1
    vaddw.u8         q10, q0, d3               ; dest[x] + a1
    vaddw.u8         q11, q0, d4               ; dest[x] + a1
    vaddw.u8         q12, q0, d5               ; dest[x] + a1
    vqmovun.s16      d2, q9                    ; clip_pixel
    vqmovun.s16      d3, q10                   ; clip_pixel
    vqmovun.s16      d30, q11                  ; clip_pixel
    vqmovun.s16      d31, q12                  ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    vaddw.u8         q9, q0, d6                 ; dest[x] + a1
    vaddw.u8         q10, q0, d7                ; dest[x] + a1
    vaddw.u8         q11, q0, d16               ; dest[x] + a1
    vaddw.u8         q12, q0, d17               ; dest[x] + a1
    vqmovun.s16      d2, q9                     ; clip_pixel
    vqmovun.s16      d3, q10                    ; clip_pixel
    vqmovun.s16      d30, q11                   ; clip_pixel
    vqmovun.s16      d31, q12                   ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    ; load destination data row4 - row7
    vld1.64          {d2}, [r1], r0
    vld1.64          {d3}, [r1], r2
    vld1.64          {d4}, [r1], r0
    vld1.64          {d5}, [r1], r2
    vld1.64          {d6}, [r1], r0
    vld1.64          {d7}, [r1], r2
    vld1.64          {d16}, [r1], r0
    vld1.64          {d17}, [r1], r2

    vaddw.u8         q9, q0, d2                ; dest[x] + a1
    vaddw.u8         q10, q0, d3               ; dest[x] + a1
    vaddw.u8         q11, q0, d4               ; dest[x] + a1
    vaddw.u8         q12, q0, d5               ; dest[x] + a1
    vqmovun.s16      d2, q9                    ; clip_pixel
    vqmovun.s16      d3, q10                   ; clip_pixel
    vqmovun.s16      d30, q11                  ; clip_pixel
    vqmovun.s16      d31, q12                  ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    vaddw.u8         q9, q0, d6                 ; dest[x] + a1
    vaddw.u8         q10, q0, d7                ; dest[x] + a1
    vaddw.u8         q11, q0, d16               ; dest[x] + a1
    vaddw.u8         q12, q0, d17               ; dest[x] + a1
    vqmovun.s16      d2, q9                     ; clip_pixel
    vqmovun.s16      d3, q10                    ; clip_pixel
    vqmovun.s16      d30, q11                   ; clip_pixel
    vqmovun.s16      d31, q12                   ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    ; load destination data row8 - row11
    vld1.64          {d2}, [r1], r0
    vld1.64          {d3}, [r1], r2
    vld1.64          {d4}, [r1], r0
    vld1.64          {d5}, [r1], r2
    vld1.64          {d6}, [r1], r0
    vld1.64          {d7}, [r1], r2
    vld1.64          {d16}, [r1], r0
    vld1.64          {d17}, [r1], r2

    vaddw.u8         q9, q0, d2                ; dest[x] + a1
    vaddw.u8         q10, q0, d3               ; dest[x] + a1
    vaddw.u8         q11, q0, d4               ; dest[x] + a1
    vaddw.u8         q12, q0, d5               ; dest[x] + a1
    vqmovun.s16      d2, q9                    ; clip_pixel
    vqmovun.s16      d3, q10                   ; clip_pixel
    vqmovun.s16      d30, q11                  ; clip_pixel
    vqmovun.s16      d31, q12                  ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    vaddw.u8         q9, q0, d6                 ; dest[x] + a1
    vaddw.u8         q10, q0, d7                ; dest[x] + a1
    vaddw.u8         q11, q0, d16               ; dest[x] + a1
    vaddw.u8         q12, q0, d17               ; dest[x] + a1
    vqmovun.s16      d2, q9                     ; clip_pixel
    vqmovun.s16      d3, q10                    ; clip_pixel
    vqmovun.s16      d30, q11                   ; clip_pixel
    vqmovun.s16      d31, q12                   ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    ; load destination data row12 - row15
    vld1.64          {d2}, [r1], r0
    vld1.64          {d3}, [r1], r2
    vld1.64          {d4}, [r1], r0
    vld1.64          {d5}, [r1], r2
    vld1.64          {d6}, [r1], r0
    vld1.64          {d7}, [r1], r2
    vld1.64          {d16}, [r1], r0
    vld1.64          {d17}, [r1], r2

    vaddw.u8         q9, q0, d2                ; dest[x] + a1
    vaddw.u8         q10, q0, d3               ; dest[x] + a1
    vaddw.u8         q11, q0, d4               ; dest[x] + a1
    vaddw.u8         q12, q0, d5               ; dest[x] + a1
    vqmovun.s16      d2, q9                    ; clip_pixel
    vqmovun.s16      d3, q10                   ; clip_pixel
    vqmovun.s16      d30, q11                  ; clip_pixel
    vqmovun.s16      d31, q12                  ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    vaddw.u8         q9, q0, d6                 ; dest[x] + a1
    vaddw.u8         q10, q0, d7                ; dest[x] + a1
    vaddw.u8         q11, q0, d16               ; dest[x] + a1
    vaddw.u8         q12, q0, d17               ; dest[x] + a1
    vqmovun.s16      d2, q9                     ; clip_pixel
    vqmovun.s16      d3, q10                    ; clip_pixel
    vqmovun.s16      d30, q11                   ; clip_pixel
    vqmovun.s16      d31, q12                   ; clip_pixel
    vst1.64          {d2}, [r12], r0
    vst1.64          {d3}, [r12], r2
    vst1.64          {d30}, [r12], r0
    vst1.64          {d31}, [r12], r2

    bx               lr
    ENDP             ; |vpx_idct16x16_1_add_neon|

    END
