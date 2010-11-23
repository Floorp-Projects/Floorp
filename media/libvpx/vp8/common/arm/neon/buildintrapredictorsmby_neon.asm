;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_build_intra_predictors_mby_neon_func|
    EXPORT  |vp8_build_intra_predictors_mby_s_neon_func|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
; r0    unsigned char *y_buffer
; r1    unsigned char *ypred_ptr
; r2    int y_stride
; r3    int mode
; stack int Up
; stack int Left

|vp8_build_intra_predictors_mby_neon_func| PROC
    push            {r4-r8, lr}

    cmp             r3, #0
    beq             case_dc_pred
    cmp             r3, #1
    beq             case_v_pred
    cmp             r3, #2
    beq             case_h_pred
    cmp             r3, #3
    beq             case_tm_pred

case_dc_pred
    ldr             r4, [sp, #24]       ; Up
    ldr             r5, [sp, #28]       ; Left

    ; Default the DC average to 128
    mov             r12, #128
    vdup.u8         q0, r12

    ; Zero out running sum
    mov             r12, #0

    ; compute shift and jump
    adds            r7, r4, r5
    beq             skip_dc_pred_up_left

    ; Load above row, if it exists
    cmp             r4, #0
    beq             skip_dc_pred_up

    sub             r6, r0, r2
    vld1.8          {q1}, [r6]
    vpaddl.u8       q2, q1
    vpaddl.u16      q3, q2
    vpaddl.u32      q4, q3

    vmov.32         r4, d8[0]
    vmov.32         r6, d9[0]

    add             r12, r4, r6

    ; Move back to interger registers

skip_dc_pred_up

    cmp             r5, #0
    beq             skip_dc_pred_left

    sub             r0, r0, #1

    ; Load left row, if it exists
    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0]

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

skip_dc_pred_left
    add             r7, r7, #3          ; Shift
    sub             r4, r7, #1
    mov             r5, #1
    add             r12, r12, r5, lsl r4
    mov             r5, r12, lsr r7     ; expected_dc

    vdup.u8         q0, r5

skip_dc_pred_up_left
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!

    pop             {r4-r8,pc}
case_v_pred
    ; Copy down above row
    sub             r6, r0, r2
    vld1.8          {q0}, [r6]

    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q0}, [r1]!
    pop             {r4-r8,pc}

case_h_pred
    ; Load 4x yleft_col
    sub             r0, r0, #1

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q1}, [r1]!
    vst1.u8         {q2}, [r1]!
    vst1.u8         {q3}, [r1]!

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q1}, [r1]!
    vst1.u8         {q2}, [r1]!
    vst1.u8         {q3}, [r1]!


    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q1}, [r1]!
    vst1.u8         {q2}, [r1]!
    vst1.u8         {q3}, [r1]!

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1]!
    vst1.u8         {q1}, [r1]!
    vst1.u8         {q2}, [r1]!
    vst1.u8         {q3}, [r1]!

    pop             {r4-r8,pc}

case_tm_pred
    ; Load yabove_row
    sub             r3, r0, r2
    vld1.8          {q8}, [r3]

    ; Load ytop_left
    sub             r3, r3, #1
    ldrb            r7, [r3]

    vdup.u16        q7, r7

    ; Compute yabove_row - ytop_left
    mov             r3, #1
    vdup.u8         q0, r3

    vmull.u8        q4, d16, d0
    vmull.u8        q5, d17, d0

    vsub.s16        q4, q4, q7
    vsub.s16        q5, q5, q7

    ; Load 4x yleft_col
    sub             r0, r0, #1
    mov             r12, #4

case_tm_pred_loop
    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u16        q0, r3
    vdup.u16        q1, r4
    vdup.u16        q2, r5
    vdup.u16        q3, r6

    vqadd.s16       q8, q0, q4
    vqadd.s16       q9, q0, q5

    vqadd.s16       q10, q1, q4
    vqadd.s16       q11, q1, q5

    vqadd.s16       q12, q2, q4
    vqadd.s16       q13, q2, q5

    vqadd.s16       q14, q3, q4
    vqadd.s16       q15, q3, q5

    vqshrun.s16     d0, q8, #0
    vqshrun.s16     d1, q9, #0

    vqshrun.s16     d2, q10, #0
    vqshrun.s16     d3, q11, #0

    vqshrun.s16     d4, q12, #0
    vqshrun.s16     d5, q13, #0

    vqshrun.s16     d6, q14, #0
    vqshrun.s16     d7, q15, #0

    vst1.u8         {q0}, [r1]!
    vst1.u8         {q1}, [r1]!
    vst1.u8         {q2}, [r1]!
    vst1.u8         {q3}, [r1]!

    subs            r12, r12, #1
    bne             case_tm_pred_loop

    pop             {r4-r8,pc}

    ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; r0    unsigned char *y_buffer
; r1    unsigned char *ypred_ptr
; r2    int y_stride
; r3    int mode
; stack int Up
; stack int Left

|vp8_build_intra_predictors_mby_s_neon_func| PROC
    push            {r4-r8, lr}

    mov             r1, r0      ;   unsigned char *ypred_ptr = x->dst.y_buffer; //x->Predictor;

    cmp             r3, #0
    beq             case_dc_pred_s
    cmp             r3, #1
    beq             case_v_pred_s
    cmp             r3, #2
    beq             case_h_pred_s
    cmp             r3, #3
    beq             case_tm_pred_s

case_dc_pred_s
    ldr             r4, [sp, #24]       ; Up
    ldr             r5, [sp, #28]       ; Left

    ; Default the DC average to 128
    mov             r12, #128
    vdup.u8         q0, r12

    ; Zero out running sum
    mov             r12, #0

    ; compute shift and jump
    adds            r7, r4, r5
    beq             skip_dc_pred_up_left_s

    ; Load above row, if it exists
    cmp             r4, #0
    beq             skip_dc_pred_up_s

    sub             r6, r0, r2
    vld1.8          {q1}, [r6]
    vpaddl.u8       q2, q1
    vpaddl.u16      q3, q2
    vpaddl.u32      q4, q3

    vmov.32         r4, d8[0]
    vmov.32         r6, d9[0]

    add             r12, r4, r6

    ; Move back to interger registers

skip_dc_pred_up_s

    cmp             r5, #0
    beq             skip_dc_pred_left_s

    sub             r0, r0, #1

    ; Load left row, if it exists
    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0]

    add             r12, r12, r3
    add             r12, r12, r4
    add             r12, r12, r5
    add             r12, r12, r6

skip_dc_pred_left_s
    add             r7, r7, #3          ; Shift
    sub             r4, r7, #1
    mov             r5, #1
    add             r12, r12, r5, lsl r4
    mov             r5, r12, lsr r7     ; expected_dc

    vdup.u8         q0, r5

skip_dc_pred_up_left_s
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2

    pop             {r4-r8,pc}
case_v_pred_s
    ; Copy down above row
    sub             r6, r0, r2
    vld1.8          {q0}, [r6]

    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q0}, [r1], r2
    pop             {r4-r8,pc}

case_h_pred_s
    ; Load 4x yleft_col
    sub             r0, r0, #1

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q1}, [r1], r2
    vst1.u8         {q2}, [r1], r2
    vst1.u8         {q3}, [r1], r2

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q1}, [r1], r2
    vst1.u8         {q2}, [r1], r2
    vst1.u8         {q3}, [r1], r2


    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q1}, [r1], r2
    vst1.u8         {q2}, [r1], r2
    vst1.u8         {q3}, [r1], r2

    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u8         q0, r3
    vdup.u8         q1, r4
    vdup.u8         q2, r5
    vdup.u8         q3, r6
    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q1}, [r1], r2
    vst1.u8         {q2}, [r1], r2
    vst1.u8         {q3}, [r1], r2

    pop             {r4-r8,pc}

case_tm_pred_s
    ; Load yabove_row
    sub             r3, r0, r2
    vld1.8          {q8}, [r3]

    ; Load ytop_left
    sub             r3, r3, #1
    ldrb            r7, [r3]

    vdup.u16        q7, r7

    ; Compute yabove_row - ytop_left
    mov             r3, #1
    vdup.u8         q0, r3

    vmull.u8        q4, d16, d0
    vmull.u8        q5, d17, d0

    vsub.s16        q4, q4, q7
    vsub.s16        q5, q5, q7

    ; Load 4x yleft_col
    sub             r0, r0, #1
    mov             r12, #4

case_tm_pred_loop_s
    ldrb            r3, [r0], r2
    ldrb            r4, [r0], r2
    ldrb            r5, [r0], r2
    ldrb            r6, [r0], r2
    vdup.u16        q0, r3
    vdup.u16        q1, r4
    vdup.u16        q2, r5
    vdup.u16        q3, r6

    vqadd.s16       q8, q0, q4
    vqadd.s16       q9, q0, q5

    vqadd.s16       q10, q1, q4
    vqadd.s16       q11, q1, q5

    vqadd.s16       q12, q2, q4
    vqadd.s16       q13, q2, q5

    vqadd.s16       q14, q3, q4
    vqadd.s16       q15, q3, q5

    vqshrun.s16     d0, q8, #0
    vqshrun.s16     d1, q9, #0

    vqshrun.s16     d2, q10, #0
    vqshrun.s16     d3, q11, #0

    vqshrun.s16     d4, q12, #0
    vqshrun.s16     d5, q13, #0

    vqshrun.s16     d6, q14, #0
    vqshrun.s16     d7, q15, #0

    vst1.u8         {q0}, [r1], r2
    vst1.u8         {q1}, [r1], r2
    vst1.u8         {q2}, [r1], r2
    vst1.u8         {q3}, [r1], r2

    subs            r12, r12, #1
    bne             case_tm_pred_loop_s

    pop             {r4-r8,pc}

    ENDP


    END
