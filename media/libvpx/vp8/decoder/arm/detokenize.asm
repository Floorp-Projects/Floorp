;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_decode_mb_tokens_v6|

    AREA    |.text|, CODE, READONLY  ; name this block of code

    INCLUDE vpx_asm_offsets.asm

l_qcoeff    EQU     0
l_i         EQU     4
l_type      EQU     8
l_stop      EQU     12
l_c         EQU     16
l_l_ptr     EQU     20
l_a_ptr     EQU     24
l_bc        EQU     28
l_coef_ptr  EQU     32
l_stacksize EQU     64


;; constant offsets -- these should be created at build time
c_block2above_offset         EQU 25
c_entropy_nodes              EQU 11
c_dct_eob_token              EQU 11

|vp8_decode_mb_tokens_v6| PROC
    stmdb       sp!, {r4 - r11, lr}
    sub         sp, sp, #l_stacksize
    mov         r7, r1                      ; type
    mov         r9, r0                      ; detoken

    ldr         r1, [r9, #detok_current_bc]
    ldr         r0, [r9, #detok_qcoeff_start_ptr]
    mov         r11, #0                     ; i
    mov         r3, #16                     ; stop

    cmp         r7, #1                      ; type ?= 1
    addeq       r11, r11, #24               ; i = 24
    addeq       r3, r3, #8                  ; stop = 24
    addeq       r0, r0, #3, 24              ; qcoefptr += 24*16

    str         r0, [sp, #l_qcoeff]
    str         r11, [sp, #l_i]
    str         r7, [sp, #l_type]
    str         r3, [sp, #l_stop]
    str         r1, [sp, #l_bc]

    add         lr, r9, r7, lsl #2          ; detoken + type*4

    ldr         r8, [r1, #bool_decoder_user_buffer]

    ldr         r10, [lr, #detok_coef_probs]
    ldr         r5, [r1, #bool_decoder_count]
    ldr         r6, [r1, #bool_decoder_range]
    ldr         r4, [r1, #bool_decoder_value]

    str         r10, [sp, #l_coef_ptr]

BLOCK_LOOP
    ldr         r3, [r9, #detok_ptr_block2leftabove]
    ldr         r1, [r9, #detok_L]
    ldr         r2, [r9, #detok_A]
    ldrb        r12, [r3, r11]!             ; block2left[i]
    ldrb        r3, [r3, #c_block2above_offset]; block2above[i]

    cmp         r7, #0                      ; c = !type
    moveq       r7, #1
    movne       r7, #0

    ldrb        r0, [r1, r12]!              ; *(L += block2left[i])
    ldrb        r3, [r2, r3]!               ; *(A += block2above[i])
    mov         lr, #c_entropy_nodes        ; ENTROPY_NODES = 11

; VP8_COMBINEENTROPYCONTETEXTS(t, *a, *l) => t = ((*a) != 0) + ((*l) !=0)
    cmp         r0, #0                      ; *l ?= 0
    movne       r0, #1
    cmp         r3, #0                      ; *a ?= 0
    addne       r0, r0, #1                  ; t

    str         r1, [sp, #l_l_ptr]          ; save &l
    str         r2, [sp, #l_a_ptr]          ; save &a
    smlabb      r0, r0, lr, r10             ; Prob = coef_probs + (t * ENTROPY_NODES)
    mov         r1, #0                      ; t = 0
    str         r7, [sp, #l_c]

    ;align 4
COEFF_LOOP
    ldr         r3, [r9, #detok_ptr_coef_bands_x]
    ldr         lr, [r9, #detok_coef_tree_ptr]
    ;STALL
    ldrb        r3, [r3, r7]                ; coef_bands_x[c]
    ;STALL
    ;STALL
    add         r0, r0, r3                  ; Prob += coef_bands_x[c]

get_token_loop
    ldrb        r2, [r0, +r1, asr #1]       ; Prob[t >> 1]
    mov         r3, r6, lsl #8              ; range << 8
    sub         r3, r3, #256                ; (range << 8) - (1 << 8)
    mov         r10, #1                     ; 1

    smlawb      r2, r3, r2, r10             ; split = 1 + (((range-1) * probability) >> 8)

    ldrb        r12, [r8]                   ; load cx data byte in stall slot : r8 = bufptr
    ;++

    subs        r3, r4, r2, lsl #24         ; value-(split<<24): used later to calculate shift for NORMALIZE
    addhs       r1, r1, #1                  ; t += 1
    movhs       r4, r3                      ; value -= bigsplit (split << 24)
    subhs       r2, r6, r2                  ; range -= split
 ;   movlo       r6, r2                      ; range = split

    ldrsb     r1, [lr, r1]                  ; t = onyx_coef_tree_ptr[t]

; NORMALIZE
    clz         r3, r2                      ; vp8dx_bitreader_norm[range] + 24
    sub         r3, r3, #24                 ; vp8dx_bitreader_norm[range]
    subs        r5, r5, r3                  ; count -= shift
    mov         r6, r2, lsl r3              ; range <<= shift
    mov         r4, r4, lsl r3              ; value <<= shift

; if count <= 0, += BR_COUNT; value |= *bufptr++ << (BR_COUNT-count); BR_COUNT = 8, but need to upshift values by +16
    addle         r5, r5, #8                ; count += 8
    rsble         r3, r5, #24               ; 24 - count
    addle         r8, r8, #1                ; bufptr++
    orrle         r4, r4, r12, lsl r3       ; value |= *bufptr << shift + 16

    cmp         r1, #0                      ; t ?= 0
    bgt         get_token_loop              ; while (t > 0)

    cmn         r1, #c_dct_eob_token        ; if(t == -DCT_EOB_TOKEN)
    beq         END_OF_BLOCK                ; break

    rsb         lr, r1, #0                  ; v = -t;

    cmp         lr, #4                      ; if(v > FOUR_TOKEN)
    ble         SKIP_EXTRABITS

    ldr         r3, [r9, #detok_teb_base_ptr]
    mov         r11, #1                     ; 1 in split = 1 + ... nope, v+= 1 << bits_count
    add         r7, r3, lr, lsl #4          ; detok_teb_base_ptr + (v << 4)

    ldrsh       lr, [r7, #tokenextrabits_min_val] ; v = teb_ptr->min_val
    ldrsh       r0, [r7, #tokenextrabits_length] ; bits_count = teb_ptr->Length

extrabits_loop
    add         r3, r0, r7                  ; &teb_ptr->Probs[bits_count]

    ldrb        r2, [r3, #4]                ; probability. why +4?
    mov         r3, r6, lsl #8              ; range << 8
    sub         r3, r3, #256                ; range << 8 + 1 << 8

    smlawb      r2, r3, r2, r11             ; split = 1 +  (((range-1) * probability) >> 8)

    ldrb        r12, [r8]                   ; *bufptr
    ;++

    subs        r10, r4, r2, lsl #24        ; value - (split<<24)
    movhs       r4, r10                     ; value = value - (split << 24)
    subhs       r2, r6, r2                  ; range = range - split
    addhs       lr, lr, r11, lsl r0         ; v += ((UINT16)1<<bits_count)

; NORMALIZE
    clz         r3, r2                      ; shift - leading zeros in split
    sub         r3, r3, #24                 ; don't count first 3 bytes
    subs        r5, r5, r3                  ; count -= shift
    mov         r6, r2, lsl r3              ; range = range << shift
    mov         r4, r4, lsl r3              ; value <<= shift

    addle       r5, r5, #8                  ; count += BR_COUNT
    addle       r8, r8, #1                  ; bufptr++
    rsble       r3, r5, #24                 ; BR_COUNT - count
    orrle       r4, r4, r12, lsl r3         ; value |= *bufptr << (BR_COUNT - count)

    subs        r0, r0, #1                  ; bits_count --
    bpl         extrabits_loop


SKIP_EXTRABITS
    ldr         r11, [sp, #l_qcoeff]
    ldr         r0, [sp, #l_coef_ptr]       ; Prob = coef_probs

    cmp         r1, #0                      ; check for nonzero token - if (t)
    beq         SKIP_EOB_CHECK              ; if t is zero, we will skip the eob table chec

    add         r3, r6, #1                  ; range + 1
    mov         r2, r3, lsr #1              ; split = (range + 1) >> 1

    subs        r3, r4, r2, lsl #24         ; value - (split<<24)
    movhs       r4, r3                      ; value -= (split << 24)
    subhs       r2, r6, r2                  ; range -= split
    mvnhs       r3, lr                      ; -v
    addhs       lr, r3, #1                  ; v = (v ^ -1) + 1

; NORMALIZE
    clz         r3, r2                      ; leading 0s in split
    sub         r3, r3, #24                 ; shift
    subs        r5, r5, r3                  ; count -= shift
    mov         r6, r2, lsl r3              ; range <<= shift
    mov         r4, r4, lsl r3              ; value <<= shift
    ldrleb      r2, [r8], #1                ; *(bufptr++)
    addle       r5, r5, #8                  ; count += 8
    rsble       r3, r5, #24                 ; BR_COUNT - count
    orrle       r4, r4, r2, lsl r3          ; value |= *bufptr << (BR_COUNT - count)

    add         r0, r0, #11                 ; Prob += ENTROPY_NODES (11)

    cmn         r1, #1                      ; t < -ONE_TOKEN

    addlt       r0, r0, #11                 ; Prob += ENTROPY_NODES (11)

    mvn         r1, #1                      ; t = -1 ???? C is -2

SKIP_EOB_CHECK
    ldr         r7, [sp, #l_c]              ; c
    ldr         r3, [r9, #detok_scan]
    add         r1, r1, #2                  ; t+= 2
    cmp         r7, #15                     ; c should will be one higher

    ldr         r3, [r3, +r7, lsl #2]       ; scan[c] this needs pre-inc c value
    add         r7, r7, #1                  ; c++
    add         r3, r11, r3, lsl #1         ; qcoeff + scan[c]

    str         r7, [sp, #l_c]              ; store c
    strh        lr, [r3]                    ; qcoef_ptr[scan[c]] = v

    blt         COEFF_LOOP

    sub         r7, r7, #1                  ; if(t != -DCT_EOB_TOKEN) --c

END_OF_BLOCK
    ldr         r3, [sp, #l_type]           ; type
    ldr         r10, [sp, #l_coef_ptr]      ; coef_ptr
    ldr         r0, [sp, #l_qcoeff]         ; qcoeff
    ldr         r11, [sp, #l_i]             ; i
    ldr         r12, [sp, #l_stop]          ; stop

    cmp         r3, #0                      ; type ?= 0
    moveq       r1, #1
    movne       r1, #0
    add         r3, r11, r9                 ; detok + i

    cmp         r7, r1                      ; c ?= !type
    strb        r7, [r3, #detok_eob]        ; eob[i] = c

    ldr         r7, [sp, #l_l_ptr]          ; l
    ldr         r2, [sp, #l_a_ptr]          ; a
    movne       r3, #1                      ; t
    moveq       r3, #0

    add         r0, r0, #32                 ; qcoeff += 32 (16 * 2?)
    add         r11, r11, #1                ; i++
    strb        r3, [r7]                    ; *l = t
    strb        r3, [r2]                    ; *a = t
    str         r0, [sp, #l_qcoeff]         ; qcoeff
    str         r11, [sp, #l_i]             ; i

    cmp         r11, r12                    ; i < stop
    ldr         r7, [sp, #l_type]           ; type

    blt         BLOCK_LOOP

    cmp         r11, #25                    ; i ?= 25
    bne         ln2_decode_mb_to

    ldr         r12, [r9, #detok_qcoeff_start_ptr]
    ldr         r10, [r9, #detok_coef_probs]
    mov         r7, #0                      ; type/i = 0
    mov         r3, #16                     ; stop = 16
    str         r12, [sp, #l_qcoeff]        ; qcoeff_ptr = qcoeff_start_ptr
    str         r7, [sp, #l_i]
    str         r7, [sp, #l_type]
    str         r3, [sp, #l_stop]

    str         r10, [sp, #l_coef_ptr]      ; coef_probs = coef_probs[type=0]

    b           BLOCK_LOOP

ln2_decode_mb_to
    cmp         r11, #16                    ; i ?= 16
    bne         ln1_decode_mb_to

    mov         r10, #detok_coef_probs
    add         r10, r10, #2*4              ; coef_probs[type]
    ldr         r10, [r9, r10]              ; detok + detok_coef_probs[type]

    mov         r7, #2                      ; type = 2
    mov         r3, #24                     ; stop = 24

    str         r7, [sp, #l_type]
    str         r3, [sp, #l_stop]

    str         r10, [sp, #l_coef_ptr]      ; coef_probs = coef_probs[type]
    b           BLOCK_LOOP

ln1_decode_mb_to
    ldr         r2, [sp, #l_bc]
    mov         r0, #0
    nop

    str         r8, [r2, #bool_decoder_user_buffer]
    str         r5, [r2, #bool_decoder_count]
    str         r4, [r2, #bool_decoder_value]
    str         r6, [r2, #bool_decoder_range]

    add         sp, sp, #l_stacksize
    ldmia       sp!, {r4 - r11, pc}

    ENDP  ; |vp8_decode_mb_tokens_v6|

    END
