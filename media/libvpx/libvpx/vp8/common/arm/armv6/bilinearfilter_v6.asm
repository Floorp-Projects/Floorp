;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_filter_block2d_bil_first_pass_armv6|
    EXPORT  |vp8_filter_block2d_bil_second_pass_armv6|

    AREA    |.text|, CODE, READONLY  ; name this block of code

;-------------------------------------
; r0    unsigned char  *src_ptr,
; r1    unsigned short *dst_ptr,
; r2    unsigned int    src_pitch,
; r3    unsigned int    height,
; stack unsigned int    width,
; stack const short    *vp8_filter
;-------------------------------------
; The output is transposed stroed in output array to make it easy for second pass filtering.
|vp8_filter_block2d_bil_first_pass_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #40]                  ; vp8_filter address
    ldr     r4, [sp, #36]                   ; width

    mov     r12, r3                         ; outer-loop counter

    add     r7, r2, r4                      ; preload next row
    pld     [r0, r7]

    sub     r2, r2, r4                      ; src increment for height loop

    ldr     r5, [r11]                       ; load up filter coefficients

    mov     r3, r3, lsl #1                  ; height*2
    add     r3, r3, #2                      ; plus 2 to make output buffer 4-bit aligned since height is actually (height+1)

    mov     r11, r1                         ; save dst_ptr for each row

    cmp     r5, #128                        ; if filter coef = 128, then skip the filter
    beq     bil_null_1st_filter

|bil_height_loop_1st_v6|
    ldrb    r6, [r0]                        ; load source data
    ldrb    r7, [r0, #1]
    ldrb    r8, [r0, #2]
    mov     lr, r4, lsr #2                  ; 4-in-parellel loop counter

|bil_width_loop_1st_v6|
    ldrb    r9, [r0, #3]
    ldrb    r10, [r0, #4]

    pkhbt   r6, r6, r7, lsl #16             ; src[1] | src[0]
    pkhbt   r7, r7, r8, lsl #16             ; src[2] | src[1]

    smuad   r6, r6, r5                      ; apply the filter
    pkhbt   r8, r8, r9, lsl #16             ; src[3] | src[2]
    smuad   r7, r7, r5
    pkhbt   r9, r9, r10, lsl #16            ; src[4] | src[3]

    smuad   r8, r8, r5
    smuad   r9, r9, r5

    add     r0, r0, #4
    subs    lr, lr, #1

    add     r6, r6, #0x40                   ; round_shift_and_clamp
    add     r7, r7, #0x40
    usat    r6, #16, r6, asr #7
    usat    r7, #16, r7, asr #7

    strh    r6, [r1], r3                    ; result is transposed and stored

    add     r8, r8, #0x40                   ; round_shift_and_clamp
    strh    r7, [r1], r3
    add     r9, r9, #0x40
    usat    r8, #16, r8, asr #7
    usat    r9, #16, r9, asr #7

    strh    r8, [r1], r3                    ; result is transposed and stored

    ldrneb  r6, [r0]                        ; load source data
    strh    r9, [r1], r3

    ldrneb  r7, [r0, #1]
    ldrneb  r8, [r0, #2]

    bne     bil_width_loop_1st_v6

    add     r0, r0, r2                      ; move to next input row
    subs    r12, r12, #1

    add     r9, r2, r4, lsl #1              ; adding back block width
    pld     [r0, r9]                        ; preload next row

    add     r11, r11, #2                    ; move over to next column
    mov     r1, r11

    bne     bil_height_loop_1st_v6

    ldmia   sp!, {r4 - r11, pc}

|bil_null_1st_filter|
|bil_height_loop_null_1st|
    mov     lr, r4, lsr #2                  ; loop counter

|bil_width_loop_null_1st|
    ldrb    r6, [r0]                        ; load data
    ldrb    r7, [r0, #1]
    ldrb    r8, [r0, #2]
    ldrb    r9, [r0, #3]

    strh    r6, [r1], r3                    ; store it to immediate buffer
    add     r0, r0, #4
    strh    r7, [r1], r3
    subs    lr, lr, #1
    strh    r8, [r1], r3
    strh    r9, [r1], r3

    bne     bil_width_loop_null_1st

    subs    r12, r12, #1
    add     r0, r0, r2                      ; move to next input line
    add     r11, r11, #2                    ; move over to next column
    mov     r1, r11

    bne     bil_height_loop_null_1st

    ldmia   sp!, {r4 - r11, pc}

    ENDP  ; |vp8_filter_block2d_bil_first_pass_armv6|


;---------------------------------
; r0    unsigned short *src_ptr,
; r1    unsigned char  *dst_ptr,
; r2    int             dst_pitch,
; r3    unsigned int    height,
; stack unsigned int    width,
; stack const short    *vp8_filter
;---------------------------------
|vp8_filter_block2d_bil_second_pass_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #40]                  ; vp8_filter address
    ldr     r4, [sp, #36]                   ; width

    ldr     r5, [r11]                       ; load up filter coefficients
    mov     r12, r4                         ; outer-loop counter = width, since we work on transposed data matrix
    mov     r11, r1

    cmp     r5, #128                        ; if filter coef = 128, then skip the filter
    beq     bil_null_2nd_filter

|bil_height_loop_2nd|
    ldr     r6, [r0]                        ; load the data
    ldr     r8, [r0, #4]
    ldrh    r10, [r0, #8]
    mov     lr, r3, lsr #2                  ; loop counter

|bil_width_loop_2nd|
    pkhtb   r7, r6, r8                      ; src[1] | src[2]
    pkhtb   r9, r8, r10                     ; src[3] | src[4]

    smuad   r6, r6, r5                      ; apply filter
    smuad   r8, r8, r5                      ; apply filter

    subs    lr, lr, #1

    smuadx  r7, r7, r5                      ; apply filter
    smuadx  r9, r9, r5                      ; apply filter

    add     r0, r0, #8

    add     r6, r6, #0x40                   ; round_shift_and_clamp
    add     r7, r7, #0x40
    usat    r6, #8, r6, asr #7
    usat    r7, #8, r7, asr #7
    strb    r6, [r1], r2                    ; the result is transposed back and stored

    add     r8, r8, #0x40                   ; round_shift_and_clamp
    strb    r7, [r1], r2
    add     r9, r9, #0x40
    usat    r8, #8, r8, asr #7
    usat    r9, #8, r9, asr #7
    strb    r8, [r1], r2                    ; the result is transposed back and stored

    ldrne   r6, [r0]                        ; load data
    strb    r9, [r1], r2
    ldrne   r8, [r0, #4]
    ldrneh  r10, [r0, #8]

    bne     bil_width_loop_2nd

    subs    r12, r12, #1
    add     r0, r0, #4                      ; update src for next row
    add     r11, r11, #1
    mov     r1, r11

    bne     bil_height_loop_2nd
    ldmia   sp!, {r4 - r11, pc}

|bil_null_2nd_filter|
|bil_height_loop_null_2nd|
    mov     lr, r3, lsr #2

|bil_width_loop_null_2nd|
    ldr     r6, [r0], #4                    ; load data
    subs    lr, lr, #1
    ldr     r8, [r0], #4

    strb    r6, [r1], r2                    ; store data
    mov     r7, r6, lsr #16
    strb    r7, [r1], r2
    mov     r9, r8, lsr #16
    strb    r8, [r1], r2
    strb    r9, [r1], r2

    bne     bil_width_loop_null_2nd

    subs    r12, r12, #1
    add     r0, r0, #4
    add     r11, r11, #1
    mov     r1, r11

    bne     bil_height_loop_null_2nd

    ldmia   sp!, {r4 - r11, pc}
    ENDP  ; |vp8_filter_block2d_second_pass_armv6|

    END
