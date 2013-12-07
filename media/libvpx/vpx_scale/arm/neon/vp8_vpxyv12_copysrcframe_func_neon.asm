;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_yv12_copy_src_frame_func_neon|
    ARM
    REQUIRE8
    PRESERVE8

    INCLUDE vpx_scale_asm_offsets.asm

    AREA ||.text||, CODE, READONLY, ALIGN=2
;Note: This function is used to copy source data in src_buffer[i] at beginning
;of the encoding. The buffer has a width and height of cpi->oxcf.Width and
;cpi->oxcf.Height, which can be ANY numbers(NOT always multiples of 16 or 4).

;void vp8_yv12_copy_src_frame_func_neon(const YV12_BUFFER_CONFIG *src_ybc,
;                                       YV12_BUFFER_CONFIG *dst_ybc);

|vp8_yv12_copy_src_frame_func_neon| PROC
    push            {r4 - r11, lr}
    vpush           {d8 - d15}

    ;Copy Y plane
    ldr             r4, [r0, #yv12_buffer_config_y_height]
    ldr             r5, [r0, #yv12_buffer_config_y_width]
    ldr             r6, [r0, #yv12_buffer_config_y_stride]
    ldr             r7, [r1, #yv12_buffer_config_y_stride]
    ldr             r2, [r0, #yv12_buffer_config_y_buffer]       ;srcptr1
    ldr             r3, [r1, #yv12_buffer_config_y_buffer]       ;dstptr1

    add             r10, r2, r6             ;second row src
    add             r11, r3, r7             ;second row dst
    mov             r6, r6, lsl #1
    mov             r7, r7, lsl #1
    sub             r6, r6, r5              ;adjust stride
    sub             r7, r7, r5

    ; copy two rows at one time
    mov             lr, r4, lsr #1

cp_src_to_dst_height_loop
    mov             r12, r5

cp_width_128_loop
    vld1.8          {q0, q1}, [r2]!
    vld1.8          {q4, q5}, [r10]!
    vld1.8          {q2, q3}, [r2]!
    vld1.8          {q6, q7}, [r10]!
    vld1.8          {q8, q9}, [r2]!
    vld1.8          {q12, q13}, [r10]!
    vld1.8          {q10, q11}, [r2]!
    vld1.8          {q14, q15}, [r10]!
    sub             r12, r12, #128
    cmp             r12, #128
    vst1.8          {q0, q1}, [r3]!
    vst1.8          {q4, q5}, [r11]!
    vst1.8          {q2, q3}, [r3]!
    vst1.8          {q6, q7}, [r11]!
    vst1.8          {q8, q9}, [r3]!
    vst1.8          {q12, q13}, [r11]!
    vst1.8          {q10, q11}, [r3]!
    vst1.8          {q14, q15}, [r11]!
    bhs             cp_width_128_loop

    cmp             r12, #0
    beq             cp_width_done

cp_width_8_loop
    vld1.8          {d0}, [r2]!
    vld1.8          {d1}, [r10]!
    sub             r12, r12, #8
    cmp             r12, #8
    vst1.8          {d0}, [r3]!
    vst1.8          {d1}, [r11]!
    bhs             cp_width_8_loop

    cmp             r12, #0
    beq             cp_width_done

cp_width_1_loop
    ldrb            r8, [r2], #1
    subs            r12, r12, #1
    strb            r8, [r3], #1
    ldrb            r8, [r10], #1
    strb            r8, [r11], #1
    bne             cp_width_1_loop

cp_width_done
    subs            lr, lr, #1
    add             r2, r2, r6
    add             r3, r3, r7
    add             r10, r10, r6
    add             r11, r11, r7
    bne             cp_src_to_dst_height_loop

;copy last line for Y if y_height is odd
    tst             r4, #1
    beq             cp_width_done_1
    mov             r12, r5

cp_width_128_loop_1
    vld1.8          {q0, q1}, [r2]!
    vld1.8          {q2, q3}, [r2]!
    vld1.8          {q8, q9}, [r2]!
    vld1.8          {q10, q11}, [r2]!
    sub             r12, r12, #128
    cmp             r12, #128
    vst1.8          {q0, q1}, [r3]!
    vst1.8          {q2, q3}, [r3]!
    vst1.8          {q8, q9}, [r3]!
    vst1.8          {q10, q11}, [r3]!
    bhs             cp_width_128_loop_1

    cmp             r12, #0
    beq             cp_width_done_1

cp_width_8_loop_1
    vld1.8          {d0}, [r2]!
    sub             r12, r12, #8
    cmp             r12, #8
    vst1.8          {d0}, [r3]!
    bhs             cp_width_8_loop_1

    cmp             r12, #0
    beq             cp_width_done_1

cp_width_1_loop_1
    ldrb            r8, [r2], #1
    subs            r12, r12, #1
    strb            r8, [r3], #1
    bne             cp_width_1_loop_1
cp_width_done_1

;Copy U & V planes
    ldr             r4, [r0, #yv12_buffer_config_uv_height]
    ldr             r5, [r0, #yv12_buffer_config_uv_width]
    ldr             r6, [r0, #yv12_buffer_config_uv_stride]
    ldr             r7, [r1, #yv12_buffer_config_uv_stride]
    ldr             r2, [r0, #yv12_buffer_config_u_buffer]       ;srcptr1
    ldr             r3, [r1, #yv12_buffer_config_u_buffer]       ;dstptr1

    add             r10, r2, r6             ;second row src
    add             r11, r3, r7             ;second row dst
    mov             r6, r6, lsl #1
    mov             r7, r7, lsl #1
    sub             r6, r6, r5              ;adjust stride
    sub             r7, r7, r5

    mov             r9, #2

cp_uv_loop
    ;copy two rows at one time
    mov             lr, r4, lsr #1

cp_src_to_dst_height_uv_loop
    mov             r12, r5

cp_width_uv_64_loop
    vld1.8          {q0, q1}, [r2]!
    vld1.8          {q4, q5}, [r10]!
    vld1.8          {q2, q3}, [r2]!
    vld1.8          {q6, q7}, [r10]!
    sub             r12, r12, #64
    cmp             r12, #64
    vst1.8          {q0, q1}, [r3]!
    vst1.8          {q4, q5}, [r11]!
    vst1.8          {q2, q3}, [r3]!
    vst1.8          {q6, q7}, [r11]!
    bhs             cp_width_uv_64_loop

    cmp             r12, #0
    beq             cp_width_uv_done

cp_width_uv_8_loop
    vld1.8          {d0}, [r2]!
    vld1.8          {d1}, [r10]!
    sub             r12, r12, #8
    cmp             r12, #8
    vst1.8          {d0}, [r3]!
    vst1.8          {d1}, [r11]!
    bhs             cp_width_uv_8_loop

    cmp             r12, #0
    beq             cp_width_uv_done

cp_width_uv_1_loop
    ldrb            r8, [r2], #1
    subs            r12, r12, #1
    strb            r8, [r3], #1
    ldrb            r8, [r10], #1
    strb            r8, [r11], #1
    bne             cp_width_uv_1_loop

cp_width_uv_done
    subs            lr, lr, #1
    add             r2, r2, r6
    add             r3, r3, r7
    add             r10, r10, r6
    add             r11, r11, r7
    bne             cp_src_to_dst_height_uv_loop

;copy last line for U & V if uv_height is odd
    tst             r4, #1
    beq             cp_width_uv_done_1
    mov             r12, r5

cp_width_uv_64_loop_1
    vld1.8          {q0, q1}, [r2]!
    vld1.8          {q2, q3}, [r2]!
    sub             r12, r12, #64
    cmp             r12, #64
    vst1.8          {q0, q1}, [r3]!
    vst1.8          {q2, q3}, [r3]!
    bhs             cp_width_uv_64_loop_1

    cmp             r12, #0
    beq             cp_width_uv_done_1

cp_width_uv_8_loop_1
    vld1.8          {d0}, [r2]!
    sub             r12, r12, #8
    cmp             r12, #8
    vst1.8          {d0}, [r3]!
    bhs             cp_width_uv_8_loop_1

    cmp             r12, #0
    beq             cp_width_uv_done_1

cp_width_uv_1_loop_1
    ldrb            r8, [r2], #1
    subs            r12, r12, #1
    strb            r8, [r3], #1
    bne             cp_width_uv_1_loop_1
cp_width_uv_done_1

    subs            r9, r9, #1
    ldrne           r2, [r0, #yv12_buffer_config_v_buffer]      ;srcptr1
    ldrne           r3, [r1, #yv12_buffer_config_v_buffer]      ;dstptr1
    ldrne           r10, [r0, #yv12_buffer_config_uv_stride]
    ldrne           r11, [r1, #yv12_buffer_config_uv_stride]

    addne           r10, r2, r10                ;second row src
    addne           r11, r3, r11                ;second row dst

    bne             cp_uv_loop

    vpop            {d8 - d15}
    pop             {r4 - r11, pc}

    ENDP
    END
