;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT |vp8cx_pack_tokens_armv5|
    IMPORT |vp8_validate_buffer_arm|

    INCLUDE asm_enc_offsets.asm

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


; r0 vp8_writer *w
; r1 const TOKENEXTRA *p
; r2 int xcount
; r3 vp8_coef_encodings
; s0 vp8_extra_bits
; s1 vp8_coef_tree
|vp8cx_pack_tokens_armv5| PROC
    push    {r4-r12, lr}
    sub     sp, sp, #16

    ; Add size of xcount * sizeof (TOKENEXTRA) to get stop
    ;  sizeof (TOKENEXTRA) is 8
    add     r2, r1, r2, lsl #3          ; stop = p + xcount*sizeof(TOKENEXTRA)
    str     r2, [sp, #0]
    str     r3, [sp, #8]                ; save vp8_coef_encodings
    ldr     r2, [r0, #vp8_writer_lowvalue]
    ldr     r5, [r0, #vp8_writer_range]
    ldr     r3, [r0, #vp8_writer_count]
    b       check_p_lt_stop

while_p_lt_stop
    ldrb    r6, [r1, #tokenextra_token] ; t
    ldr     r4, [sp, #8]                ; vp8_coef_encodings
    mov     lr, #0
    add     r4, r4, r6, lsl #3          ; a = vp8_coef_encodings + t
    ldr     r9, [r1, #tokenextra_context_tree]   ; pp

    ldrb    r7, [r1, #tokenextra_skip_eob_node]

    ldr     r6, [r4, #vp8_token_value]  ; v
    ldr     r8, [r4, #vp8_token_len]    ; n

    ; vp8 specific skip_eob_node
    cmp     r7, #0
    movne   lr, #2                      ; i = 2
    subne   r8, r8, #1                  ; --n

    rsb     r4, r8, #32                 ; 32-n
    ldr     r10, [sp, #60]              ; vp8_coef_tree

    ; v is kept in r12 during the token pack loop
    lsl     r12, r6, r4                ; r12 = v << 32 - n

; loop start
token_loop
    ldrb    r4, [r9, lr, asr #1]        ; pp [i>>1]
    sub     r7, r5, #1                  ; range-1

    ; Decisions are made based on the bit value shifted
    ; off of v, so set a flag here based on this.
    ; This value is refered to as "bb"
    lsls    r12, r12, #1                ; bb = v >> n
    mul     r6, r4, r7                  ; ((range-1) * pp[i>>1]))

    ; bb can only be 0 or 1.  So only execute this statement
    ; if bb == 1, otherwise it will act like i + 0
    addcs   lr, lr, #1                  ; i + bb

    mov     r7, #1
    ldrsb   lr, [r10, lr]               ; i = vp8_coef_tree[i+bb]
    add     r4, r7, r6, lsr #8          ; 1 + (((range-1) * pp[i>>1]) >> 8)

    addcs   r2, r2, r4                  ; if  (bb) lowvalue += split
    subcs   r4, r5, r4                  ; if  (bb) range = range-split

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
    mov     r10, #0
    strb    r10, [r7, r4]               ; w->buffer[x] =(unsigned char)0
    sub     r4, r4, #1                  ; x--
token_zero_while_start
    cmp     r4, #0
    ldrge   r7, [r0, #vp8_writer_buffer]
    ldrb    r11, [r7, r4]
    cmpge   r11, #0xff
    beq     token_zero_while_loop

    ldr     r7, [r0, #vp8_writer_buffer]
    ldrb    r10, [r7, r4]               ; w->buffer[x]
    add     r10, r10, #1
    strb    r10, [r7, r4]               ; w->buffer[x] + 1
token_high_bit_not_set
    rsb     r4, r6, #24                 ; 24-offset
    ldr     r10, [r0, #vp8_writer_buffer]
    lsr     r7, r2, r4                  ; lowvalue >> (24-offset)
    ldr     r4, [r0, #vp8_writer_pos]   ; w->pos
    lsl     r2, r2, r6                  ; lowvalue <<= offset
    mov     r6, r3                      ; shift = count
    add     r11, r4, #1                 ; w->pos++
    bic     r2, r2, #0xff000000         ; lowvalue &= 0xffffff
    str     r11, [r0, #vp8_writer_pos]
    sub     r3, r3, #8                  ; count -= 8

    VALIDATE_POS r10, r11               ; validate_buffer at pos

    strb    r7, [r10, r4]               ; w->buffer[w->pos++]

    ; r10 is used earlier in the loop, but r10 is used as
    ; temp variable here.  So after r10 is used, reload
    ; vp8_coef_tree_dcd into r10
    ldr     r10, [sp, #60]              ; vp8_coef_tree

token_count_lt_zero
    lsl     r2, r2, r6                  ; lowvalue <<= shift

    subs    r8, r8, #1                  ; --n
    bne     token_loop

    ldrb    r6, [r1, #tokenextra_token] ; t
    ldr     r7, [sp, #56]               ; vp8_extra_bits
    ; Add t * sizeof (vp8_extra_bit_struct) to get the desired
    ;  element.  Here vp8_extra_bit_struct == 16
    add     r12, r7, r6, lsl #4         ; b = vp8_extra_bits + t

    ldr     r4, [r12, #vp8_extra_bit_struct_base_val]
    cmp     r4, #0
    beq     skip_extra_bits

;   if( b->base_val)
    ldr     r8, [r12, #vp8_extra_bit_struct_len] ; L
    ldrsh   lr, [r1, #tokenextra_extra] ; e = p->Extra
    cmp     r8, #0                      ; if( L)
    beq     no_extra_bits

    ldr     r9, [r12, #vp8_extra_bit_struct_prob]
    asr     r7, lr, #1                  ; v=e>>1

    ldr     r10, [r12, #vp8_extra_bit_struct_tree]
    str     r10, [sp, #4]               ; b->tree

    rsb     r4, r8, #32
    lsl     r12, r7, r4

    mov     lr, #0                      ; i = 0

extra_bits_loop
    ldrb    r4, [r9, lr, asr #1]            ; pp[i>>1]
    sub     r7, r5, #1                  ; range-1
    lsls    r12, r12, #1                ; v >> n
    mul     r6, r4, r7                  ; (range-1) * pp[i>>1]
    addcs   lr, lr, #1                  ; i + bb

    mov     r7, #1
    ldrsb   lr, [r10, lr]               ; i = b->tree[i+bb]
    add     r4, r7, r6, lsr #8          ; split = 1 +  (((range-1) * pp[i>>1]) >> 8)

    addcs   r2, r2, r4                  ; if  (bb) lowvalue += split
    subcs   r4, r5, r4                  ; if  (bb) range = range-split

    clz     r6, r4
    sub     r6, r6, #24

    adds    r3, r3, r6                  ; count += shift
    lsl     r5, r4, r6                  ; range <<= shift
    bmi     extra_count_lt_zero         ; if(count >= 0)

    sub     r6, r6, r3                  ; offset= shift - count
    sub     r4, r6, #1                  ; offset-1
    lsls    r4, r2, r4                  ; if((lowvalue<<(offset-1)) & 0x80000000 )
    bpl     extra_high_bit_not_set

    ldr     r4, [r0, #vp8_writer_pos]   ; x
    sub     r4, r4, #1                  ; x = w->pos - 1
    b       extra_zero_while_start
extra_zero_while_loop
    mov     r10, #0
    strb    r10, [r7, r4]               ; w->buffer[x] =(unsigned char)0
    sub     r4, r4, #1                  ; x--
extra_zero_while_start
    cmp     r4, #0
    ldrge   r7, [r0, #vp8_writer_buffer]
    ldrb    r11, [r7, r4]
    cmpge   r11, #0xff
    beq     extra_zero_while_loop

    ldr     r7, [r0, #vp8_writer_buffer]
    ldrb    r10, [r7, r4]
    add     r10, r10, #1
    strb    r10, [r7, r4]
extra_high_bit_not_set
    rsb     r4, r6, #24                 ; 24-offset
    ldr     r10, [r0, #vp8_writer_buffer]
    lsr     r7, r2, r4                  ; lowvalue >> (24-offset)
    ldr     r4, [r0, #vp8_writer_pos]
    lsl     r2, r2, r6                  ; lowvalue <<= offset
    mov     r6, r3                      ; shift = count
    add     r11, r4, #1                 ; w->pos++
    bic     r2, r2, #0xff000000         ; lowvalue &= 0xffffff
    str     r11, [r0, #vp8_writer_pos]
    sub     r3, r3, #8                  ; count -= 8

    VALIDATE_POS r10, r11               ; validate_buffer at pos

    strb    r7, [r10, r4]               ; w->buffer[w->pos++]=(lowvalue >> (24-offset))
    ldr     r10, [sp, #4]               ; b->tree
extra_count_lt_zero
    lsl     r2, r2, r6

    subs    r8, r8, #1                  ; --n
    bne     extra_bits_loop             ; while (n)

no_extra_bits
    ldr     lr, [r1, #4]                ; e = p->Extra
    add     r4, r5, #1                  ; range + 1
    tst     lr, #1
    lsr     r4, r4, #1                  ; split = (range + 1) >> 1
    addne   r2, r2, r4                  ; lowvalue += split
    subne   r4, r5, r4                  ; range = range-split
    tst     r2, #0x80000000             ; lowvalue & 0x80000000
    lsl     r5, r4, #1                  ; range <<= 1
    beq     end_high_bit_not_set

    ldr     r4, [r0, #vp8_writer_pos]
    mov     r7, #0
    sub     r4, r4, #1
    b       end_zero_while_start
end_zero_while_loop
    strb    r7, [r6, r4]
    sub     r4, r4, #1                  ; x--
end_zero_while_start
    cmp     r4, #0
    ldrge   r6, [r0, #vp8_writer_buffer]
    ldrb    r12, [r6, r4]
    cmpge   r12, #0xff
    beq     end_zero_while_loop

    ldr     r6, [r0, #vp8_writer_buffer]
    ldrb    r7, [r6, r4]
    add     r7, r7, #1
    strb    r7, [r6, r4]
end_high_bit_not_set
    adds    r3, r3, #1                  ; ++count
    lsl     r2, r2, #1                  ; lowvalue  <<= 1
    bne     end_count_zero

    ldr     r4, [r0, #vp8_writer_pos]
    mvn     r3, #7
    ldr     r7, [r0, #vp8_writer_buffer]
    lsr     r6, r2, #24                 ; lowvalue >> 24
    add     r12, r4, #1                 ; w->pos++
    bic     r2, r2, #0xff000000         ; lowvalue &= 0xffffff
    str     r12, [r0, #vp8_writer_pos]

    VALIDATE_POS r7, r12               ; validate_buffer at pos

    strb    r6, [r7, r4]
end_count_zero
skip_extra_bits
    add     r1, r1, #TOKENEXTRA_SZ      ; ++p
check_p_lt_stop
    ldr     r4, [sp, #0]                ; stop
    cmp     r1, r4                      ; while( p < stop)
    bcc     while_p_lt_stop

    str     r2, [r0, #vp8_writer_lowvalue]
    str     r5, [r0, #vp8_writer_range]
    str     r3, [r0, #vp8_writer_count]
    add     sp, sp, #16
    pop     {r4-r12, pc}
    ENDP

    END
