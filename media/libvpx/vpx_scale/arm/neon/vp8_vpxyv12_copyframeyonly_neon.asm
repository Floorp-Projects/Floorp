;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_yv12_copy_frame_yonly_neon|
    EXPORT  |vp8_yv12_copy_frame_yonly_no_extend_frame_borders_neon|

    ARM
    REQUIRE8
    PRESERVE8

    INCLUDE asm_com_offsets.asm

    AREA ||.text||, CODE, READONLY, ALIGN=2
;void vpxyv12_copy_frame_yonly(YV12_BUFFER_CONFIG *src_ybc, YV12_BUFFER_CONFIG *dst_ybc);
; Note: this is VP8 function, which has border=32 and 16. Internal y_width and y_height
; are always multiples of 16.

|vp8_yv12_copy_frame_yonly_neon| PROC
    push            {r4 - r11, lr}
    vpush           {d8 - d15}

    ldr             r4, [r0, #yv12_buffer_config_y_height]
    ldr             r5, [r0, #yv12_buffer_config_y_width]
    ldr             r6, [r0, #yv12_buffer_config_y_stride]
    ldr             r7, [r1, #yv12_buffer_config_y_stride]
    ldr             r2, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    ldr             r3, [r1, #yv12_buffer_config_y_buffer]       ;dstptr1

    ; copy two rows at one time
    mov             lr, r4, lsr #1

cp_src_to_dst_height_loop
    mov             r8, r2
    mov             r9, r3
    add             r10, r2, r6
    add             r11, r3, r7
    mov             r12, r5, lsr #7

cp_src_to_dst_width_loop
    vld1.8          {q0, q1}, [r8]!
    vld1.8          {q8, q9}, [r10]!
    vld1.8          {q2, q3}, [r8]!
    vld1.8          {q10, q11}, [r10]!
    vld1.8          {q4, q5}, [r8]!
    vld1.8          {q12, q13}, [r10]!
    vld1.8          {q6, q7}, [r8]!
    vld1.8          {q14, q15}, [r10]!

    subs            r12, r12, #1

    vst1.8          {q0, q1}, [r9]!
    vst1.8          {q8, q9}, [r11]!
    vst1.8          {q2, q3}, [r9]!
    vst1.8          {q10, q11}, [r11]!
    vst1.8          {q4, q5}, [r9]!
    vst1.8          {q12, q13}, [r11]!
    vst1.8          {q6, q7}, [r9]!
    vst1.8          {q14, q15}, [r11]!

    bne             cp_src_to_dst_width_loop

    subs            lr, lr, #1
    add             r2, r2, r6, lsl #1
    add             r3, r3, r7, lsl #1

    bne             cp_src_to_dst_height_loop

    ands            r10, r5, #0x7f                  ;check to see if extra copy is needed
    sub             r11, r5, r10
    ldr             r2, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    ldr             r3, [r1, #yv12_buffer_config_y_buffer]       ;dstptr1
    bne             extra_cp_src_to_dst_width
end_of_cp_src_to_dst


    ;vpxyv12_extend_frame_borders_yonly
    mov             r0, r1
    ;Not need to load y_width, since: y_width = y_stride - 2*border
    ldr             r3, [r0, #yv12_buffer_config_border]
    ldr             r1, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    ldr             r4, [r0, #yv12_buffer_config_y_height]
    ldr             lr, [r0, #yv12_buffer_config_y_stride]

    cmp             r3, #16
    beq             b16_extend_frame_borders

;=======================
b32_extend_frame_borders
;border = 32
;=======================
;Border copy for Y plane
;copy the left and right most columns out
    sub             r5, r1, r3              ;destptr1
    add             r6, r1, lr
    sub             r6, r6, r3, lsl #1      ;destptr2
    sub             r2, r6, #1              ;srcptr2

    ;Do four rows at one time
    mov             r12, r4, lsr #2

copy_left_right_y
    vld1.8          {d0[], d1[]}, [r1], lr
    vld1.8          {d4[], d5[]}, [r2], lr
    vld1.8          {d8[], d9[]}, [r1], lr
    vld1.8          {d12[], d13[]}, [r2], lr
    vld1.8          {d16[], d17[]},  [r1], lr
    vld1.8          {d20[], d21[]}, [r2], lr
    vld1.8          {d24[], d25[]}, [r1], lr
    vld1.8          {d28[], d29[]}, [r2], lr

    vmov            q1, q0
    vmov            q3, q2
    vmov            q5, q4
    vmov            q7, q6
    vmov            q9, q8
    vmov            q11, q10
    vmov            q13, q12
    vmov            q15, q14

    subs            r12, r12, #1

    vst1.8          {q0, q1}, [r5], lr
    vst1.8          {q2, q3}, [r6], lr
    vst1.8          {q4, q5}, [r5], lr
    vst1.8          {q6, q7}, [r6], lr
    vst1.8          {q8, q9}, [r5], lr
    vst1.8          {q10, q11}, [r6], lr
    vst1.8          {q12, q13}, [r5], lr
    vst1.8          {q14, q15}, [r6], lr

    bne             copy_left_right_y

;Now copy the top and bottom source lines into each line of the respective borders
    ldr             r7, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    mul             r8, r3, lr

    mov             r12, lr, lsr #7

    sub             r6, r1, r3              ;destptr2
    sub             r2, r6, lr              ;srcptr2
    sub             r1, r7, r3              ;srcptr1
    sub             r5, r1, r8              ;destptr1

copy_top_bottom_y
    vld1.8          {q0, q1}, [r1]!
    vld1.8          {q8, q9}, [r2]!
    vld1.8          {q2, q3}, [r1]!
    vld1.8          {q10, q11}, [r2]!
    vld1.8          {q4, q5}, [r1]!
    vld1.8          {q12, q13}, [r2]!
    vld1.8          {q6, q7}, [r1]!
    vld1.8          {q14, q15}, [r2]!

    mov             r7, r3

top_bottom_32
    subs            r7, r7, #1

    vst1.8          {q0, q1}, [r5]!
    vst1.8          {q8, q9}, [r6]!
    vst1.8          {q2, q3}, [r5]!
    vst1.8          {q10, q11}, [r6]!
    vst1.8          {q4, q5}, [r5]!
    vst1.8          {q12, q13}, [r6]!
    vst1.8          {q6, q7}, [r5]!
    vst1.8          {q14, q15}, [r6]!

    add             r5, r5, lr
    sub             r5, r5, #128
    add             r6, r6, lr
    sub             r6, r6, #128

    bne             top_bottom_32

    sub             r5, r1, r8
    add             r6, r2, lr

    subs            r12, r12, #1
    bne             copy_top_bottom_y

    mov             r7, lr, lsr #4              ;check to see if extra copy is needed
    ands            r7, r7, #0x7
    bne             extra_top_bottom_y
end_of_border_copy_y

    vpop            {d8 - d15}
    pop             {r4 - r11, pc}

;=====================
;extra copy part for Y
extra_top_bottom_y
    vld1.8          {q0}, [r1]!
    vld1.8          {q2}, [r2]!

    mov             r9, r3, lsr #3

extra_top_bottom_32
    subs            r9, r9, #1

    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    bne             extra_top_bottom_32

    sub             r5, r1, r8
    add             r6, r2, lr
    subs            r7, r7, #1
    bne             extra_top_bottom_y

    b               end_of_border_copy_y


;=======================
b16_extend_frame_borders
;border = 16
;=======================
;Border copy for Y plane
;copy the left and right most columns out
    sub             r5, r1, r3              ;destptr1
    add             r6, r1, lr
    sub             r6, r6, r3, lsl #1      ;destptr2
    sub             r2, r6, #1              ;srcptr2

    ;Do four rows at one time
    mov             r12, r4, lsr #2

copy_left_right_y_b16
    vld1.8          {d0[], d1[]}, [r1], lr
    vld1.8          {d4[], d5[]}, [r2], lr
    vld1.8          {d8[], d9[]}, [r1], lr
    vld1.8          {d12[], d13[]}, [r2], lr
    vld1.8          {d16[], d17[]},  [r1], lr
    vld1.8          {d20[], d21[]}, [r2], lr
    vld1.8          {d24[], d25[]}, [r1], lr
    vld1.8          {d28[], d29[]}, [r2], lr

    subs            r12, r12, #1

    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q4}, [r5], lr
    vst1.8          {q6}, [r6], lr
    vst1.8          {q8}, [r5], lr
    vst1.8          {q10}, [r6], lr
    vst1.8          {q12}, [r5], lr
    vst1.8          {q14}, [r6], lr

    bne             copy_left_right_y_b16

;Now copy the top and bottom source lines into each line of the respective borders
    ldr             r7, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    mul             r8, r3, lr

    mov             r12, lr, lsr #7

    sub             r6, r1, r3              ;destptr2
    sub             r2, r6, lr              ;srcptr2
    sub             r1, r7, r3              ;srcptr1
    sub             r5, r1, r8              ;destptr1

copy_top_bottom_y_b16
    vld1.8          {q0, q1}, [r1]!
    vld1.8          {q8, q9}, [r2]!
    vld1.8          {q2, q3}, [r1]!
    vld1.8          {q10, q11}, [r2]!
    vld1.8          {q4, q5}, [r1]!
    vld1.8          {q12, q13}, [r2]!
    vld1.8          {q6, q7}, [r1]!
    vld1.8          {q14, q15}, [r2]!

    mov             r7, r3

top_bottom_16_b16
    subs            r7, r7, #1

    vst1.8          {q0, q1}, [r5]!
    vst1.8          {q8, q9}, [r6]!
    vst1.8          {q2, q3}, [r5]!
    vst1.8          {q10, q11}, [r6]!
    vst1.8          {q4, q5}, [r5]!
    vst1.8          {q12, q13}, [r6]!
    vst1.8          {q6, q7}, [r5]!
    vst1.8          {q14, q15}, [r6]!

    add             r5, r5, lr
    sub             r5, r5, #128
    add             r6, r6, lr
    sub             r6, r6, #128

    bne             top_bottom_16_b16

    sub             r5, r1, r8
    add             r6, r2, lr

    subs            r12, r12, #1
    bne             copy_top_bottom_y_b16

    mov             r7, lr, lsr #4              ;check to see if extra copy is needed
    ands            r7, r7, #0x7
    bne             extra_top_bottom_y_b16
end_of_border_copy_y_b16

    vpop            {d8 - d15}
    pop             {r4 - r11, pc}

;=====================
;extra copy part for Y
extra_top_bottom_y_b16
    vld1.8          {q0}, [r1]!
    vld1.8          {q2}, [r2]!

    mov             r9, r3, lsr #3

extra_top_bottom_16_b16
    subs            r9, r9, #1

    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    vst1.8          {q0}, [r5], lr
    vst1.8          {q2}, [r6], lr
    bne             extra_top_bottom_16_b16

    sub             r5, r1, r8
    add             r6, r2, lr
    subs            r7, r7, #1
    bne             extra_top_bottom_y_b16

    b               end_of_border_copy_y_b16

;=============================
extra_cp_src_to_dst_width
    add             r2, r2, r11
    add             r3, r3, r11
    add             r0, r8, r6
    add             r11, r9, r7

    mov             lr, r4, lsr #1
extra_cp_src_to_dst_height_loop
    mov             r8, r2
    mov             r9, r3
    add             r0, r8, r6
    add             r11, r9, r7

    mov             r12, r10

extra_cp_src_to_dst_width_loop
    vld1.8          {q0}, [r8]!
    vld1.8          {q1}, [r0]!

    subs            r12, r12, #16

    vst1.8          {q0}, [r9]!
    vst1.8          {q1}, [r11]!
    bne             extra_cp_src_to_dst_width_loop

    subs            lr, lr, #1

    add             r2, r2, r6, lsl #1
    add             r3, r3, r7, lsl #1

    bne             extra_cp_src_to_dst_height_loop

    b               end_of_cp_src_to_dst

    ENDP

;===========================================================
;In vp8cx_pick_filter_level(), call vp8_yv12_copy_frame_yonly
;without extend_frame_borders.
|vp8_yv12_copy_frame_yonly_no_extend_frame_borders_neon| PROC
    push            {r4 - r11, lr}
    vpush           {d8-d15}

    ldr             r4, [r0, #yv12_buffer_config_y_height]
    ldr             r5, [r0, #yv12_buffer_config_y_width]
    ldr             r6, [r0, #yv12_buffer_config_y_stride]
    ldr             r7, [r1, #yv12_buffer_config_y_stride]
    ldr             r2, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    ldr             r3, [r1, #yv12_buffer_config_y_buffer]       ;dstptr1

    ; copy two rows at one time
    mov             lr, r4, lsr #1

cp_src_to_dst_height_loop1
    mov             r8, r2
    mov             r9, r3
    add             r10, r2, r6
    add             r11, r3, r7
    mov             r12, r5, lsr #7

cp_src_to_dst_width_loop1
    vld1.8          {q0, q1}, [r8]!
    vld1.8          {q8, q9}, [r10]!
    vld1.8          {q2, q3}, [r8]!
    vld1.8          {q10, q11}, [r10]!
    vld1.8          {q4, q5}, [r8]!
    vld1.8          {q12, q13}, [r10]!
    vld1.8          {q6, q7}, [r8]!
    vld1.8          {q14, q15}, [r10]!

    subs            r12, r12, #1

    vst1.8          {q0, q1}, [r9]!
    vst1.8          {q8, q9}, [r11]!
    vst1.8          {q2, q3}, [r9]!
    vst1.8          {q10, q11}, [r11]!
    vst1.8          {q4, q5}, [r9]!
    vst1.8          {q12, q13}, [r11]!
    vst1.8          {q6, q7}, [r9]!
    vst1.8          {q14, q15}, [r11]!

    bne             cp_src_to_dst_width_loop1

    subs            lr, lr, #1
    add             r2, r2, r6, lsl #1
    add             r3, r3, r7, lsl #1

    bne             cp_src_to_dst_height_loop1

    ands            r10, r5, #0x7f                  ;check to see if extra copy is needed
    sub             r11, r5, r10
    ldr             r2, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    ldr             r3, [r1, #yv12_buffer_config_y_buffer]       ;dstptr1
    bne             extra_cp_src_to_dst_width1
end_of_cp_src_to_dst1

    vpop            {d8 - d15}
    pop             {r4-r11, pc}

;=============================
extra_cp_src_to_dst_width1
    add             r2, r2, r11
    add             r3, r3, r11
    add             r0, r8, r6
    add             r11, r9, r7

    mov             lr, r4, lsr #1
extra_cp_src_to_dst_height_loop1
    mov             r8, r2
    mov             r9, r3
    add             r0, r8, r6
    add             r11, r9, r7

    mov             r12, r10

extra_cp_src_to_dst_width_loop1
    vld1.8          {q0}, [r8]!
    vld1.8          {q1}, [r0]!

    subs            r12, r12, #16

    vst1.8          {q0}, [r9]!
    vst1.8          {q1}, [r11]!
    bne             extra_cp_src_to_dst_width_loop1

    subs            lr, lr, #1

    add             r2, r2, r6, lsl #1
    add             r3, r3, r7, lsl #1

    bne             extra_cp_src_to_dst_height_loop1

    b               end_of_cp_src_to_dst1

    ENDP

    END
