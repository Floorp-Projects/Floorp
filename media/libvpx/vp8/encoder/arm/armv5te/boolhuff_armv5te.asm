;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT |vp8_start_encode|
    EXPORT |vp8_encode_bool|
    EXPORT |vp8_stop_encode|
    EXPORT |vp8_encode_value|
    IMPORT |vp8_validate_buffer_arm|

    INCLUDE vp8_asm_enc_offsets.asm

    ARM
    REQUIRE8
    PRESERVE8

    AREA    |.text|, CODE, READONLY

    ; macro for validating write buffer position
    ; needs vp8_writer in r0
    ; start shall not be in r1
    MACRO
    VALIDATE_POS $start, $pos
    push {r0-r3, r12, lr}        ; rest of regs are preserved by subroutine call
    ldr  r2, [r0, #vp8_writer_buffer_end]
    ldr  r3, [r0, #vp8_writer_error]
    mov  r1, $pos
    mov  r0, $start
    bl   vp8_validate_buffer_arm
    pop  {r0-r3, r12, lr}
    MEND

; r0 BOOL_CODER *br
; r1 unsigned char *source
; r2 unsigned char *source_end
|vp8_start_encode| PROC
    str     r2,  [r0, #vp8_writer_buffer_end]
    mov     r12, #0
    mov     r3,  #255
    mvn     r2,  #23
    str     r12, [r0, #vp8_writer_lowvalue]
    str     r3,  [r0, #vp8_writer_range]
    str     r2,  [r0, #vp8_writer_count]
    str     r12, [r0, #vp8_writer_pos]
    str     r1,  [r0, #vp8_writer_buffer]
    bx      lr
    ENDP

; r0 BOOL_CODER *br
; r1 int bit
; r2 int probability
|vp8_encode_bool| PROC
    push    {r4-r10, lr}

    mov     r4, r2

    ldr     r2, [r0, #vp8_writer_lowvalue]
    ldr     r5, [r0, #vp8_writer_range]
    ldr     r3, [r0, #vp8_writer_count]

    sub     r7, r5, #1                  ; range-1

    cmp     r1, #0
    mul     r6, r4, r7                  ; ((range-1) * probability)

    mov     r7, #1
    add     r4, r7, r6, lsr #8          ; 1 + (((range-1) * probability) >> 8)

    addne   r2, r2, r4                  ; if  (bit) lowvalue += split
    subne   r4, r5, r4                  ; if  (bit) range = range-split

    ; Counting the leading zeros is used to normalize range.
    clz     r6, r4
    sub     r6, r6, #24                 ; shift

    ; Flag is set on the sum of count.  This flag is used later
    ; to determine if count >= 0
    adds    r3, r3, r6                  ; count += shift
    lsl     r5, r4, r6                  ; range <<= shift
    bmi     token_count_lt_zero         ; if(count >= 0)

    sub     r6, r6, r3                  ; offset = shift - count
    sub     r4, r6, #1                  ; offset-1
    lsls    r4, r2, r4                  ; if((lowvalue<<(offset-1)) & 0x80000000 )
    bpl     token_high_bit_not_set

    ldr     r4, [r0, #vp8_writer_pos]   ; x
    sub     r4, r4, #1                  ; x = w->pos-1
    b       token_zero_while_start
token_zero_while_loop
    mov     r9, #0
    strb    r9, [r7, r4]                ; w->buffer[x] =(unsigned char)0
    sub     r4, r4, #1                  ; x--
token_zero_while_start
    cmp     r4, #0
    ldrge   r7, [r0, #vp8_writer_buffer]
    ldrb    r1, [r7, r4]
    cmpge   r1, #0xff
    beq     token_zero_while_loop

    ldr     r7, [r0, #vp8_writer_buffer]
    ldrb    r9, [r7, r4]                ; w->buffer[x]
    add     r9, r9, #1
    strb    r9, [r7, r4]                ; w->buffer[x] + 1
token_high_bit_not_set
    rsb     r4, r6, #24                 ; 24-offset
    ldr     r9, [r0, #vp8_writer_buffer]
    lsr     r7, r2, r4                  ; lowvalue >> (24-offset)
    ldr     r4, [r0, #vp8_writer_pos]   ; w->pos
    lsl     r2, r2, r6                  ; lowvalue <<= offset
    mov     r6, r3                      ; shift = count
    add     r1, r4, #1                  ; w->pos++
    bic     r2, r2, #0xff000000         ; lowvalue &= 0xffffff
    str     r1, [r0, #vp8_writer_pos]
    sub     r3, r3, #8                  ; count -= 8

    VALIDATE_POS r9, r1                 ; validate_buffer at pos

    strb    r7, [r9, r4]                ; w->buffer[w->pos++]

token_count_lt_zero
    lsl     r2, r2, r6                  ; lowvalue <<= shift

    str     r2, [r0, #vp8_writer_lowvalue]
    str     r5, [r0, #vp8_writer_range]
    str     r3, [r0, #vp8_writer_count]
    pop     {r4-r10, pc}
    ENDP

; r0 BOOL_CODER *br
|vp8_stop_encode| PROC
    push    {r4-r10, lr}

    ldr     r2, [r0, #vp8_writer_lowvalue]
    ldr     r5, [r0, #vp8_writer_range]
    ldr     r3, [r0, #vp8_writer_count]

    mov     r10, #32

stop_encode_loop
    sub     r7, r5, #1                  ; range-1

    mov     r4, r7, lsl #7              ; ((range-1) * 128)

    mov     r7, #1
    add     r4, r7, r4, lsr #8          ; 1 + (((range-1) * 128) >> 8)

    ; Counting the leading zeros is used to normalize range.
    clz     r6, r4
    sub     r6, r6, #24                 ; shift

    ; Flag is set on the sum of count.  This flag is used later
    ; to determine if count >= 0
    adds    r3, r3, r6                  ; count += shift
    lsl     r5, r4, r6                  ; range <<= shift
    bmi     token_count_lt_zero_se      ; if(count >= 0)

    sub     r6, r6, r3                  ; offset = shift - count
    sub     r4, r6, #1                  ; offset-1
    lsls    r4, r2, r4                  ; if((lowvalue<<(offset-1)) & 0x80000000 )
    bpl     token_high_bit_not_set_se

    ldr     r4, [r0, #vp8_writer_pos]   ; x
    sub     r4, r4, #1                  ; x = w->pos-1
    b       token_zero_while_start_se
token_zero_while_loop_se
    mov     r9, #0
    strb    r9, [r7, r4]                ; w->buffer[x] =(unsigned char)0
    sub     r4, r4, #1                  ; x--
token_zero_while_start_se
    cmp     r4, #0
    ldrge   r7, [r0, #vp8_writer_buffer]
    ldrb    r1, [r7, r4]
    cmpge   r1, #0xff
    beq     token_zero_while_loop_se

    ldr     r7, [r0, #vp8_writer_buffer]
    ldrb    r9, [r7, r4]                ; w->buffer[x]
    add     r9, r9, #1
    strb    r9, [r7, r4]                ; w->buffer[x] + 1
token_high_bit_not_set_se
    rsb     r4, r6, #24                 ; 24-offset
    ldr     r9, [r0, #vp8_writer_buffer]
    lsr     r7, r2, r4                  ; lowvalue >> (24-offset)
    ldr     r4, [r0, #vp8_writer_pos]   ; w->pos
    lsl     r2, r2, r6                  ; lowvalue <<= offset
    mov     r6, r3                      ; shift = count
    add     r1, r4, #1                  ; w->pos++
    bic     r2, r2, #0xff000000         ; lowvalue &= 0xffffff
    str     r1, [r0, #vp8_writer_pos]
    sub     r3, r3, #8                  ; count -= 8

    VALIDATE_POS r9, r1                 ; validate_buffer at pos

    strb    r7, [r9, r4]                ; w->buffer[w->pos++]

token_count_lt_zero_se
    lsl     r2, r2, r6                  ; lowvalue <<= shift

    subs    r10, r10, #1
    bne     stop_encode_loop

    str     r2, [r0, #vp8_writer_lowvalue]
    str     r5, [r0, #vp8_writer_range]
    str     r3, [r0, #vp8_writer_count]
    pop     {r4-r10, pc}

    ENDP

; r0 BOOL_CODER *br
; r1 int data
; r2 int bits
|vp8_encode_value| PROC
    push    {r4-r12, lr}

    mov     r10, r2

    ldr     r2, [r0, #vp8_writer_lowvalue]
    ldr     r5, [r0, #vp8_writer_range]
    ldr     r3, [r0, #vp8_writer_count]

    rsb     r4, r10, #32                 ; 32-n

    ; v is kept in r1 during the token pack loop
    lsl     r1, r1, r4                  ; r1 = v << 32 - n

encode_value_loop
    sub     r7, r5, #1                  ; range-1

    ; Decisions are made based on the bit value shifted
    ; off of v, so set a flag here based on this.
    ; This value is refered to as "bb"
    lsls    r1, r1, #1                  ; bit = v >> n
    mov     r4, r7, lsl #7              ; ((range-1) * 128)

    mov     r7, #1
    add     r4, r7, r4, lsr #8          ; 1 + (((range-1) * 128) >> 8)

    addcs   r2, r2, r4                  ; if  (bit) lowvalue += split
    subcs   r4, r5, r4                  ; if  (bit) range = range-split

    ; Counting the leading zeros is used to normalize range.
    clz     r6, r4
    sub     r6, r6, #24                 ; shift

    ; Flag is set on the sum of count.  This flag is used later
    ; to determine if count >= 0
    adds    r3, r3, r6                  ; count += shift
    lsl     r5, r4, r6                  ; range <<= shift
    bmi     token_count_lt_zero_ev      ; if(count >= 0)

    sub     r6, r6, r3                  ; offset = shift - count
    sub     r4, r6, #1                  ; offset-1
    lsls    r4, r2, r4                  ; if((lowvalue<<(offset-1)) & 0x80000000 )
    bpl     token_high_bit_not_set_ev

    ldr     r4, [r0, #vp8_writer_pos]   ; x
    sub     r4, r4, #1                  ; x = w->pos-1
    b       token_zero_while_start_ev
token_zero_while_loop_ev
    mov     r9, #0
    strb    r9, [r7, r4]                ; w->buffer[x] =(unsigned char)0
    sub     r4, r4, #1                  ; x--
token_zero_while_start_ev
    cmp     r4, #0
    ldrge   r7, [r0, #vp8_writer_buffer]
    ldrb    r11, [r7, r4]
    cmpge   r11, #0xff
    beq     token_zero_while_loop_ev

    ldr     r7, [r0, #vp8_writer_buffer]
    ldrb    r9, [r7, r4]                ; w->buffer[x]
    add     r9, r9, #1
    strb    r9, [r7, r4]                ; w->buffer[x] + 1
token_high_bit_not_set_ev
    rsb     r4, r6, #24                 ; 24-offset
    ldr     r9, [r0, #vp8_writer_buffer]
    lsr     r7, r2, r4                  ; lowvalue >> (24-offset)
    ldr     r4, [r0, #vp8_writer_pos]   ; w->pos
    lsl     r2, r2, r6                  ; lowvalue <<= offset
    mov     r6, r3                      ; shift = count
    add     r11, r4, #1                 ; w->pos++
    bic     r2, r2, #0xff000000         ; lowvalue &= 0xffffff
    str     r11, [r0, #vp8_writer_pos]
    sub     r3, r3, #8                  ; count -= 8

    VALIDATE_POS r9, r11                ; validate_buffer at pos

    strb    r7, [r9, r4]                ; w->buffer[w->pos++]

token_count_lt_zero_ev
    lsl     r2, r2, r6                  ; lowvalue <<= shift

    subs    r10, r10, #1
    bne     encode_value_loop

    str     r2, [r0, #vp8_writer_lowvalue]
    str     r5, [r0, #vp8_writer_range]
    str     r3, [r0, #vp8_writer_count]
    pop     {r4-r12, pc}
    ENDP

    END
