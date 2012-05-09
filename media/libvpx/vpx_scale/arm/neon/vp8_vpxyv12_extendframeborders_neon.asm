;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_yv12_extend_frame_borders_neon|
    ARM
    REQUIRE8
    PRESERVE8

    INCLUDE asm_com_offsets.asm

    AREA ||.text||, CODE, READONLY, ALIGN=2
;void vp8_yv12_extend_frame_borders_neon (YV12_BUFFER_CONFIG *ybf);
; we depend on VP8BORDERINPIXELS being 32

|vp8_yv12_extend_frame_borders_neon| PROC
    push            {r4 - r10, lr}
    vpush           {d8 - d15}

    ; Border = 32
    ldr             r3, [r0, #yv12_buffer_config_y_width]  ; plane_width
    ldr             r1, [r0, #yv12_buffer_config_y_buffer] ; src_ptr1
    ldr             r4, [r0, #yv12_buffer_config_y_height] ; plane_height
    ldr             lr, [r0, #yv12_buffer_config_y_stride] ; plane_stride

; Border copy for Y plane
; copy the left and right most columns out
    add             r6, r1, r3              ; dest_ptr2 = src_ptr2 + 1 (src_ptr1 + plane_width)
    sub             r2, r6, #1              ; src_ptr2 = src_ptr1 + plane_width - 1
    sub             r5, r1, #32             ; dest_ptr1 = src_ptr1 - Border

    mov             r12, r4, lsr #2         ; plane_height / 4

copy_left_right_y
    vld1.8          {d0[], d1[]}, [r1], lr
    vld1.8          {d4[], d5[]}, [r2], lr
    vld1.8          {d8[], d9[]}, [r1], lr
    vld1.8          {d12[], d13[]}, [r2], lr
    vld1.8          {d16[], d17[]}, [r1], lr
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
    ldr             r1, [r0, #yv12_buffer_config_y_buffer] ; y_buffer
    mul             r8, r4, lr              ; plane_height * plane_stride

    ; copy width is plane_stride
    movs            r12, lr, lsr #7         ; plane_stride / 128

    sub             r1, r1, #32             ; src_ptr1 = y_buffer - Border
    add             r6, r1, r8              ; dest_ptr2 = src_ptr2 - plane_stride (src_ptr1 + (plane_height * plane_stride))
    sub             r2, r6, lr              ; src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride
    sub             r5, r1, lr, asl #5      ; dest_ptr1 = src_ptr1 - (Border * plane_stride)
    ble             extra_y_copy_needed     ; plane stride < 128

copy_top_bottom_y
    vld1.8          {q0, q1}, [r1]!
    vld1.8          {q8, q9}, [r2]!
    vld1.8          {q2, q3}, [r1]!
    vld1.8          {q10, q11}, [r2]!
    vld1.8          {q4, q5}, [r1]!
    vld1.8          {q12, q13}, [r2]!
    vld1.8          {q6, q7}, [r1]!
    vld1.8          {q14, q15}, [r2]!

    mov             r7, #32                 ; Border

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

    add             r5, r5, lr              ; dest_ptr1 += plane_stride
    sub             r5, r5, #128            ; dest_ptr1 -= 128
    add             r6, r6, lr              ; dest_ptr2 += plane_stride
    sub             r6, r6, #128            ; dest_ptr2 -= 128

    bne             top_bottom_32

    sub             r5, r1, lr, asl #5      ; src_ptr1 - (Border* plane_stride)
    add             r6, r2, lr              ; src_ptr2 + plane_stride

    subs            r12, r12, #1
    bne             copy_top_bottom_y

extra_y_copy_needed
    mov             r7, lr, lsr #4          ; check to see if extra copy is needed
    ands            r7, r7, #0x7
    bne             extra_top_bottom_y
end_of_border_copy_y

;Border copy for U, V planes
; Border = 16
    ldr             r7, [r0, #yv12_buffer_config_u_buffer]  ; src_ptr1
    ldr             lr, [r0, #yv12_buffer_config_uv_stride] ; plane_stride
    ldr             r3, [r0, #yv12_buffer_config_uv_width]  ; plane_width
    ldr             r4, [r0, #yv12_buffer_config_uv_height] ; plane_height

    mov             r10, #2

;copy the left and right most columns out
border_copy_uv
    mov             r1, r7                  ; src_ptr1 needs to be saved for second half of loop
    sub             r5, r1, #16             ; dest_ptr1 = src_ptr1 - Border
    add             r6, r1, r3              ; dest_ptr2 = src_ptr2 + 1 (src_ptr1 + plane_width)
    sub             r2, r6, #1              ; src_ptr2 = src_ptr1 + plane_width - 1

    mov             r12, r4, lsr #3         ; plane_height / 8

copy_left_right_uv
    vld1.8          {d0[], d1[]}, [r1], lr
    vld1.8          {d2[], d3[]}, [r2], lr
    vld1.8          {d4[], d5[]}, [r1], lr
    vld1.8          {d6[], d7[]}, [r2], lr
    vld1.8          {d8[], d9[]},  [r1], lr
    vld1.8          {d10[], d11[]}, [r2], lr
    vld1.8          {d12[], d13[]}, [r1], lr
    vld1.8          {d14[], d15[]}, [r2], lr
    vld1.8          {d16[], d17[]}, [r1], lr
    vld1.8          {d18[], d19[]}, [r2], lr
    vld1.8          {d20[], d21[]}, [r1], lr
    vld1.8          {d22[], d23[]}, [r2], lr
    vld1.8          {d24[], d25[]}, [r1], lr
    vld1.8          {d26[], d27[]}, [r2], lr
    vld1.8          {d28[], d29[]}, [r1], lr
    vld1.8          {d30[], d31[]}, [r2], lr

    subs            r12, r12, #1

    vst1.8          {q0}, [r5], lr
    vst1.8          {q1}, [r6], lr
    vst1.8          {q2}, [r5], lr
    vst1.8          {q3}, [r6], lr
    vst1.8          {q4}, [r5], lr
    vst1.8          {q5}, [r6], lr
    vst1.8          {q6}, [r5], lr
    vst1.8          {q7}, [r6], lr
    vst1.8          {q8}, [r5], lr
    vst1.8          {q9}, [r6], lr
    vst1.8          {q10}, [r5], lr
    vst1.8          {q11}, [r6], lr
    vst1.8          {q12}, [r5], lr
    vst1.8          {q13}, [r6], lr
    vst1.8          {q14}, [r5], lr
    vst1.8          {q15}, [r6], lr

    bne             copy_left_right_uv

;Now copy the top and bottom source lines into each line of the respective borders
    mov             r1, r7
    mul             r8, r4, lr              ; plane_height * plane_stride
    movs            r12, lr, lsr #6         ; plane_stride / 64

    sub             r1, r1, #16             ; src_ptr1 = u_buffer - Border
    add             r6, r1, r8              ; dest_ptr2 = src_ptr2 + plane_stride (src_ptr1 + (plane_height * plane_stride)
    sub             r2, r6, lr              ; src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride
    sub             r5, r1, lr, asl #4      ; dest_ptr1 = src_ptr1 - (Border * plane_stride)
    ble             extra_uv_copy_needed    ; plane_stride < 64

copy_top_bottom_uv
    vld1.8          {q0, q1}, [r1]!
    vld1.8          {q8, q9}, [r2]!
    vld1.8          {q2, q3}, [r1]!
    vld1.8          {q10, q11}, [r2]!

    mov             r7, #16                 ; Border

top_bottom_16
    subs            r7, r7, #1

    vst1.8          {q0, q1}, [r5]!
    vst1.8          {q8, q9}, [r6]!
    vst1.8          {q2, q3}, [r5]!
    vst1.8          {q10, q11}, [r6]!

    add             r5, r5, lr              ; dest_ptr1 += plane_stride
    sub             r5, r5, #64
    add             r6, r6, lr              ; dest_ptr2 += plane_stride
    sub             r6, r6, #64

    bne             top_bottom_16

    sub             r5, r1, lr, asl #4      ; dest_ptr1 = src_ptr1 - (Border * plane_stride)
    add             r6, r2, lr              ; dest_ptr2 = src_ptr2 + plane_stride

    subs            r12, r12, #1
    bne             copy_top_bottom_uv
extra_uv_copy_needed
    mov             r7, lr, lsr #3          ; check to see if extra copy is needed
    ands            r7, r7, #0x7
    bne             extra_top_bottom_uv

end_of_border_copy_uv
    subs            r10, r10, #1
    ldrne           r7, [r0, #yv12_buffer_config_v_buffer] ; src_ptr1
    bne             border_copy_uv

    vpop            {d8 - d15}
    pop             {r4 - r10, pc}

;;;;;;;;;;;;;;;;;;;;;;
extra_top_bottom_y
    vld1.8          {q0}, [r1]!
    vld1.8          {q2}, [r2]!

    mov             r9, #4                  ; 32 >> 3

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

    sub             r5, r1, lr, asl #5      ; src_ptr1 - (Border * plane_stride)
    add             r6, r2, lr              ; src_ptr2 + plane_stride
    subs            r7, r7, #1
    bne             extra_top_bottom_y

    b               end_of_border_copy_y

extra_top_bottom_uv
    vld1.8          {d0}, [r1]!
    vld1.8          {d8}, [r2]!

    mov             r9, #2                  ; 16 >> 3

extra_top_bottom_16
    subs            r9, r9, #1

    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    vst1.8          {d0}, [r5], lr
    vst1.8          {d8}, [r6], lr
    bne             extra_top_bottom_16

    sub             r5, r1, lr, asl #4      ; src_ptr1 - (Border * plane_stride)
    add             r6, r2, lr              ; src_ptr2 + plane_stride
    subs            r7, r7, #1
    bne             extra_top_bottom_uv

    b               end_of_border_copy_uv

    ENDP
    END
