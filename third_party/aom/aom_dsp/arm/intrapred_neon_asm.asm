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

;

    EXPORT  |aom_v_predictor_4x4_neon|
    EXPORT  |aom_v_predictor_8x8_neon|
    EXPORT  |aom_v_predictor_16x16_neon|
    EXPORT  |aom_v_predictor_32x32_neon|
    EXPORT  |aom_h_predictor_4x4_neon|
    EXPORT  |aom_h_predictor_8x8_neon|
    EXPORT  |aom_h_predictor_16x16_neon|
    EXPORT  |aom_h_predictor_32x32_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void aom_v_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride,
;                              const uint8_t *above,
;                              const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_v_predictor_4x4_neon| PROC
    vld1.32             {d0[0]}, [r2]
    vst1.32             {d0[0]}, [r0], r1
    vst1.32             {d0[0]}, [r0], r1
    vst1.32             {d0[0]}, [r0], r1
    vst1.32             {d0[0]}, [r0], r1
    bx                  lr
    ENDP                ; |aom_v_predictor_4x4_neon|

;void aom_v_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride,
;                              const uint8_t *above,
;                              const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_v_predictor_8x8_neon| PROC
    vld1.8              {d0}, [r2]
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    vst1.8              {d0}, [r0], r1
    bx                  lr
    ENDP                ; |aom_v_predictor_8x8_neon|

;void aom_v_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride,
;                                const uint8_t *above,
;                                const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_v_predictor_16x16_neon| PROC
    vld1.8              {q0}, [r2]
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    vst1.8              {q0}, [r0], r1
    bx                  lr
    ENDP                ; |aom_v_predictor_16x16_neon|

;void aom_v_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride,
;                                const uint8_t *above,
;                                const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_v_predictor_32x32_neon| PROC
    vld1.8              {q0, q1}, [r2]
    mov                 r2, #2
loop_v
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    vst1.8              {q0, q1}, [r0], r1
    subs                r2, r2, #1
    bgt                 loop_v
    bx                  lr
    ENDP                ; |aom_v_predictor_32x32_neon|

;void aom_h_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride,
;                              const uint8_t *above,
;                              const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_h_predictor_4x4_neon| PROC
    vld1.32             {d1[0]}, [r3]
    vdup.8              d0, d1[0]
    vst1.32             {d0[0]}, [r0], r1
    vdup.8              d0, d1[1]
    vst1.32             {d0[0]}, [r0], r1
    vdup.8              d0, d1[2]
    vst1.32             {d0[0]}, [r0], r1
    vdup.8              d0, d1[3]
    vst1.32             {d0[0]}, [r0], r1
    bx                  lr
    ENDP                ; |aom_h_predictor_4x4_neon|

;void aom_h_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride,
;                              const uint8_t *above,
;                              const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_h_predictor_8x8_neon| PROC
    vld1.64             {d1}, [r3]
    vdup.8              d0, d1[0]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[1]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[2]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[3]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[4]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[5]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[6]
    vst1.64             {d0}, [r0], r1
    vdup.8              d0, d1[7]
    vst1.64             {d0}, [r0], r1
    bx                  lr
    ENDP                ; |aom_h_predictor_8x8_neon|

;void aom_h_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride,
;                                const uint8_t *above,
;                                const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_h_predictor_16x16_neon| PROC
    vld1.8              {q1}, [r3]
    vdup.8              q0, d2[0]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[1]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[2]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[3]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[4]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[5]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[6]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[7]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[0]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[1]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[2]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[3]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[4]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[5]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[6]
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[7]
    vst1.8              {q0}, [r0], r1
    bx                  lr
    ENDP                ; |aom_h_predictor_16x16_neon|

;void aom_h_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride,
;                                const uint8_t *above,
;                                const uint8_t *left)
; r0  uint8_t *dst
; r1  ptrdiff_t y_stride
; r2  const uint8_t *above
; r3  const uint8_t *left

|aom_h_predictor_32x32_neon| PROC
    sub                 r1, r1, #16
    mov                 r2, #2
loop_h
    vld1.8              {q1}, [r3]!
    vdup.8              q0, d2[0]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[1]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[2]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[3]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[4]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[5]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[6]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d2[7]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[0]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[1]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[2]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[3]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[4]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[5]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[6]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    vdup.8              q0, d3[7]
    vst1.8              {q0}, [r0]!
    vst1.8              {q0}, [r0], r1
    subs                r2, r2, #1
    bgt                 loop_h
    bx                  lr
    ENDP                ; |aom_h_predictor_32x32_neon|
