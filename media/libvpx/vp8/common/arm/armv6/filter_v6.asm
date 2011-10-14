;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_filter_block2d_first_pass_armv6|
    EXPORT  |vp8_filter_block2d_first_pass_16x16_armv6|
    EXPORT  |vp8_filter_block2d_first_pass_8x8_armv6|
    EXPORT  |vp8_filter_block2d_second_pass_armv6|
    EXPORT  |vp8_filter4_block2d_second_pass_armv6|
    EXPORT  |vp8_filter_block2d_first_pass_only_armv6|
    EXPORT  |vp8_filter_block2d_second_pass_only_armv6|

    AREA    |.text|, CODE, READONLY  ; name this block of code
;-------------------------------------
; r0    unsigned char *src_ptr
; r1    short         *output_ptr
; r2    unsigned int src_pixels_per_line
; r3    unsigned int output_width
; stack unsigned int output_height
; stack const short *vp8_filter
;-------------------------------------
; vp8_filter the input and put in the output array.  Apply the 6 tap FIR filter with
; the output being a 2 byte value and the intput being a 1 byte value.
|vp8_filter_block2d_first_pass_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #40]                  ; vp8_filter address
    ldr     r7, [sp, #36]                   ; output height

    sub     r2, r2, r3                      ; inside loop increments input array,
                                            ; so the height loop only needs to add
                                            ; r2 - width to the input pointer

    mov     r3, r3, lsl #1                  ; multiply width by 2 because using shorts
    add     r12, r3, #16                    ; square off the output
    sub     sp, sp, #4

    ldr     r4, [r11]                       ; load up packed filter coefficients
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

    str     r1, [sp]                        ; push destination to stack
    mov     r7, r7, lsl #16                 ; height is top part of counter

; six tap filter
|height_loop_1st_6|
    ldrb    r8, [r0, #-2]                   ; load source data
    ldrb    r9, [r0, #-1]
    ldrb    r10, [r0], #2
    orr     r7, r7, r3, lsr #2              ; construct loop counter

|width_loop_1st_6|
    ldrb    r11, [r0, #-1]

    pkhbt   lr, r8, r9, lsl #16             ; r9 | r8
    pkhbt   r8, r9, r10, lsl #16            ; r10 | r9

    ldrb    r9, [r0]

    smuad   lr, lr, r4                      ; apply the filter
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10
    smuad   r8, r8, r4
    pkhbt   r11, r11, r9, lsl #16           ; r9 | r11

    smlad   lr, r10, r5, lr
    ldrb    r10, [r0, #1]
    smlad   r8, r11, r5, r8
    ldrb    r11, [r0, #2]

    sub     r7, r7, #1

    pkhbt   r9, r9, r10, lsl #16            ; r10 | r9
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10

    smlad   lr, r9, r6, lr
    smlad   r11, r10, r6, r8

    ands    r10, r7, #0xff                  ; test loop counter

    add     lr, lr, #0x40                   ; round_shift_and_clamp
    ldrneb  r8, [r0, #-2]                   ; load data for next loop
    usat    lr, #8, lr, asr #7
    add     r11, r11, #0x40
    ldrneb  r9, [r0, #-1]
    usat    r11, #8, r11, asr #7

    strh    lr, [r1], r12                   ; result is transposed and stored, which
                                            ; will make second pass filtering easier.
    ldrneb  r10, [r0], #2
    strh    r11, [r1], r12

    bne     width_loop_1st_6

    ldr     r1, [sp]                        ; load and update dst address
    subs    r7, r7, #0x10000
    add     r0, r0, r2                      ; move to next input line

    add     r1, r1, #2                      ; move over to next column
    str     r1, [sp]

    bne     height_loop_1st_6

    add     sp, sp, #4
    ldmia   sp!, {r4 - r11, pc}

    ENDP

; --------------------------
; 16x16 version
; -----------------------------
|vp8_filter_block2d_first_pass_16x16_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #40]                  ; vp8_filter address
    ldr     r7, [sp, #36]                   ; output height

    add     r4, r2, #18                     ; preload next low
    pld     [r0, r4]

    sub     r2, r2, r3                      ; inside loop increments input array,
                                            ; so the height loop only needs to add
                                            ; r2 - width to the input pointer

    mov     r3, r3, lsl #1                  ; multiply width by 2 because using shorts
    add     r12, r3, #16                    ; square off the output
    sub     sp, sp, #4

    ldr     r4, [r11]                       ; load up packed filter coefficients
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

    str     r1, [sp]                        ; push destination to stack
    mov     r7, r7, lsl #16                 ; height is top part of counter

; six tap filter
|height_loop_1st_16_6|
    ldrb    r8, [r0, #-2]                   ; load source data
    ldrb    r9, [r0, #-1]
    ldrb    r10, [r0], #2
    orr     r7, r7, r3, lsr #2              ; construct loop counter

|width_loop_1st_16_6|
    ldrb    r11, [r0, #-1]

    pkhbt   lr, r8, r9, lsl #16             ; r9 | r8
    pkhbt   r8, r9, r10, lsl #16            ; r10 | r9

    ldrb    r9, [r0]

    smuad   lr, lr, r4                      ; apply the filter
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10
    smuad   r8, r8, r4
    pkhbt   r11, r11, r9, lsl #16           ; r9 | r11

    smlad   lr, r10, r5, lr
    ldrb    r10, [r0, #1]
    smlad   r8, r11, r5, r8
    ldrb    r11, [r0, #2]

    sub     r7, r7, #1

    pkhbt   r9, r9, r10, lsl #16            ; r10 | r9
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10

    smlad   lr, r9, r6, lr
    smlad   r11, r10, r6, r8

    ands    r10, r7, #0xff                  ; test loop counter

    add     lr, lr, #0x40                   ; round_shift_and_clamp
    ldrneb  r8, [r0, #-2]                   ; load data for next loop
    usat    lr, #8, lr, asr #7
    add     r11, r11, #0x40
    ldrneb  r9, [r0, #-1]
    usat    r11, #8, r11, asr #7

    strh    lr, [r1], r12                   ; result is transposed and stored, which
                                            ; will make second pass filtering easier.
    ldrneb  r10, [r0], #2
    strh    r11, [r1], r12

    bne     width_loop_1st_16_6

    ldr     r1, [sp]                        ; load and update dst address
    subs    r7, r7, #0x10000
    add     r0, r0, r2                      ; move to next input line

    add     r11, r2, #34                    ; adding back block width(=16)
    pld     [r0, r11]                       ; preload next low

    add     r1, r1, #2                      ; move over to next column
    str     r1, [sp]

    bne     height_loop_1st_16_6

    add     sp, sp, #4
    ldmia   sp!, {r4 - r11, pc}

    ENDP

; --------------------------
; 8x8 version
; -----------------------------
|vp8_filter_block2d_first_pass_8x8_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #40]                  ; vp8_filter address
    ldr     r7, [sp, #36]                   ; output height

    add     r4, r2, #10                     ; preload next low
    pld     [r0, r4]

    sub     r2, r2, r3                      ; inside loop increments input array,
                                            ; so the height loop only needs to add
                                            ; r2 - width to the input pointer

    mov     r3, r3, lsl #1                  ; multiply width by 2 because using shorts
    add     r12, r3, #16                    ; square off the output
    sub     sp, sp, #4

    ldr     r4, [r11]                       ; load up packed filter coefficients
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

    str     r1, [sp]                        ; push destination to stack
    mov     r7, r7, lsl #16                 ; height is top part of counter

; six tap filter
|height_loop_1st_8_6|
    ldrb    r8, [r0, #-2]                   ; load source data
    ldrb    r9, [r0, #-1]
    ldrb    r10, [r0], #2
    orr     r7, r7, r3, lsr #2              ; construct loop counter

|width_loop_1st_8_6|
    ldrb    r11, [r0, #-1]

    pkhbt   lr, r8, r9, lsl #16             ; r9 | r8
    pkhbt   r8, r9, r10, lsl #16            ; r10 | r9

    ldrb    r9, [r0]

    smuad   lr, lr, r4                      ; apply the filter
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10
    smuad   r8, r8, r4
    pkhbt   r11, r11, r9, lsl #16           ; r9 | r11

    smlad   lr, r10, r5, lr
    ldrb    r10, [r0, #1]
    smlad   r8, r11, r5, r8
    ldrb    r11, [r0, #2]

    sub     r7, r7, #1

    pkhbt   r9, r9, r10, lsl #16            ; r10 | r9
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10

    smlad   lr, r9, r6, lr
    smlad   r11, r10, r6, r8

    ands    r10, r7, #0xff                  ; test loop counter

    add     lr, lr, #0x40                   ; round_shift_and_clamp
    ldrneb  r8, [r0, #-2]                   ; load data for next loop
    usat    lr, #8, lr, asr #7
    add     r11, r11, #0x40
    ldrneb  r9, [r0, #-1]
    usat    r11, #8, r11, asr #7

    strh    lr, [r1], r12                   ; result is transposed and stored, which
                                            ; will make second pass filtering easier.
    ldrneb  r10, [r0], #2
    strh    r11, [r1], r12

    bne     width_loop_1st_8_6

    ldr     r1, [sp]                        ; load and update dst address
    subs    r7, r7, #0x10000
    add     r0, r0, r2                      ; move to next input line

    add     r11, r2, #18                    ; adding back block width(=8)
    pld     [r0, r11]                       ; preload next low

    add     r1, r1, #2                      ; move over to next column
    str     r1, [sp]

    bne     height_loop_1st_8_6

    add     sp, sp, #4
    ldmia   sp!, {r4 - r11, pc}

    ENDP

;---------------------------------
; r0    short         *src_ptr,
; r1    unsigned char *output_ptr,
; r2    unsigned int output_pitch,
; r3    unsigned int cnt,
; stack const short *vp8_filter
;---------------------------------
|vp8_filter_block2d_second_pass_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #36]                  ; vp8_filter address
    sub     sp, sp, #4
    mov     r7, r3, lsl #16                 ; height is top part of counter
    str     r1, [sp]                        ; push destination to stack

    ldr     r4, [r11]                       ; load up packed filter coefficients
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

    pkhbt   r12, r5, r4                     ; pack the filter differently
    pkhbt   r11, r6, r5

    sub     r0, r0, #4                      ; offset input buffer

|height_loop_2nd|
    ldr     r8, [r0]                        ; load the data
    ldr     r9, [r0, #4]
    orr     r7, r7, r3, lsr #1              ; loop counter

|width_loop_2nd|
    smuad   lr, r4, r8                      ; apply filter
    sub     r7, r7, #1
    smulbt  r8, r4, r8

    ldr     r10, [r0, #8]

    smlad   lr, r5, r9, lr
    smladx  r8, r12, r9, r8

    ldrh    r9, [r0, #12]

    smlad   lr, r6, r10, lr
    smladx  r8, r11, r10, r8

    add     r0, r0, #4
    smlatb  r10, r6, r9, r8

    add     lr, lr, #0x40                   ; round_shift_and_clamp
    ands    r8, r7, #0xff
    usat    lr, #8, lr, asr #7
    add     r10, r10, #0x40
    strb    lr, [r1], r2                    ; the result is transposed back and stored
    usat    r10, #8, r10, asr #7

    ldrne   r8, [r0]                        ; load data for next loop
    ldrne   r9, [r0, #4]
    strb    r10, [r1], r2

    bne     width_loop_2nd

    ldr     r1, [sp]                        ; update dst for next loop
    subs    r7, r7, #0x10000
    add     r0, r0, #16                     ; updata src for next loop
    add     r1, r1, #1
    str     r1, [sp]

    bne     height_loop_2nd

    add     sp, sp, #4
    ldmia   sp!, {r4 - r11, pc}

    ENDP

;---------------------------------
; r0    short         *src_ptr,
; r1    unsigned char *output_ptr,
; r2    unsigned int output_pitch,
; r3    unsigned int cnt,
; stack const short *vp8_filter
;---------------------------------
|vp8_filter4_block2d_second_pass_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #36]                  ; vp8_filter address
    mov     r7, r3, lsl #16                 ; height is top part of counter

    ldr     r4, [r11]                       ; load up packed filter coefficients
    add     lr, r1, r3                      ; save final destination pointer
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

    pkhbt   r12, r5, r4                     ; pack the filter differently
    pkhbt   r11, r6, r5
    mov     r4, #0x40                       ; rounding factor (for smlad{x})

|height_loop_2nd_4|
    ldrd    r8, [r0, #-4]                   ; load the data
    orr     r7, r7, r3, lsr #1              ; loop counter

|width_loop_2nd_4|
    ldr     r10, [r0, #4]!
    smladx  r6, r9, r12, r4                 ; apply filter
    pkhbt   r8, r9, r8
    smlad   r5, r8, r12, r4
    pkhbt   r8, r10, r9
    smladx  r6, r10, r11, r6
    sub     r7, r7, #1
    smlad   r5, r8, r11, r5

    mov     r8, r9                          ; shift the data for the next loop
    mov     r9, r10

    usat    r6, #8, r6, asr #7              ; shift and clamp
    usat    r5, #8, r5, asr #7

    strb    r5, [r1], r2                    ; the result is transposed back and stored
    tst     r7, #0xff
    strb    r6, [r1], r2

    bne     width_loop_2nd_4

    subs    r7, r7, #0x10000
    add     r0, r0, #16                     ; update src for next loop
    sub     r1, lr, r7, lsr #16             ; update dst for next loop

    bne     height_loop_2nd_4

    ldmia   sp!, {r4 - r11, pc}

    ENDP

;------------------------------------
; r0    unsigned char *src_ptr
; r1    unsigned char *output_ptr,
; r2    unsigned int src_pixels_per_line
; r3    unsigned int cnt,
; stack unsigned int output_pitch,
; stack const short *vp8_filter
;------------------------------------
|vp8_filter_block2d_first_pass_only_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    add     r7, r2, r3                      ; preload next low
    add     r7, r7, #2
    pld     [r0, r7]

    ldr     r4, [sp, #36]                   ; output pitch
    ldr     r11, [sp, #40]                  ; HFilter address
    sub     sp, sp, #8

    mov     r7, r3
    sub     r2, r2, r3                      ; inside loop increments input array,
                                            ; so the height loop only needs to add
                                            ; r2 - width to the input pointer

    sub     r4, r4, r3
    str     r4, [sp]                        ; save modified output pitch
    str     r2, [sp, #4]

    mov     r2, #0x40

    ldr     r4, [r11]                       ; load up packed filter coefficients
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

; six tap filter
|height_loop_1st_only_6|
    ldrb    r8, [r0, #-2]                   ; load data
    ldrb    r9, [r0, #-1]
    ldrb    r10, [r0], #2

    mov     r12, r3, lsr #1                 ; loop counter

|width_loop_1st_only_6|
    ldrb    r11, [r0, #-1]

    pkhbt   lr, r8, r9, lsl #16             ; r9 | r8
    pkhbt   r8, r9, r10, lsl #16            ; r10 | r9

    ldrb    r9, [r0]

;;  smuad   lr, lr, r4
    smlad   lr, lr, r4, r2
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10
;;  smuad   r8, r8, r4
    smlad   r8, r8, r4, r2
    pkhbt   r11, r11, r9, lsl #16           ; r9 | r11

    smlad   lr, r10, r5, lr
    ldrb    r10, [r0, #1]
    smlad   r8, r11, r5, r8
    ldrb    r11, [r0, #2]

    subs    r12, r12, #1

    pkhbt   r9, r9, r10, lsl #16            ; r10 | r9
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10

    smlad   lr, r9, r6, lr
    smlad   r10, r10, r6, r8

;;  add     lr, lr, #0x40                   ; round_shift_and_clamp
    ldrneb  r8, [r0, #-2]                   ; load data for next loop
    usat    lr, #8, lr, asr #7
;;  add     r10, r10, #0x40
    strb    lr, [r1], #1                    ; store the result
    usat    r10, #8, r10, asr #7

    ldrneb  r9, [r0, #-1]
    strb    r10, [r1], #1
    ldrneb  r10, [r0], #2

    bne     width_loop_1st_only_6

    ldr     lr, [sp]                        ; load back output pitch
    ldr     r12, [sp, #4]                   ; load back output pitch
    subs    r7, r7, #1
    add     r0, r0, r12                     ; updata src for next loop

    add     r11, r12, r3                    ; preload next low
    add     r11, r11, #2
    pld     [r0, r11]

    add     r1, r1, lr                      ; update dst for next loop

    bne     height_loop_1st_only_6

    add     sp, sp, #8
    ldmia   sp!, {r4 - r11, pc}
    ENDP  ; |vp8_filter_block2d_first_pass_only_armv6|


;------------------------------------
; r0    unsigned char *src_ptr,
; r1    unsigned char *output_ptr,
; r2    unsigned int src_pixels_per_line
; r3    unsigned int cnt,
; stack unsigned int output_pitch,
; stack const short *vp8_filter
;------------------------------------
|vp8_filter_block2d_second_pass_only_armv6| PROC
    stmdb   sp!, {r4 - r11, lr}

    ldr     r11, [sp, #40]                  ; VFilter address
    ldr     r12, [sp, #36]                  ; output pitch

    mov     r7, r3, lsl #16                 ; height is top part of counter
    sub     r0, r0, r2, lsl #1              ; need 6 elements for filtering, 2 before, 3 after

    sub     sp, sp, #8

    ldr     r4, [r11]                       ; load up packed filter coefficients
    ldr     r5, [r11, #4]
    ldr     r6, [r11, #8]

    str     r0, [sp]                        ; save r0 to stack
    str     r1, [sp, #4]                    ; save dst to stack

; six tap filter
|width_loop_2nd_only_6|
    ldrb    r8, [r0], r2                    ; load data
    orr     r7, r7, r3                      ; loop counter
    ldrb    r9, [r0], r2
    ldrb    r10, [r0], r2

|height_loop_2nd_only_6|
    ; filter first column in this inner loop, than, move to next colum.
    ldrb    r11, [r0], r2

    pkhbt   lr, r8, r9, lsl #16             ; r9 | r8
    pkhbt   r8, r9, r10, lsl #16            ; r10 | r9

    ldrb    r9, [r0], r2

    smuad   lr, lr, r4
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10
    smuad   r8, r8, r4
    pkhbt   r11, r11, r9, lsl #16           ; r9 | r11

    smlad   lr, r10, r5, lr
    ldrb    r10, [r0], r2
    smlad   r8, r11, r5, r8
    ldrb    r11, [r0]

    sub     r7, r7, #2
    sub     r0, r0, r2, lsl #2

    pkhbt   r9, r9, r10, lsl #16            ; r10 | r9
    pkhbt   r10, r10, r11, lsl #16          ; r11 | r10

    smlad   lr, r9, r6, lr
    smlad   r10, r10, r6, r8

    ands    r9, r7, #0xff

    add     lr, lr, #0x40                   ; round_shift_and_clamp
    ldrneb  r8, [r0], r2                    ; load data for next loop
    usat    lr, #8, lr, asr #7
    add     r10, r10, #0x40
    strb    lr, [r1], r12                   ; store the result for the column
    usat    r10, #8, r10, asr #7

    ldrneb  r9, [r0], r2
    strb    r10, [r1], r12
    ldrneb  r10, [r0], r2

    bne     height_loop_2nd_only_6

    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    subs    r7, r7, #0x10000
    add     r0, r0, #1                      ; move to filter next column
    str     r0, [sp]
    add     r1, r1, #1
    str     r1, [sp, #4]

    bne     width_loop_2nd_only_6

    add     sp, sp, #8

    ldmia   sp!, {r4 - r11, pc}
    ENDP  ; |vp8_filter_block2d_second_pass_only_armv6|

    END
