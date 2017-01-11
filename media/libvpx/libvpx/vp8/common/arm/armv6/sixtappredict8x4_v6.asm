;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_sixtap_predict8x4_armv6|

    AREA    |.text|, CODE, READONLY  ; name this block of code
;-------------------------------------
; r0    unsigned char *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; stack unsigned char *dst_ptr,
; stack int  dst_pitch
;-------------------------------------
;note: In first pass, store the result in transpose(8linesx9columns) on stack. Temporary stack size is 184.
;Line width is 20 that is 9 short data plus 2 to make it 4bytes aligned. In second pass, load data from stack,
;and the result is stored in transpose.
|vp8_sixtap_predict8x4_armv6| PROC
    stmdb       sp!, {r4 - r11, lr}
    str         r3, [sp, #-184]!            ;reserve space on stack for temporary storage, store yoffset

    cmp         r2, #0                      ;skip first_pass filter if xoffset=0
    add         lr, sp, #4                  ;point to temporary buffer
    beq         skip_firstpass_filter

;first-pass filter
    adr         r12, filter8_coeff
    sub         r0, r0, r1, lsl #1

    add         r3, r1, #10                 ; preload next low
    pld         [r0, r3]

    add         r2, r12, r2, lsl #4         ;calculate filter location
    add         r0, r0, #3                  ;adjust src only for loading convinience

    ldr         r3, [r2]                    ; load up packed filter coefficients
    ldr         r4, [r2, #4]
    ldr         r5, [r2, #8]

    mov         r2, #0x90000                ; height=9 is top part of counter

    sub         r1, r1, #8

|first_pass_hloop_v6|
    ldrb        r6, [r0, #-5]               ; load source data
    ldrb        r7, [r0, #-4]
    ldrb        r8, [r0, #-3]
    ldrb        r9, [r0, #-2]
    ldrb        r10, [r0, #-1]

    orr         r2, r2, #0x4                ; construct loop counter. width=8=4x2

    pkhbt       r6, r6, r7, lsl #16         ; r7 | r6
    pkhbt       r7, r7, r8, lsl #16         ; r8 | r7

    pkhbt       r8, r8, r9, lsl #16         ; r9 | r8
    pkhbt       r9, r9, r10, lsl #16        ; r10 | r9

|first_pass_wloop_v6|
    smuad       r11, r6, r3                 ; vp8_filter[0], vp8_filter[1]
    smuad       r12, r7, r3

    ldrb        r6, [r0], #1

    smlad       r11, r8, r4, r11            ; vp8_filter[2], vp8_filter[3]
    ldrb        r7, [r0], #1
    smlad       r12, r9, r4, r12

    pkhbt       r10, r10, r6, lsl #16       ; r10 | r9
    pkhbt       r6, r6, r7, lsl #16         ; r11 | r10
    smlad       r11, r10, r5, r11           ; vp8_filter[4], vp8_filter[5]
    smlad       r12, r6, r5, r12

    sub         r2, r2, #1

    add         r11, r11, #0x40             ; round_shift_and_clamp
    tst         r2, #0xff                   ; test loop counter
    usat        r11, #8, r11, asr #7
    add         r12, r12, #0x40
    strh        r11, [lr], #20              ; result is transposed and stored, which
    usat        r12, #8, r12, asr #7

    strh        r12, [lr], #20

    movne       r11, r6
    movne       r12, r7

    movne       r6, r8
    movne       r7, r9
    movne       r8, r10
    movne       r9, r11
    movne       r10, r12

    bne         first_pass_wloop_v6

    ;;add       r9, ppl, #30                ; attempt to load 2 adjacent cache lines
    ;;IF ARCHITECTURE=6
    ;pld        [src, ppl]
    ;;pld       [src, r9]
    ;;ENDIF

    subs        r2, r2, #0x10000

    sub         lr, lr, #158

    add         r0, r0, r1                  ; move to next input line

    add         r11, r1, #18                ; preload next low. adding back block width(=8), which is subtracted earlier
    pld         [r0, r11]

    bne         first_pass_hloop_v6

;second pass filter
secondpass_filter
    ldr         r3, [sp], #4                ; load back yoffset
    ldr         r0, [sp, #216]              ; load dst address from stack 180+36
    ldr         r1, [sp, #220]              ; load dst stride from stack 180+40

    cmp         r3, #0
    beq         skip_secondpass_filter

    adr         r12, filter8_coeff
    add         lr, r12, r3, lsl #4         ;calculate filter location

    mov         r2, #0x00080000

    ldr         r3, [lr]                    ; load up packed filter coefficients
    ldr         r4, [lr, #4]
    ldr         r5, [lr, #8]

    pkhbt       r12, r4, r3                 ; pack the filter differently
    pkhbt       r11, r5, r4

second_pass_hloop_v6
    ldr         r6, [sp]                    ; load the data
    ldr         r7, [sp, #4]

    orr         r2, r2, #2                  ; loop counter

second_pass_wloop_v6
    smuad       lr, r3, r6                  ; apply filter
    smulbt      r10, r3, r6

    ldr         r8, [sp, #8]

    smlad       lr, r4, r7, lr
    smladx      r10, r12, r7, r10

    ldrh        r9, [sp, #12]

    smlad       lr, r5, r8, lr
    smladx      r10, r11, r8, r10

    add         sp, sp, #4
    smlatb      r10, r5, r9, r10

    sub         r2, r2, #1

    add         lr, lr, #0x40               ; round_shift_and_clamp
    tst         r2, #0xff
    usat        lr, #8, lr, asr #7
    add         r10, r10, #0x40
    strb        lr, [r0], r1                ; the result is transposed back and stored
    usat        r10, #8, r10, asr #7

    strb        r10, [r0],r1

    movne       r6, r7
    movne       r7, r8

    bne         second_pass_wloop_v6

    subs        r2, r2, #0x10000
    add         sp, sp, #12                 ; updata src for next loop (20-8)
    sub         r0, r0, r1, lsl #2
    add         r0, r0, #1

    bne         second_pass_hloop_v6

    add         sp, sp, #20
    ldmia       sp!, {r4 - r11, pc}

;--------------------
skip_firstpass_filter
    sub         r0, r0, r1, lsl #1
    sub         r1, r1, #8
    mov         r2, #9

skip_firstpass_hloop
    ldrb        r4, [r0], #1                ; load data
    subs        r2, r2, #1
    ldrb        r5, [r0], #1
    strh        r4, [lr], #20               ; store it to immediate buffer
    ldrb        r6, [r0], #1                ; load data
    strh        r5, [lr], #20
    ldrb        r7, [r0], #1
    strh        r6, [lr], #20
    ldrb        r8, [r0], #1
    strh        r7, [lr], #20
    ldrb        r9, [r0], #1
    strh        r8, [lr], #20
    ldrb        r10, [r0], #1
    strh        r9, [lr], #20
    ldrb        r11, [r0], #1
    strh        r10, [lr], #20
    add         r0, r0, r1                  ; move to next input line
    strh        r11, [lr], #20

    sub         lr, lr, #158                ; move over to next column
    bne         skip_firstpass_hloop

    b           secondpass_filter

;--------------------
skip_secondpass_filter
    mov         r2, #8
    add         sp, sp, #4                  ;start from src[0] instead of src[-2]

skip_secondpass_hloop
    ldr         r6, [sp], #4
    subs        r2, r2, #1
    ldr         r8, [sp], #4

    mov         r7, r6, lsr #16             ; unpack
    strb        r6, [r0], r1
    mov         r9, r8, lsr #16
    strb        r7, [r0], r1
    add         sp, sp, #12                 ; 20-8
    strb        r8, [r0], r1
    strb        r9, [r0], r1

    sub         r0, r0, r1, lsl #2
    add         r0, r0, #1

    bne         skip_secondpass_hloop

    add         sp, sp, #16                 ; 180 - (160 +4)

    ldmia       sp!, {r4 - r11, pc}

    ENDP

;-----------------
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
filter8_coeff
    DCD     0x00000000,     0x00000080,     0x00000000,     0x00000000
    DCD     0xfffa0000,     0x000c007b,     0x0000ffff,     0x00000000
    DCD     0xfff50002,     0x0024006c,     0x0001fff8,     0x00000000
    DCD     0xfff70000,     0x0032005d,     0x0000fffa,     0x00000000
    DCD     0xfff00003,     0x004d004d,     0x0003fff0,     0x00000000
    DCD     0xfffa0000,     0x005d0032,     0x0000fff7,     0x00000000
    DCD     0xfff80001,     0x006c0024,     0x0002fff5,     0x00000000
    DCD     0xffff0000,     0x007b000c,     0x0000fffa,     0x00000000

    ;DCD        0,  0,  128,    0,   0,  0
    ;DCD        0, -6,  123,   12,  -1,  0
    ;DCD        2, -11, 108,   36,  -8,  1
    ;DCD        0, -9,   93,   50,  -6,  0
    ;DCD        3, -16,  77,   77, -16,  3
    ;DCD        0, -6,   50,   93,  -9,  0
    ;DCD        1, -8,   36,  108, -11,  2
    ;DCD        0, -1,   12,  123,  -6,  0

    END
