;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_variance_halfpixvar16x16_h_neon|
    EXPORT  |vp8_variance_halfpixvar16x16_v_neon|
    EXPORT  |vp8_variance_halfpixvar16x16_hv_neon|
    EXPORT  |vp8_sub_pixel_variance16x16s_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;================================================
;unsigned int vp8_variance_halfpixvar16x16_h_neon
;(
;    unsigned char  *src_ptr, r0
;    int  src_pixels_per_line,  r1
;    unsigned char *dst_ptr,  r2
;    int dst_pixels_per_line,   r3
;    unsigned int *sse
;);
;================================================
|vp8_variance_halfpixvar16x16_h_neon| PROC
    push            {lr}

    mov             r12, #4                  ;loop counter
    ldr             lr, [sp, #4]           ;load *sse from stack
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

;First Pass: output_height lines x output_width columns (16x16)
vp8_filt_fpo16x16s_4_0_loop_neon
    vld1.u8         {d0, d1, d2, d3}, [r0], r1      ;load src data
    vld1.8          {q11}, [r2], r3
    vld1.u8         {d4, d5, d6, d7}, [r0], r1
    vld1.8          {q12}, [r2], r3
    vld1.u8         {d8, d9, d10, d11}, [r0], r1
    vld1.8          {q13}, [r2], r3
    vld1.u8         {d12, d13, d14, d15}, [r0], r1

    ;pld                [r0]
    ;pld                [r0, r1]
    ;pld                [r0, r1, lsl #1]

    vext.8          q1, q0, q1, #1          ;construct src_ptr[1]
    vext.8          q3, q2, q3, #1
    vext.8          q5, q4, q5, #1
    vext.8          q7, q6, q7, #1

    vrhadd.u8       q0, q0, q1              ;(src_ptr[0]+src_ptr[1])/round/shift right 1
    vld1.8          {q14}, [r2], r3
    vrhadd.u8       q1, q2, q3
    vrhadd.u8       q2, q4, q5
    vrhadd.u8       q3, q6, q7

    vsubl.u8        q4, d0, d22                 ;diff
    vsubl.u8        q5, d1, d23
    vsubl.u8        q6, d2, d24
    vsubl.u8        q7, d3, d25
    vsubl.u8        q0, d4, d26
    vsubl.u8        q1, d5, d27
    vsubl.u8        q2, d6, d28
    vsubl.u8        q3, d7, d29

    vpadal.s16      q8, q4                     ;sum
    vmlal.s16       q9, d8, d8                ;sse
    vmlal.s16       q10, d9, d9

    subs            r12, r12, #1

    vpadal.s16      q8, q5
    vmlal.s16       q9, d10, d10
    vmlal.s16       q10, d11, d11
    vpadal.s16      q8, q6
    vmlal.s16       q9, d12, d12
    vmlal.s16       q10, d13, d13
    vpadal.s16      q8, q7
    vmlal.s16       q9, d14, d14
    vmlal.s16       q10, d15, d15

    vpadal.s16      q8, q0                     ;sum
    vmlal.s16       q9, d0, d0                ;sse
    vmlal.s16       q10, d1, d1
    vpadal.s16      q8, q1
    vmlal.s16       q9, d2, d2
    vmlal.s16       q10, d3, d3
    vpadal.s16      q8, q2
    vmlal.s16       q9, d4, d4
    vmlal.s16       q10, d5, d5
    vpadal.s16      q8, q3
    vmlal.s16       q9, d6, d6
    vmlal.s16       q10, d7, d7

    bne             vp8_filt_fpo16x16s_4_0_loop_neon

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [lr]               ;store sse
    vshr.s32        d10, d10, #8
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    pop             {pc}
    ENDP

;================================================
;unsigned int vp8_variance_halfpixvar16x16_v_neon
;(
;    unsigned char  *src_ptr, r0
;    int  src_pixels_per_line,  r1
;    unsigned char *dst_ptr,  r2
;    int dst_pixels_per_line,   r3
;    unsigned int *sse
;);
;================================================
|vp8_variance_halfpixvar16x16_v_neon| PROC
    push            {lr}

    mov             r12, #4                     ;loop counter

    vld1.u8         {q0}, [r0], r1              ;load src data
    ldr             lr, [sp, #4]                ;load *sse from stack

    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

vp8_filt_spo16x16s_0_4_loop_neon
    vld1.u8         {q2}, [r0], r1
    vld1.8          {q1}, [r2], r3
    vld1.u8         {q4}, [r0], r1
    vld1.8          {q3}, [r2], r3
    vld1.u8         {q6}, [r0], r1
    vld1.8          {q5}, [r2], r3
    vld1.u8         {q15}, [r0], r1

    vrhadd.u8       q0, q0, q2
    vld1.8          {q7}, [r2], r3
    vrhadd.u8       q2, q2, q4
    vrhadd.u8       q4, q4, q6
    vrhadd.u8       q6, q6, q15

    vsubl.u8        q11, d0, d2                 ;diff
    vsubl.u8        q12, d1, d3
    vsubl.u8        q13, d4, d6
    vsubl.u8        q14, d5, d7
    vsubl.u8        q0, d8, d10
    vsubl.u8        q1, d9, d11
    vsubl.u8        q2, d12, d14
    vsubl.u8        q3, d13, d15

    vpadal.s16      q8, q11                     ;sum
    vmlal.s16       q9, d22, d22                ;sse
    vmlal.s16       q10, d23, d23

    subs            r12, r12, #1

    vpadal.s16      q8, q12
    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25
    vpadal.s16      q8, q13
    vmlal.s16       q9, d26, d26
    vmlal.s16       q10, d27, d27
    vpadal.s16      q8, q14
    vmlal.s16       q9, d28, d28
    vmlal.s16       q10, d29, d29

    vpadal.s16      q8, q0                     ;sum
    vmlal.s16       q9, d0, d0                 ;sse
    vmlal.s16       q10, d1, d1
    vpadal.s16      q8, q1
    vmlal.s16       q9, d2, d2
    vmlal.s16       q10, d3, d3
    vpadal.s16      q8, q2
    vmlal.s16       q9, d4, d4
    vmlal.s16       q10, d5, d5

    vmov            q0, q15

    vpadal.s16      q8, q3
    vmlal.s16       q9, d6, d6
    vmlal.s16       q10, d7, d7

    bne             vp8_filt_spo16x16s_0_4_loop_neon

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [lr]               ;store sse
    vshr.s32        d10, d10, #8
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    pop             {pc}
    ENDP

;================================================
;unsigned int vp8_variance_halfpixvar16x16_hv_neon
;(
;    unsigned char  *src_ptr, r0
;    int  src_pixels_per_line,  r1
;    unsigned char *dst_ptr,  r2
;    int dst_pixels_per_line,   r3
;    unsigned int *sse
;);
;================================================
|vp8_variance_halfpixvar16x16_hv_neon| PROC
    push            {lr}

    vld1.u8         {d0, d1, d2, d3}, [r0], r1      ;load src data

    ldr             lr, [sp, #4]           ;load *sse from stack
    vmov.i8         q13, #0                      ;q8 - sum
    vext.8          q1, q0, q1, #1          ;construct src_ptr[1]

    vmov.i8         q14, #0                      ;q9, q10 - sse
    vmov.i8         q15, #0

    mov             r12, #4                  ;loop counter
    vrhadd.u8       q0, q0, q1              ;(src_ptr[0]+src_ptr[1])/round/shift right 1

;First Pass: output_height lines x output_width columns (17x16)
vp8_filt16x16s_4_4_loop_neon
    vld1.u8         {d4, d5, d6, d7}, [r0], r1
    vld1.u8         {d8, d9, d10, d11}, [r0], r1
    vld1.u8         {d12, d13, d14, d15}, [r0], r1
    vld1.u8         {d16, d17, d18, d19}, [r0], r1

    ;pld                [r0]
    ;pld                [r0, r1]
    ;pld                [r0, r1, lsl #1]

    vext.8          q3, q2, q3, #1          ;construct src_ptr[1]
    vext.8          q5, q4, q5, #1
    vext.8          q7, q6, q7, #1
    vext.8          q9, q8, q9, #1

    vrhadd.u8       q1, q2, q3              ;(src_ptr[0]+src_ptr[1])/round/shift right 1
    vrhadd.u8       q2, q4, q5
    vrhadd.u8       q3, q6, q7
    vrhadd.u8       q4, q8, q9

    vld1.8          {q5}, [r2], r3
    vrhadd.u8       q0, q0, q1
    vld1.8          {q6}, [r2], r3
    vrhadd.u8       q1, q1, q2
    vld1.8          {q7}, [r2], r3
    vrhadd.u8       q2, q2, q3
    vld1.8          {q8}, [r2], r3
    vrhadd.u8       q3, q3, q4

    vsubl.u8        q9, d0, d10                 ;diff
    vsubl.u8        q10, d1, d11
    vsubl.u8        q11, d2, d12
    vsubl.u8        q12, d3, d13

    vsubl.u8        q0, d4, d14                 ;diff
    vsubl.u8        q1, d5, d15
    vsubl.u8        q5, d6, d16
    vsubl.u8        q6, d7, d17

    vpadal.s16      q13, q9                     ;sum
    vmlal.s16       q14, d18, d18                ;sse
    vmlal.s16       q15, d19, d19

    vpadal.s16      q13, q10                     ;sum
    vmlal.s16       q14, d20, d20                ;sse
    vmlal.s16       q15, d21, d21

    vpadal.s16      q13, q11                     ;sum
    vmlal.s16       q14, d22, d22                ;sse
    vmlal.s16       q15, d23, d23

    vpadal.s16      q13, q12                     ;sum
    vmlal.s16       q14, d24, d24                ;sse
    vmlal.s16       q15, d25, d25

    subs            r12, r12, #1

    vpadal.s16      q13, q0                     ;sum
    vmlal.s16       q14, d0, d0                ;sse
    vmlal.s16       q15, d1, d1

    vpadal.s16      q13, q1                     ;sum
    vmlal.s16       q14, d2, d2                ;sse
    vmlal.s16       q15, d3, d3

    vpadal.s16      q13, q5                     ;sum
    vmlal.s16       q14, d10, d10                ;sse
    vmlal.s16       q15, d11, d11

    vmov            q0, q4

    vpadal.s16      q13, q6                     ;sum
    vmlal.s16       q14, d12, d12                ;sse
    vmlal.s16       q15, d13, d13

    bne             vp8_filt16x16s_4_4_loop_neon

    vadd.u32        q15, q14, q15                ;accumulate sse
    vpaddl.s32      q0, q13                      ;accumulate sum

    vpaddl.u32      q1, q15
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [lr]               ;store sse
    vshr.s32        d10, d10, #8
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    pop             {pc}
    ENDP

;==============================
; r0    unsigned char  *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; stack unsigned char *dst_ptr,
; stack int dst_pixels_per_line,
; stack unsigned int *sse
;note: in vp8_find_best_half_pixel_step()(called when 8<Speed<15), and first call of vp8_find_best_sub_pixel_step()
;(called when speed<=8). xoffset/yoffset can only be 4 or 0, which means either by pass the filter,
;or filter coeff is {64, 64}. This simplified program only works in this situation.
;note: It happens that both xoffset and yoffset are zero. This can be handled in c code later.

|vp8_sub_pixel_variance16x16s_neon| PROC
    push            {r4, lr}

    ldr             r4, [sp, #8]            ;load *dst_ptr from stack
    ldr             r12, [sp, #12]          ;load dst_pixels_per_line from stack
    ldr             lr, [sp, #16]           ;load *sse from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             secondpass_bfilter16x16s_only

    cmp             r3, #0                  ;skip second_pass filter if yoffset=0
    beq             firstpass_bfilter16x16s_only

    vld1.u8         {d0, d1, d2, d3}, [r0], r1      ;load src data
    sub             sp, sp, #256            ;reserve space on stack for temporary storage
    vext.8          q1, q0, q1, #1          ;construct src_ptr[1]
    mov             r3, sp
    mov             r2, #4                  ;loop counter
    vrhadd.u8       q0, q0, q1              ;(src_ptr[0]+src_ptr[1])/round/shift right 1

;First Pass: output_height lines x output_width columns (17x16)
vp8e_filt_blk2d_fp16x16s_loop_neon
    vld1.u8         {d4, d5, d6, d7}, [r0], r1
    vld1.u8         {d8, d9, d10, d11}, [r0], r1
    vld1.u8         {d12, d13, d14, d15}, [r0], r1
    vld1.u8         {d16, d17, d18, d19}, [r0], r1

    ;pld                [r0]
    ;pld                [r0, r1]
    ;pld                [r0, r1, lsl #1]

    vext.8          q3, q2, q3, #1          ;construct src_ptr[1]
    vext.8          q5, q4, q5, #1
    vext.8          q7, q6, q7, #1
    vext.8          q9, q8, q9, #1

    vrhadd.u8       q1, q2, q3              ;(src_ptr[0]+src_ptr[1])/round/shift right 1
    vrhadd.u8       q2, q4, q5
    vrhadd.u8       q3, q6, q7
    vrhadd.u8       q4, q8, q9

    vrhadd.u8       q0, q0, q1
    vrhadd.u8       q1, q1, q2
    vrhadd.u8       q2, q2, q3
    vrhadd.u8       q3, q3, q4

    subs            r2, r2, #1
    vst1.u8         {d0, d1 ,d2, d3}, [r3]!         ;store result
    vmov            q0, q4
    vst1.u8         {d4, d5, d6, d7}, [r3]!

    bne             vp8e_filt_blk2d_fp16x16s_loop_neon

    b               sub_pixel_variance16x16s_neon

;--------------------
firstpass_bfilter16x16s_only
    mov             r2, #2                  ;loop counter
    sub             sp, sp, #256            ;reserve space on stack for temporary storage
    mov             r3, sp

;First Pass: output_height lines x output_width columns (16x16)
vp8e_filt_blk2d_fpo16x16s_loop_neon
    vld1.u8         {d0, d1, d2, d3}, [r0], r1      ;load src data
    vld1.u8         {d4, d5, d6, d7}, [r0], r1
    vld1.u8         {d8, d9, d10, d11}, [r0], r1
    vld1.u8         {d12, d13, d14, d15}, [r0], r1

    ;pld                [r0]
    ;pld                [r0, r1]
    ;pld                [r0, r1, lsl #1]

    vext.8          q1, q0, q1, #1          ;construct src_ptr[1]
    vld1.u8         {d16, d17, d18, d19}, [r0], r1
    vext.8          q3, q2, q3, #1
    vld1.u8         {d20, d21, d22, d23}, [r0], r1
    vext.8          q5, q4, q5, #1
    vld1.u8         {d24, d25, d26, d27}, [r0], r1
    vext.8          q7, q6, q7, #1
    vld1.u8         {d28, d29, d30, d31}, [r0], r1
    vext.8          q9, q8, q9, #1
    vext.8          q11, q10, q11, #1
    vext.8          q13, q12, q13, #1
    vext.8          q15, q14, q15, #1

    vrhadd.u8       q0, q0, q1              ;(src_ptr[0]+src_ptr[1])/round/shift right 1
    vrhadd.u8       q1, q2, q3
    vrhadd.u8       q2, q4, q5
    vrhadd.u8       q3, q6, q7
    vrhadd.u8       q4, q8, q9
    vrhadd.u8       q5, q10, q11
    vrhadd.u8       q6, q12, q13
    vrhadd.u8       q7, q14, q15

    subs            r2, r2, #1

    vst1.u8         {d0, d1, d2, d3}, [r3]!         ;store result
    vst1.u8         {d4, d5, d6, d7}, [r3]!
    vst1.u8         {d8, d9, d10, d11}, [r3]!
    vst1.u8         {d12, d13, d14, d15}, [r3]!

    bne             vp8e_filt_blk2d_fpo16x16s_loop_neon

    b               sub_pixel_variance16x16s_neon

;---------------------
secondpass_bfilter16x16s_only
    sub             sp, sp, #256            ;reserve space on stack for temporary storage

    mov             r2, #2                  ;loop counter
    vld1.u8         {d0, d1}, [r0], r1      ;load src data
    mov             r3, sp

vp8e_filt_blk2d_spo16x16s_loop_neon
    vld1.u8         {d2, d3}, [r0], r1
    vld1.u8         {d4, d5}, [r0], r1
    vld1.u8         {d6, d7}, [r0], r1
    vld1.u8         {d8, d9}, [r0], r1

    vrhadd.u8       q0, q0, q1
    vld1.u8         {d10, d11}, [r0], r1
    vrhadd.u8       q1, q1, q2
    vld1.u8         {d12, d13}, [r0], r1
    vrhadd.u8       q2, q2, q3
    vld1.u8         {d14, d15}, [r0], r1
    vrhadd.u8       q3, q3, q4
    vld1.u8         {d16, d17}, [r0], r1
    vrhadd.u8       q4, q4, q5
    vrhadd.u8       q5, q5, q6
    vrhadd.u8       q6, q6, q7
    vrhadd.u8       q7, q7, q8

    subs            r2, r2, #1

    vst1.u8         {d0, d1, d2, d3}, [r3]!         ;store result
    vmov            q0, q8
    vst1.u8         {d4, d5, d6, d7}, [r3]!
    vst1.u8         {d8, d9, d10, d11}, [r3]!           ;store result
    vst1.u8         {d12, d13, d14, d15}, [r3]!

    bne             vp8e_filt_blk2d_spo16x16s_loop_neon

    b               sub_pixel_variance16x16s_neon

;----------------------------
;variance16x16
sub_pixel_variance16x16s_neon
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

    sub             r3, r3, #256
    mov             r2, #4

sub_pixel_variance16x16s_neon_loop
    vld1.8          {q0}, [r3]!                 ;Load up source and reference
    vld1.8          {q1}, [r4], r12
    vld1.8          {q2}, [r3]!
    vld1.8          {q3}, [r4], r12
    vld1.8          {q4}, [r3]!
    vld1.8          {q5}, [r4], r12
    vld1.8          {q6}, [r3]!
    vld1.8          {q7}, [r4], r12

    vsubl.u8        q11, d0, d2                 ;diff
    vsubl.u8        q12, d1, d3
    vsubl.u8        q13, d4, d6
    vsubl.u8        q14, d5, d7
    vsubl.u8        q0, d8, d10
    vsubl.u8        q1, d9, d11
    vsubl.u8        q2, d12, d14
    vsubl.u8        q3, d13, d15

    vpadal.s16      q8, q11                     ;sum
    vmlal.s16       q9, d22, d22                ;sse
    vmlal.s16       q10, d23, d23

    subs            r2, r2, #1

    vpadal.s16      q8, q12
    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25
    vpadal.s16      q8, q13
    vmlal.s16       q9, d26, d26
    vmlal.s16       q10, d27, d27
    vpadal.s16      q8, q14
    vmlal.s16       q9, d28, d28
    vmlal.s16       q10, d29, d29

    vpadal.s16      q8, q0                     ;sum
    vmlal.s16       q9, d0, d0                ;sse
    vmlal.s16       q10, d1, d1
    vpadal.s16      q8, q1
    vmlal.s16       q9, d2, d2
    vmlal.s16       q10, d3, d3
    vpadal.s16      q8, q2
    vmlal.s16       q9, d4, d4
    vmlal.s16       q10, d5, d5
    vpadal.s16      q8, q3
    vmlal.s16       q9, d6, d6
    vmlal.s16       q10, d7, d7

    bne             sub_pixel_variance16x16s_neon_loop

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [lr]               ;store sse
    vshr.s32        d10, d10, #8
    vsub.s32        d0, d1, d10

    add             sp, sp, #256
    vmov.32         r0, d0[0]                   ;return

    pop             {r4, pc}
    ENDP

    END
