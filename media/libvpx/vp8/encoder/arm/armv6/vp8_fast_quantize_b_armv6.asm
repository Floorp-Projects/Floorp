;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_fast_quantize_b_armv6|

    INCLUDE asm_enc_offsets.asm

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    BLOCK *b
; r1    BLOCKD *d
|vp8_fast_quantize_b_armv6| PROC
    stmfd   sp!, {r1, r4-r11, lr}

    ldr     r3, [r0, #vp8_block_coeff]      ; coeff
    ldr     r4, [r0, #vp8_block_quant_fast] ; quant_fast
    ldr     r5, [r0, #vp8_block_round]      ; round
    ldr     r6, [r1, #vp8_blockd_qcoeff]    ; qcoeff
    ldr     r7, [r1, #vp8_blockd_dqcoeff]   ; dqcoeff
    ldr     r8, [r1, #vp8_blockd_dequant]   ; dequant

    ldr     r2, loop_count          ; loop_count=0x1000000. 'lsls' instruction
                                    ; is used to update the counter so that
                                    ; it can be used to mark nonzero
                                    ; quantized coefficient pairs.

    mov     r1, #0                  ; flags for quantized coeffs

    ; PART 1: quantization and dequantization loop
loop
    ldr     r9, [r3], #4            ; [z1 | z0]
    ldr     r10, [r5], #4           ; [r1 | r0]
    ldr     r11, [r4], #4           ; [q1 | q0]

    ssat16  lr, #1, r9              ; [sz1 | sz0]
    eor     r9, r9, lr              ; [z1 ^ sz1 | z0 ^ sz0]
    ssub16  r9, r9, lr              ; x = (z ^ sz) - sz
    sadd16  r9, r9, r10             ; [x1+r1 | x0+r0]

    ldr     r12, [r3], #4           ; [z3 | z2]

    smulbb  r0, r9, r11             ; [(x0+r0)*q0]
    smultt  r9, r9, r11             ; [(x1+r1)*q1]

    ldr     r10, [r5], #4           ; [r3 | r2]

    ssat16  r11, #1, r12            ; [sz3 | sz2]
    eor     r12, r12, r11           ; [z3 ^ sz3 | z2 ^ sz2]
    pkhtb   r0, r9, r0, asr #16     ; [y1 | y0]
    ldr     r9, [r4], #4            ; [q3 | q2]
    ssub16  r12, r12, r11           ; x = (z ^ sz) - sz

    sadd16  r12, r12, r10           ; [x3+r3 | x2+r2]

    eor     r0, r0, lr              ; [(y1 ^ sz1) | (y0 ^ sz0)]

    smulbb  r10, r12, r9            ; [(x2+r2)*q2]
    smultt  r12, r12, r9            ; [(x3+r3)*q3]

    ssub16  r0, r0, lr              ; x = (y ^ sz) - sz

    cmp     r0, #0                  ; check if zero
    orrne   r1, r1, r2, lsr #24     ; add flag for nonzero coeffs

    str     r0, [r6], #4            ; *qcoeff++ = x
    ldr     r9, [r8], #4            ; [dq1 | dq0]

    pkhtb   r10, r12, r10, asr #16  ; [y3 | y2]
    eor     r10, r10, r11           ; [(y3 ^ sz3) | (y2 ^ sz2)]
    ssub16  r10, r10, r11           ; x = (y ^ sz) - sz

    cmp     r10, #0                 ; check if zero
    orrne   r1, r1, r2, lsr #23     ; add flag for nonzero coeffs

    str     r10, [r6], #4           ; *qcoeff++ = x
    ldr     r11, [r8], #4           ; [dq3 | dq2]

    smulbb  r12, r0, r9             ; [x0*dq0]
    smultt  r0, r0, r9              ; [x1*dq1]

    smulbb  r9, r10, r11            ; [x2*dq2]
    smultt  r10, r10, r11           ; [x3*dq3]

    lsls    r2, r2, #2              ; update loop counter
    strh    r12, [r7, #0]           ; dqcoeff[0] = [x0*dq0]
    strh    r0, [r7, #2]            ; dqcoeff[1] = [x1*dq1]
    strh    r9, [r7, #4]            ; dqcoeff[2] = [x2*dq2]
    strh    r10, [r7, #6]           ; dqcoeff[3] = [x3*dq3]
    add     r7, r7, #8              ; dqcoeff += 8
    bne     loop

    ; PART 2: check position for eob...
    mov     lr, #0                  ; init eob
    cmp     r1, #0                  ; coeffs after quantization?
    ldr     r11, [sp, #0]           ; restore BLOCKD pointer
    beq     end                     ; skip eob calculations if all zero

    ldr     r0, [r11, #vp8_blockd_qcoeff]

    ; check shortcut for nonzero qcoeffs
    tst    r1, #0x80
    bne    quant_coeff_15_14
    tst    r1, #0x20
    bne    quant_coeff_13_11
    tst    r1, #0x8
    bne    quant_coeff_12_7
    tst    r1, #0x40
    bne    quant_coeff_10_9
    tst    r1, #0x10
    bne    quant_coeff_8_3
    tst    r1, #0x2
    bne    quant_coeff_6_5
    tst    r1, #0x4
    bne    quant_coeff_4_2
    b      quant_coeff_1_0

quant_coeff_15_14
    ldrh    r2, [r0, #30]       ; rc=15, i=15
    mov     lr, #16
    cmp     r2, #0
    bne     end

    ldrh    r3, [r0, #28]       ; rc=14, i=14
    mov     lr, #15
    cmp     r3, #0
    bne     end

quant_coeff_13_11
    ldrh    r2, [r0, #22]       ; rc=11, i=13
    mov     lr, #14
    cmp     r2, #0
    bne     end

quant_coeff_12_7
    ldrh    r3, [r0, #14]       ; rc=7,  i=12
    mov     lr, #13
    cmp     r3, #0
    bne     end

    ldrh    r2, [r0, #20]       ; rc=10, i=11
    mov     lr, #12
    cmp     r2, #0
    bne     end

quant_coeff_10_9
    ldrh    r3, [r0, #26]       ; rc=13, i=10
    mov     lr, #11
    cmp     r3, #0
    bne     end

    ldrh    r2, [r0, #24]       ; rc=12, i=9
    mov     lr, #10
    cmp     r2, #0
    bne     end

quant_coeff_8_3
    ldrh    r3, [r0, #18]       ; rc=9,  i=8
    mov     lr, #9
    cmp     r3, #0
    bne     end

    ldrh    r2, [r0, #12]       ; rc=6,  i=7
    mov     lr, #8
    cmp     r2, #0
    bne     end

quant_coeff_6_5
    ldrh    r3, [r0, #6]        ; rc=3,  i=6
    mov     lr, #7
    cmp     r3, #0
    bne     end

    ldrh    r2, [r0, #4]        ; rc=2,  i=5
    mov     lr, #6
    cmp     r2, #0
    bne     end

quant_coeff_4_2
    ldrh    r3, [r0, #10]       ; rc=5,  i=4
    mov     lr, #5
    cmp     r3, #0
    bne     end

    ldrh    r2, [r0, #16]       ; rc=8,  i=3
    mov     lr, #4
    cmp     r2, #0
    bne     end

    ldrh    r3, [r0, #8]        ; rc=4,  i=2
    mov     lr, #3
    cmp     r3, #0
    bne     end

quant_coeff_1_0
    ldrh    r2, [r0, #2]        ; rc=1,  i=1
    mov     lr, #2
    cmp     r2, #0
    bne     end

    mov     lr, #1              ; rc=0,  i=0

end
    str     lr, [r11, #vp8_blockd_eob]
    ldmfd   sp!, {r1, r4-r11, pc}

    ENDP

loop_count
    DCD     0x1000000

    END

