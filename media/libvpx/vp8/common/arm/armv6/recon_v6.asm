;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_recon_b_armv6|
    EXPORT  |vp8_recon2b_armv6|
    EXPORT  |vp8_recon4b_armv6|

    AREA    |.text|, CODE, READONLY  ; name this block of code
prd     RN  r0
dif     RN  r1
dst     RN  r2
stride      RN  r3

;void recon_b(unsigned char *pred_ptr, short *diff_ptr, unsigned char *dst_ptr, int stride)
; R0 char* pred_ptr
; R1 short * dif_ptr
; R2 char * dst_ptr
; R3 int stride

; Description:
; Loop through the block adding the Pred and Diff together.  Clamp and then
; store back into the Dst.

; Restrictions :
; all buffers are expected to be 4 byte aligned coming in and
; going out.
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
;
;
;
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
|vp8_recon_b_armv6| PROC
    stmdb   sp!, {r4 - r9, lr}

    ;0, 1, 2, 3
    ldr     r4, [prd], #16          ; 3 | 2 | 1 | 0
    ldr     r6, [dif, #0]           ;     1 |     0
    ldr     r7, [dif, #4]           ;     3 |     2

    pkhbt   r8, r6, r7, lsl #16     ;     2 |     0
    pkhtb   r9, r7, r6, asr #16     ;     3 |     1

    uxtab16 r8, r8, r4              ;     2 |     0  +  3 | 2 | 2 | 0
    uxtab16 r9, r9, r4, ror #8      ;     3 |     1  +  0 | 3 | 2 | 1

    usat16  r8, #8, r8
    usat16  r9, #8, r9
    add     dif, dif, #32
    orr     r8, r8, r9, lsl #8

    str     r8, [dst], stride

    ;0, 1, 2, 3
    ldr     r4, [prd], #16          ; 3 | 2 | 1 | 0
;;  ldr     r6, [dif, #8]           ;     1 |     0
;;  ldr     r7, [dif, #12]          ;     3 |     2
    ldr     r6, [dif, #0]           ;     1 |     0
    ldr     r7, [dif, #4]           ;     3 |     2

    pkhbt   r8, r6, r7, lsl #16     ;     2 |     0
    pkhtb   r9, r7, r6, asr #16     ;     3 |     1

    uxtab16 r8, r8, r4              ;     2 |     0  +  3 | 2 | 2 | 0
    uxtab16 r9, r9, r4, ror #8      ;     3 |     1  +  0 | 3 | 2 | 1

    usat16  r8, #8, r8
    usat16  r9, #8, r9
    add     dif, dif, #32
    orr     r8, r8, r9, lsl #8

    str     r8, [dst], stride

    ;0, 1, 2, 3
    ldr     r4, [prd], #16          ; 3 | 2 | 1 | 0
;;  ldr     r6, [dif, #16]          ;     1 |     0
;;  ldr     r7, [dif, #20]          ;     3 |     2
    ldr     r6, [dif, #0]           ;     1 |     0
    ldr     r7, [dif, #4]           ;     3 |     2

    pkhbt   r8, r6, r7, lsl #16     ;     2 |     0
    pkhtb   r9, r7, r6, asr #16     ;     3 |     1

    uxtab16 r8, r8, r4              ;     2 |     0  +  3 | 2 | 2 | 0
    uxtab16 r9, r9, r4, ror #8      ;     3 |     1  +  0 | 3 | 2 | 1

    usat16  r8, #8, r8
    usat16  r9, #8, r9
    add     dif, dif, #32
    orr     r8, r8, r9, lsl #8

    str     r8, [dst], stride

    ;0, 1, 2, 3
    ldr     r4, [prd], #16          ; 3 | 2 | 1 | 0
;;  ldr     r6, [dif, #24]          ;     1 |     0
;;  ldr     r7, [dif, #28]          ;     3 |     2
    ldr     r6, [dif, #0]           ;     1 |     0
    ldr     r7, [dif, #4]           ;     3 |     2

    pkhbt   r8, r6, r7, lsl #16     ;     2 |     0
    pkhtb   r9, r7, r6, asr #16     ;     3 |     1

    uxtab16 r8, r8, r4              ;     2 |     0  +  3 | 2 | 2 | 0
    uxtab16 r9, r9, r4, ror #8      ;     3 |     1  +  0 | 3 | 2 | 1

    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst], stride

    ldmia   sp!, {r4 - r9, pc}

    ENDP    ; |recon_b|

;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
;
;
;
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
; R0 char  *pred_ptr
; R1 short *dif_ptr
; R2 char  *dst_ptr
; R3 int stride
|vp8_recon4b_armv6| PROC
    stmdb   sp!, {r4 - r9, lr}

    mov     lr, #4

recon4b_loop
    ;0, 1, 2, 3
    ldr     r4, [prd], #4           ; 3 | 2 | 1 | 0
    ldr     r6, [dif, #0]           ;     1 |     0
    ldr     r7, [dif, #4]           ;     3 |     2

    pkhbt   r8, r6, r7, lsl #16     ;     2 |     0
    pkhtb   r9, r7, r6, asr #16     ;     3 |     1

    uxtab16 r8, r8, r4              ;     2 |     0  +  3 | 2 | 2 | 0
    uxtab16 r9, r9, r4, ror #8      ;     3 |     1  +  0 | 3 | 2 | 1

    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst]

    ;4, 5, 6, 7
    ldr     r4, [prd], #4
;;  ldr     r6, [dif, #32]
;;  ldr     r7, [dif, #36]
    ldr     r6, [dif, #8]
    ldr     r7, [dif, #12]

    pkhbt   r8, r6, r7, lsl #16
    pkhtb   r9, r7, r6, asr #16

    uxtab16 r8, r8, r4
    uxtab16 r9, r9, r4, ror #8
    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst, #4]

    ;8, 9, 10, 11
    ldr     r4, [prd], #4
;;  ldr     r6, [dif, #64]
;;  ldr     r7, [dif, #68]
    ldr     r6, [dif, #16]
    ldr     r7, [dif, #20]

    pkhbt   r8, r6, r7, lsl #16
    pkhtb   r9, r7, r6, asr #16

    uxtab16 r8, r8, r4
    uxtab16 r9, r9, r4, ror #8
    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst, #8]

    ;12, 13, 14, 15
    ldr     r4, [prd], #4
;;  ldr     r6, [dif, #96]
;;  ldr     r7, [dif, #100]
    ldr     r6, [dif, #24]
    ldr     r7, [dif, #28]

    pkhbt   r8, r6, r7, lsl #16
    pkhtb   r9, r7, r6, asr #16

    uxtab16 r8, r8, r4
    uxtab16 r9, r9, r4, ror #8
    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst, #12]

    add     dst, dst, stride
;;  add     dif, dif, #8
    add     dif, dif, #32

    subs    lr, lr, #1
    bne     recon4b_loop

    ldmia   sp!, {r4 - r9, pc}

    ENDP    ; |Recon4B|

;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
;
;
;
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
; R0 char  *pred_ptr
; R1 short *dif_ptr
; R2 char  *dst_ptr
; R3 int stride
|vp8_recon2b_armv6| PROC
    stmdb   sp!, {r4 - r9, lr}

    mov     lr, #4

recon2b_loop
    ;0, 1, 2, 3
    ldr     r4, [prd], #4
    ldr     r6, [dif, #0]
    ldr     r7, [dif, #4]

    pkhbt   r8, r6, r7, lsl #16
    pkhtb   r9, r7, r6, asr #16

    uxtab16 r8, r8, r4
    uxtab16 r9, r9, r4, ror #8
    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst]

    ;4, 5, 6, 7
    ldr     r4, [prd], #4
;;  ldr     r6, [dif, #32]
;;  ldr     r7, [dif, #36]
    ldr     r6, [dif, #8]
    ldr     r7, [dif, #12]

    pkhbt   r8, r6, r7, lsl #16
    pkhtb   r9, r7, r6, asr #16

    uxtab16 r8, r8, r4
    uxtab16 r9, r9, r4, ror #8
    usat16  r8, #8, r8
    usat16  r9, #8, r9
    orr     r8, r8, r9, lsl #8

    str     r8, [dst, #4]

    add     dst, dst, stride
;;  add     dif, dif, #8
    add     dif, dif, #16

    subs    lr, lr, #1
    bne     recon2b_loop

    ldmia   sp!, {r4 - r9, pc}

    ENDP    ; |Recon2B|

    END
