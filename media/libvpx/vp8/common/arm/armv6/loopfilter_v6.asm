;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT |vp8_loop_filter_horizontal_edge_armv6|
    EXPORT |vp8_mbloop_filter_horizontal_edge_armv6|
    EXPORT |vp8_loop_filter_vertical_edge_armv6|
    EXPORT |vp8_mbloop_filter_vertical_edge_armv6|

    AREA    |.text|, CODE, READONLY  ; name this block of code

    MACRO
    TRANSPOSE_MATRIX $a0, $a1, $a2, $a3, $b0, $b1, $b2, $b3
    ; input: $a0, $a1, $a2, $a3; output: $b0, $b1, $b2, $b3
    ; a0: 03 02 01 00
    ; a1: 13 12 11 10
    ; a2: 23 22 21 20
    ; a3: 33 32 31 30
    ;     b3 b2 b1 b0

    uxtb16      $b1, $a1                    ; xx 12 xx 10
    uxtb16      $b0, $a0                    ; xx 02 xx 00
    uxtb16      $b3, $a3                    ; xx 32 xx 30
    uxtb16      $b2, $a2                    ; xx 22 xx 20
    orr         $b1, $b0, $b1, lsl #8       ; 12 02 10 00
    orr         $b3, $b2, $b3, lsl #8       ; 32 22 30 20

    uxtb16      $a1, $a1, ror #8            ; xx 13 xx 11
    uxtb16      $a3, $a3, ror #8            ; xx 33 xx 31
    uxtb16      $a0, $a0, ror #8            ; xx 03 xx 01
    uxtb16      $a2, $a2, ror #8            ; xx 23 xx 21
    orr         $a0, $a0, $a1, lsl #8       ; 13 03 11 01
    orr         $a2, $a2, $a3, lsl #8       ; 33 23 31 21

    pkhtb       $b2, $b3, $b1, asr #16      ; 32 22 12 02   -- p1
    pkhbt       $b0, $b1, $b3, lsl #16      ; 30 20 10 00   -- p3

    pkhtb       $b3, $a2, $a0, asr #16      ; 33 23 13 03   -- p0
    pkhbt       $b1, $a0, $a2, lsl #16      ; 31 21 11 01   -- p2
    MEND


src         RN  r0
pstep       RN  r1
count       RN  r5

;r0     unsigned char *src_ptr,
;r1     int src_pixel_step,
;r2     const char *flimit,
;r3     const char *limit,
;stack  const char *thresh,
;stack  int  count

;Note: All 16 elements in flimit are equal. So, in the code, only one load is needed
;for flimit. Same way applies to limit and thresh.

;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
|vp8_loop_filter_horizontal_edge_armv6| PROC
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    stmdb       sp!, {r4 - r11, lr}

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines
    ldr         count, [sp, #40]            ; count for 8-in-parallel
    ldr         r6, [sp, #36]               ; load thresh address
    sub         sp, sp, #16                 ; create temp buffer

    ldr         r9, [src], pstep            ; p3
    ldr         r4, [r2], #4                ; flimit
    ldr         r10, [src], pstep           ; p2
    ldr         r2, [r3], #4                ; limit
    ldr         r11, [src], pstep           ; p1
    uadd8       r4, r4, r4                  ; flimit * 2
    ldr         r3, [r6], #4                ; thresh
    mov         count, count, lsl #1        ; 4-in-parallel
    uadd8       r4, r4, r2                  ; flimit * 2 + limit

|Hnext8|
    ; vp8_filter_mask() function
    ; calculate breakout conditions
    ldr         r12, [src], pstep           ; p0

    uqsub8      r6, r9, r10                 ; p3 - p2
    uqsub8      r7, r10, r9                 ; p2 - p3
    uqsub8      r8, r10, r11                ; p2 - p1
    uqsub8      r10, r11, r10               ; p1 - p2

    orr         r6, r6, r7                  ; abs (p3-p2)
    orr         r8, r8, r10                 ; abs (p2-p1)
    uqsub8      lr, r6, r2                  ; compare to limit. lr: vp8_filter_mask
    uqsub8      r8, r8, r2                  ; compare to limit
    uqsub8      r6, r11, r12                ; p1 - p0
    orr         lr, lr, r8
    uqsub8      r7, r12, r11                ; p0 - p1
    ldr         r9, [src], pstep            ; q0
    ldr         r10, [src], pstep           ; q1
    orr         r6, r6, r7                  ; abs (p1-p0)
    uqsub8      r7, r6, r2                  ; compare to limit
    uqsub8      r8, r6, r3                  ; compare to thresh  -- save r8 for later
    orr         lr, lr, r7

    uqsub8      r6, r11, r10                ; p1 - q1
    uqsub8      r7, r10, r11                ; q1 - p1
    uqsub8      r11, r12, r9                ; p0 - q0
    uqsub8      r12, r9, r12                ; q0 - p0
    orr         r6, r6, r7                  ; abs (p1-q1)
    ldr         r7, c0x7F7F7F7F
    orr         r12, r11, r12               ; abs (p0-q0)
    ldr         r11, [src], pstep           ; q2
    uqadd8      r12, r12, r12               ; abs (p0-q0) * 2
    and         r6, r7, r6, lsr #1          ; abs (p1-q1) / 2
    uqsub8      r7, r9, r10                 ; q0 - q1
    uqadd8      r12, r12, r6                ; abs (p0-q0)*2 + abs (p1-q1)/2
    uqsub8      r6, r10, r9                 ; q1 - q0
    uqsub8      r12, r12, r4                ; compare to flimit
    uqsub8      r9, r11, r10                ; q2 - q1

    orr         lr, lr, r12

    ldr         r12, [src], pstep           ; q3
    uqsub8      r10, r10, r11               ; q1 - q2
    orr         r6, r7, r6                  ; abs (q1-q0)
    orr         r10, r9, r10                ; abs (q2-q1)
    uqsub8      r7, r6, r2                  ; compare to limit
    uqsub8      r10, r10, r2                ; compare to limit
    uqsub8      r6, r6, r3                  ; compare to thresh -- save r6 for later
    orr         lr, lr, r7
    orr         lr, lr, r10

    uqsub8      r10, r12, r11               ; q3 - q2
    uqsub8      r9, r11, r12                ; q2 - q3

    mvn         r11, #0                     ; r11 == -1

    orr         r10, r10, r9                ; abs (q3-q2)
    uqsub8      r10, r10, r2                ; compare to limit

    mov         r12, #0
    orr         lr, lr, r10
    sub         src, src, pstep, lsl #2

    usub8       lr, r12, lr                 ; use usub8 instead of ssub8
    sel         lr, r11, r12                ; filter mask: lr

    cmp         lr, #0
    beq         hskip_filter                 ; skip filtering

    sub         src, src, pstep, lsl #1     ; move src pointer down by 6 lines

    ;vp8_hevmask() function
    ;calculate high edge variance
    orr         r10, r6, r8                 ; calculate vp8_hevmask

    ldr         r7, [src], pstep            ; p1

    usub8       r10, r12, r10               ; use usub8 instead of ssub8
    sel         r6, r12, r11                ; obtain vp8_hevmask: r6

    ;vp8_filter() function
    ldr         r8, [src], pstep            ; p0
    ldr         r12, c0x80808080
    ldr         r9, [src], pstep            ; q0
    ldr         r10, [src], pstep           ; q1

    eor         r7, r7, r12                 ; p1 offset to convert to a signed value
    eor         r8, r8, r12                 ; p0 offset to convert to a signed value
    eor         r9, r9, r12                 ; q0 offset to convert to a signed value
    eor         r10, r10, r12               ; q1 offset to convert to a signed value

    str         r9, [sp]                    ; store qs0 temporarily
    str         r8, [sp, #4]                ; store ps0 temporarily
    str         r10, [sp, #8]               ; store qs1 temporarily
    str         r7, [sp, #12]               ; store ps1 temporarily

    qsub8       r7, r7, r10                 ; vp8_signed_char_clamp(ps1-qs1)
    qsub8       r8, r9, r8                  ; vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))

    and         r7, r7, r6                  ; vp8_filter (r7) &= hev

    qadd8       r7, r7, r8
    ldr         r9, c0x03030303             ; r9 = 3 --modified for vp8

    qadd8       r7, r7, r8
    ldr         r10, c0x04040404

    qadd8       r7, r7, r8
    and         r7, r7, lr                  ; vp8_filter &= mask;

    ;modify code for vp8 -- Filter1 = vp8_filter (r7)
    qadd8       r8 , r7 , r9                ; Filter2 (r8) = vp8_signed_char_clamp(vp8_filter+3)
    qadd8       r7 , r7 , r10               ; vp8_filter = vp8_signed_char_clamp(vp8_filter+4)

    mov         r9, #0
    shadd8      r8 , r8 , r9                ; Filter2 >>= 3
    shadd8      r7 , r7 , r9                ; vp8_filter >>= 3
    shadd8      r8 , r8 , r9
    shadd8      r7 , r7 , r9
    shadd8      lr , r8 , r9                ; lr: Filter2
    shadd8      r7 , r7 , r9                ; r7: filter

    ;usub8      lr, r8, r10                 ; s = (s==4)*-1
    ;sel        lr, r11, r9
    ;usub8      r8, r10, r8
    ;sel        r8, r11, r9
    ;and        r8, r8, lr                  ; -1 for each element that equals 4

    ;calculate output
    ;qadd8      lr, r8, r7                  ; u = vp8_signed_char_clamp(s + vp8_filter)

    ldr         r8, [sp]                    ; load qs0
    ldr         r9, [sp, #4]                ; load ps0

    ldr         r10, c0x01010101

    qsub8       r8 ,r8, r7                  ; u = vp8_signed_char_clamp(qs0 - vp8_filter)
    qadd8       r9, r9, lr                  ; u = vp8_signed_char_clamp(ps0 + Filter2)

    ;end of modification for vp8

    mov         lr, #0
    sadd8       r7, r7 , r10                ; vp8_filter += 1
    shadd8      r7, r7, lr                  ; vp8_filter >>= 1

    ldr         r11, [sp, #12]              ; load ps1
    ldr         r10, [sp, #8]               ; load qs1

    bic         r7, r7, r6                  ; vp8_filter &= ~hev
    sub         src, src, pstep, lsl #2

    qadd8       r11, r11, r7                ; u = vp8_signed_char_clamp(ps1 + vp8_filter)
    qsub8       r10, r10,r7                 ; u = vp8_signed_char_clamp(qs1 - vp8_filter)

    eor         r11, r11, r12               ; *op1 = u^0x80
    str         r11, [src], pstep           ; store op1
    eor         r9, r9, r12                 ; *op0 = u^0x80
    str         r9, [src], pstep            ; store op0 result
    eor         r8, r8, r12                 ; *oq0 = u^0x80
    str         r8, [src], pstep            ; store oq0 result
    eor         r10, r10, r12               ; *oq1 = u^0x80
    str         r10, [src], pstep           ; store oq1

    sub         src, src, pstep, lsl #1

|hskip_filter|
    add         src, src, #4
    sub         src, src, pstep, lsl #2

    subs        count, count, #1

    ;pld            [src]
    ;pld            [src, pstep]
    ;pld            [src, pstep, lsl #1]
    ;pld            [src, pstep, lsl #2]
    ;pld            [src, pstep, lsl #3]

    ldrne       r9, [src], pstep            ; p3
    ldrne       r10, [src], pstep           ; p2
    ldrne       r11, [src], pstep           ; p1

    bne         Hnext8

    add         sp, sp, #16
    ldmia       sp!, {r4 - r11, pc}
    ENDP        ; |vp8_loop_filter_horizontal_edge_armv6|


;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
|vp8_mbloop_filter_horizontal_edge_armv6| PROC
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    stmdb       sp!, {r4 - r11, lr}

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines
    ldr         count, [sp, #40]            ; count for 8-in-parallel
    ldr         r6, [sp, #36]               ; load thresh address
    sub         sp, sp, #16                 ; create temp buffer

    ldr         r9, [src], pstep            ; p3
    ldr         r4, [r2], #4                ; flimit
    ldr         r10, [src], pstep           ; p2
    ldr         r2, [r3], #4                ; limit
    ldr         r11, [src], pstep           ; p1
    uadd8       r4, r4, r4                  ; flimit * 2
    ldr         r3, [r6], #4                ; thresh
    mov         count, count, lsl #1        ; 4-in-parallel
    uadd8       r4, r4, r2                  ; flimit * 2 + limit

|MBHnext8|

    ; vp8_filter_mask() function
    ; calculate breakout conditions
    ldr         r12, [src], pstep           ; p0

    uqsub8      r6, r9, r10                 ; p3 - p2
    uqsub8      r7, r10, r9                 ; p2 - p3
    uqsub8      r8, r10, r11                ; p2 - p1
    uqsub8      r10, r11, r10               ; p1 - p2

    orr         r6, r6, r7                  ; abs (p3-p2)
    orr         r8, r8, r10                 ; abs (p2-p1)
    uqsub8      lr, r6, r2                  ; compare to limit. lr: vp8_filter_mask
    uqsub8      r8, r8, r2                  ; compare to limit

    uqsub8      r6, r11, r12                ; p1 - p0
    orr         lr, lr, r8
    uqsub8      r7, r12, r11                ; p0 - p1
    ldr         r9, [src], pstep            ; q0
    ldr         r10, [src], pstep           ; q1
    orr         r6, r6, r7                  ; abs (p1-p0)
    uqsub8      r7, r6, r2                  ; compare to limit
    uqsub8      r8, r6, r3                  ; compare to thresh  -- save r8 for later
    orr         lr, lr, r7

    uqsub8      r6, r11, r10                ; p1 - q1
    uqsub8      r7, r10, r11                ; q1 - p1
    uqsub8      r11, r12, r9                ; p0 - q0
    uqsub8      r12, r9, r12                ; q0 - p0
    orr         r6, r6, r7                  ; abs (p1-q1)
    ldr         r7, c0x7F7F7F7F
    orr         r12, r11, r12               ; abs (p0-q0)
    ldr         r11, [src], pstep           ; q2
    uqadd8      r12, r12, r12               ; abs (p0-q0) * 2
    and         r6, r7, r6, lsr #1          ; abs (p1-q1) / 2
    uqsub8      r7, r9, r10                 ; q0 - q1
    uqadd8      r12, r12, r6                ; abs (p0-q0)*2 + abs (p1-q1)/2
    uqsub8      r6, r10, r9                 ; q1 - q0
    uqsub8      r12, r12, r4                ; compare to flimit
    uqsub8      r9, r11, r10                ; q2 - q1

    orr         lr, lr, r12

    ldr         r12, [src], pstep           ; q3

    uqsub8      r10, r10, r11               ; q1 - q2
    orr         r6, r7, r6                  ; abs (q1-q0)
    orr         r10, r9, r10                ; abs (q2-q1)
    uqsub8      r7, r6, r2                  ; compare to limit
    uqsub8      r10, r10, r2                ; compare to limit
    uqsub8      r6, r6, r3                  ; compare to thresh -- save r6 for later
    orr         lr, lr, r7
    orr         lr, lr, r10

    uqsub8      r10, r12, r11               ; q3 - q2
    uqsub8      r9, r11, r12                ; q2 - q3

    mvn         r11, #0                     ; r11 == -1

    orr         r10, r10, r9                ; abs (q3-q2)
    uqsub8      r10, r10, r2                ; compare to limit

    mov         r12, #0

    orr         lr, lr, r10

    usub8       lr, r12, lr                 ; use usub8 instead of ssub8
    sel         lr, r11, r12                ; filter mask: lr

    cmp         lr, #0
    beq         mbhskip_filter               ; skip filtering

    ;vp8_hevmask() function
    ;calculate high edge variance
    sub         src, src, pstep, lsl #2     ; move src pointer down by 6 lines
    sub         src, src, pstep, lsl #1

    orr         r10, r6, r8
    ldr         r7, [src], pstep            ; p1

    usub8       r10, r12, r10
    sel         r6, r12, r11                ; hev mask: r6

    ;vp8_mbfilter() function
    ;p2, q2 are only needed at the end. Don't need to load them in now.
    ldr         r8, [src], pstep            ; p0
    ldr         r12, c0x80808080
    ldr         r9, [src], pstep            ; q0
    ldr         r10, [src]                  ; q1

    eor         r7, r7, r12                 ; ps1
    eor         r8, r8, r12                 ; ps0
    eor         r9, r9, r12                 ; qs0
    eor         r10, r10, r12               ; qs1

    qsub8       r12, r9, r8                 ; vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    str         r7, [sp, #12]               ; store ps1 temporarily
    qsub8       r7, r7, r10                 ; vp8_signed_char_clamp(ps1-qs1)
    str         r10, [sp, #8]               ; store qs1 temporarily
    qadd8       r7, r7, r12
    str         r9, [sp]                    ; store qs0 temporarily
    qadd8       r7, r7, r12
    str         r8, [sp, #4]                ; store ps0 temporarily
    qadd8       r7, r7, r12                 ; vp8_filter: r7

    ldr         r10, c0x03030303            ; r10 = 3 --modified for vp8
    ldr         r9, c0x04040404

    and         r7, r7, lr                  ; vp8_filter &= mask (lr is free)

    mov         r12, r7                     ; Filter2: r12
    and         r12, r12, r6                ; Filter2 &= hev

    ;modify code for vp8
    ;save bottom 3 bits so that we round one side +4 and the other +3
    qadd8       r8 , r12 , r9               ; Filter1 (r8) = vp8_signed_char_clamp(Filter2+4)
    qadd8       r12 , r12 , r10             ; Filter2 (r12) = vp8_signed_char_clamp(Filter2+3)

    mov         r10, #0
    shadd8      r8 , r8 , r10               ; Filter1 >>= 3
    shadd8      r12 , r12 , r10             ; Filter2 >>= 3
    shadd8      r8 , r8 , r10
    shadd8      r12 , r12 , r10
    shadd8      r8 , r8 , r10               ; r8: Filter1
    shadd8      r12 , r12 , r10             ; r12: Filter2

    ldr         r9, [sp]                    ; load qs0
    ldr         r11, [sp, #4]               ; load ps0

    qsub8       r9 , r9, r8                 ; qs0 = vp8_signed_char_clamp(qs0 - Filter1)
    qadd8       r11, r11, r12               ; ps0 = vp8_signed_char_clamp(ps0 + Filter2)

    ;save bottom 3 bits so that we round one side +4 and the other +3
    ;and            r8, r12, r10                ; s = Filter2 & 7 (s: r8)
    ;qadd8      r12 , r12 , r9              ; Filter2 = vp8_signed_char_clamp(Filter2+4)
    ;mov            r10, #0
    ;shadd8     r12 , r12 , r10             ; Filter2 >>= 3
    ;usub8      lr, r8, r9                  ; s = (s==4)*-1
    ;sel            lr, r11, r10
    ;shadd8     r12 , r12 , r10
    ;usub8      r8, r9, r8
    ;sel            r8, r11, r10
    ;ldr            r9, [sp]                    ; load qs0
    ;ldr            r11, [sp, #4]               ; load ps0
    ;shadd8     r12 , r12 , r10
    ;and            r8, r8, lr                  ; -1 for each element that equals 4
    ;qadd8      r10, r8, r12                ; u = vp8_signed_char_clamp(s + Filter2)
    ;qsub8      r9 , r9, r12                ; qs0 = vp8_signed_char_clamp(qs0 - Filter2)
    ;qadd8      r11, r11, r10               ; ps0 = vp8_signed_char_clamp(ps0 + u)

    ;end of modification for vp8

    bic         r12, r7, r6                 ; vp8_filter &= ~hev    ( r6 is free)
    ;mov        r12, r7

    ;roughly 3/7th difference across boundary
    mov         lr, #0x1b                   ; 27
    mov         r7, #0x3f                   ; 63

    sxtb16      r6, r12
    sxtb16      r10, r12, ror #8
    smlabb      r8, r6, lr, r7
    smlatb      r6, r6, lr, r7
    smlabb      r7, r10, lr, r7
    smultb      r10, r10, lr
    ssat        r8, #8, r8, asr #7
    ssat        r6, #8, r6, asr #7
    add         r10, r10, #63
    ssat        r7, #8, r7, asr #7
    ssat        r10, #8, r10, asr #7

    ldr         lr, c0x80808080

    pkhbt       r6, r8, r6, lsl #16
    pkhbt       r10, r7, r10, lsl #16
    uxtb16      r6, r6
    uxtb16      r10, r10

    sub         src, src, pstep

    orr         r10, r6, r10, lsl #8        ; u = vp8_signed_char_clamp((63 + Filter2 * 27)>>7)

    qsub8       r8, r9, r10                 ; s = vp8_signed_char_clamp(qs0 - u)
    qadd8       r10, r11, r10               ; s = vp8_signed_char_clamp(ps0 + u)
    eor         r8, r8, lr                  ; *oq0 = s^0x80
    str         r8, [src]                   ; store *oq0
    sub         src, src, pstep
    eor         r10, r10, lr                ; *op0 = s^0x80
    str         r10, [src]                  ; store *op0

    ;roughly 2/7th difference across boundary
    mov         lr, #0x12                   ; 18
    mov         r7, #0x3f                   ; 63

    sxtb16      r6, r12
    sxtb16      r10, r12, ror #8
    smlabb      r8, r6, lr, r7
    smlatb      r6, r6, lr, r7
    smlabb      r9, r10, lr, r7
    smlatb      r10, r10, lr, r7
    ssat        r8, #8, r8, asr #7
    ssat        r6, #8, r6, asr #7
    ssat        r9, #8, r9, asr #7
    ssat        r10, #8, r10, asr #7

    ldr         lr, c0x80808080

    pkhbt       r6, r8, r6, lsl #16
    pkhbt       r10, r9, r10, lsl #16

    ldr         r9, [sp, #8]                ; load qs1
    ldr         r11, [sp, #12]              ; load ps1

    uxtb16      r6, r6
    uxtb16      r10, r10

    sub         src, src, pstep

    orr         r10, r6, r10, lsl #8        ; u = vp8_signed_char_clamp((63 + Filter2 * 18)>>7)

    qadd8       r11, r11, r10               ; s = vp8_signed_char_clamp(ps1 + u)
    qsub8       r8, r9, r10                 ; s = vp8_signed_char_clamp(qs1 - u)
    eor         r11, r11, lr                ; *op1 = s^0x80
    str         r11, [src], pstep           ; store *op1
    eor         r8, r8, lr                  ; *oq1 = s^0x80
    add         src, src, pstep, lsl #1

    mov         r7, #0x3f                   ; 63

    str         r8, [src], pstep            ; store *oq1

    ;roughly 1/7th difference across boundary
    mov         lr, #0x9                    ; 9
    ldr         r9, [src]                   ; load q2

    sxtb16      r6, r12
    sxtb16      r10, r12, ror #8
    smlabb      r8, r6, lr, r7
    smlatb      r6, r6, lr, r7
    smlabb      r12, r10, lr, r7
    smlatb      r10, r10, lr, r7
    ssat        r8, #8, r8, asr #7
    ssat        r6, #8, r6, asr #7
    ssat        r12, #8, r12, asr #7
    ssat        r10, #8, r10, asr #7

    sub         src, src, pstep, lsl #2

    pkhbt       r6, r8, r6, lsl #16
    pkhbt       r10, r12, r10, lsl #16

    sub         src, src, pstep
    ldr         lr, c0x80808080

    ldr         r11, [src]                  ; load p2

    uxtb16      r6, r6
    uxtb16      r10, r10

    eor         r9, r9, lr
    eor         r11, r11, lr

    orr         r10, r6, r10, lsl #8        ; u = vp8_signed_char_clamp((63 + Filter2 * 9)>>7)

    qadd8       r8, r11, r10                ; s = vp8_signed_char_clamp(ps2 + u)
    qsub8       r10, r9, r10                ; s = vp8_signed_char_clamp(qs2 - u)
    eor         r8, r8, lr                  ; *op2 = s^0x80
    str         r8, [src], pstep, lsl #2    ; store *op2
    add         src, src, pstep
    eor         r10, r10, lr                ; *oq2 = s^0x80
    str         r10, [src], pstep, lsl #1   ; store *oq2

|mbhskip_filter|
    add         src, src, #4
    sub         src, src, pstep, lsl #3
    subs        count, count, #1

    ldrne       r9, [src], pstep            ; p3
    ldrne       r10, [src], pstep           ; p2
    ldrne       r11, [src], pstep           ; p1

    bne         MBHnext8

    add         sp, sp, #16
    ldmia       sp!, {r4 - r11, pc}
    ENDP        ; |vp8_mbloop_filter_horizontal_edge_armv6|


;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
|vp8_loop_filter_vertical_edge_armv6| PROC
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    stmdb       sp!, {r4 - r11, lr}

    sub         src, src, #4                ; move src pointer down by 4
    ldr         count, [sp, #40]            ; count for 8-in-parallel
    ldr         r12, [sp, #36]              ; load thresh address
    sub         sp, sp, #16                 ; create temp buffer

    ldr         r6, [src], pstep            ; load source data
    ldr         r4, [r2], #4                ; flimit
    ldr         r7, [src], pstep
    ldr         r2, [r3], #4                ; limit
    ldr         r8, [src], pstep
    uadd8       r4, r4, r4                  ; flimit * 2
    ldr         r3, [r12], #4               ; thresh
    ldr         lr, [src], pstep
    mov         count, count, lsl #1        ; 4-in-parallel
    uadd8       r4, r4, r2                  ; flimit * 2 + limit

|Vnext8|

    ; vp8_filter_mask() function
    ; calculate breakout conditions
    ; transpose the source data for 4-in-parallel operation
    TRANSPOSE_MATRIX r6, r7, r8, lr, r9, r10, r11, r12

    uqsub8      r7, r9, r10                 ; p3 - p2
    uqsub8      r8, r10, r9                 ; p2 - p3
    uqsub8      r9, r10, r11                ; p2 - p1
    uqsub8      r10, r11, r10               ; p1 - p2
    orr         r7, r7, r8                  ; abs (p3-p2)
    orr         r10, r9, r10                ; abs (p2-p1)
    uqsub8      lr, r7, r2                  ; compare to limit. lr: vp8_filter_mask
    uqsub8      r10, r10, r2                ; compare to limit

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines

    orr         lr, lr, r10

    uqsub8      r6, r11, r12                ; p1 - p0
    uqsub8      r7, r12, r11                ; p0 - p1
    add         src, src, #4                ; move src pointer up by 4
    orr         r6, r6, r7                  ; abs (p1-p0)
    str         r11, [sp, #12]              ; save p1
    uqsub8      r10, r6, r2                 ; compare to limit
    uqsub8      r11, r6, r3                 ; compare to thresh
    orr         lr, lr, r10

    ; transpose uses 8 regs(r6 - r12 and lr). Need to save reg value now
    ; transpose the source data for 4-in-parallel operation
    ldr         r6, [src], pstep            ; load source data
    str         r11, [sp]                   ; push r11 to stack
    ldr         r7, [src], pstep
    str         r12, [sp, #4]               ; save current reg before load q0 - q3 data
    ldr         r8, [src], pstep
    str         lr, [sp, #8]
    ldr         lr, [src], pstep

    TRANSPOSE_MATRIX r6, r7, r8, lr, r9, r10, r11, r12

    ldr         lr, [sp, #8]                ; load back (f)limit accumulator

    uqsub8      r6, r12, r11                ; q3 - q2
    uqsub8      r7, r11, r12                ; q2 - q3
    uqsub8      r12, r11, r10               ; q2 - q1
    uqsub8      r11, r10, r11               ; q1 - q2
    orr         r6, r6, r7                  ; abs (q3-q2)
    orr         r7, r12, r11                ; abs (q2-q1)
    uqsub8      r6, r6, r2                  ; compare to limit
    uqsub8      r7, r7, r2                  ; compare to limit
    ldr         r11, [sp, #4]               ; load back p0
    ldr         r12, [sp, #12]              ; load back p1
    orr         lr, lr, r6
    orr         lr, lr, r7

    uqsub8      r6, r11, r9                 ; p0 - q0
    uqsub8      r7, r9, r11                 ; q0 - p0
    uqsub8      r8, r12, r10                ; p1 - q1
    uqsub8      r11, r10, r12               ; q1 - p1
    orr         r6, r6, r7                  ; abs (p0-q0)
    ldr         r7, c0x7F7F7F7F
    orr         r8, r8, r11                 ; abs (p1-q1)
    uqadd8      r6, r6, r6                  ; abs (p0-q0) * 2
    and         r8, r7, r8, lsr #1          ; abs (p1-q1) / 2
    uqsub8      r11, r10, r9                ; q1 - q0
    uqadd8      r6, r8, r6                  ; abs (p0-q0)*2 + abs (p1-q1)/2
    uqsub8      r12, r9, r10                ; q0 - q1
    uqsub8      r6, r6, r4                  ; compare to flimit

    orr         r9, r11, r12                ; abs (q1-q0)
    uqsub8      r8, r9, r2                  ; compare to limit
    uqsub8      r10, r9, r3                 ; compare to thresh
    orr         lr, lr, r6
    orr         lr, lr, r8

    mvn         r11, #0                     ; r11 == -1
    mov         r12, #0

    usub8       lr, r12, lr
    ldr         r9, [sp]                    ; load the compared result
    sel         lr, r11, r12                ; filter mask: lr

    cmp         lr, #0
    beq         vskip_filter                 ; skip filtering

    ;vp8_hevmask() function
    ;calculate high edge variance

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines

    orr         r9, r9, r10

    ldrh        r7, [src, #-2]
    ldrh        r8, [src], pstep

    usub8       r9, r12, r9
    sel         r6, r12, r11                ; hev mask: r6

    ;vp8_filter() function
    ; load soure data to r6, r11, r12, lr
    ldrh        r9, [src, #-2]
    ldrh        r10, [src], pstep

    pkhbt       r12, r7, r8, lsl #16

    ldrh        r7, [src, #-2]
    ldrh        r8, [src], pstep

    pkhbt       r11, r9, r10, lsl #16

    ldrh        r9, [src, #-2]
    ldrh        r10, [src], pstep

    ; Transpose needs 8 regs(r6 - r12, and lr). Save r6 and lr first
    str         r6, [sp]
    str         lr, [sp, #4]

    pkhbt       r6, r7, r8, lsl #16
    pkhbt       lr, r9, r10, lsl #16

    ;transpose r12, r11, r6, lr to r7, r8, r9, r10
    TRANSPOSE_MATRIX r12, r11, r6, lr, r7, r8, r9, r10

    ;load back hev_mask r6 and filter_mask lr
    ldr         r12, c0x80808080
    ldr         r6, [sp]
    ldr         lr, [sp, #4]

    eor         r7, r7, r12                 ; p1 offset to convert to a signed value
    eor         r8, r8, r12                 ; p0 offset to convert to a signed value
    eor         r9, r9, r12                 ; q0 offset to convert to a signed value
    eor         r10, r10, r12               ; q1 offset to convert to a signed value

    str         r9, [sp]                    ; store qs0 temporarily
    str         r8, [sp, #4]                ; store ps0 temporarily
    str         r10, [sp, #8]               ; store qs1 temporarily
    str         r7, [sp, #12]               ; store ps1 temporarily

    qsub8       r7, r7, r10                 ; vp8_signed_char_clamp(ps1-qs1)
    qsub8       r8, r9, r8                  ; vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))

    and         r7, r7, r6                  ;  vp8_filter (r7) &= hev (r7 : filter)

    qadd8       r7, r7, r8
    ldr         r9, c0x03030303             ; r9 = 3 --modified for vp8

    qadd8       r7, r7, r8
    ldr         r10, c0x04040404

    qadd8       r7, r7, r8
    ;mvn         r11, #0                     ; r11 == -1

    and         r7, r7, lr                  ; vp8_filter &= mask

    ;modify code for vp8 -- Filter1 = vp8_filter (r7)
    qadd8       r8 , r7 , r9                ; Filter2 (r8) = vp8_signed_char_clamp(vp8_filter+3)
    qadd8       r7 , r7 , r10               ; vp8_filter = vp8_signed_char_clamp(vp8_filter+4)

    mov         r9, #0
    shadd8      r8 , r8 , r9                ; Filter2 >>= 3
    shadd8      r7 , r7 , r9                ; vp8_filter >>= 3
    shadd8      r8 , r8 , r9
    shadd8      r7 , r7 , r9
    shadd8      lr , r8 , r9                ; lr: filter2
    shadd8      r7 , r7 , r9                ; r7: filter

    ;usub8      lr, r8, r10                 ; s = (s==4)*-1
    ;sel            lr, r11, r9
    ;usub8      r8, r10, r8
    ;sel            r8, r11, r9
    ;and            r8, r8, lr                  ; -1 for each element that equals 4 -- r8: s

    ;calculate output
    ;qadd8      lr, r8, r7                  ; u = vp8_signed_char_clamp(s + vp8_filter)

    ldr         r8, [sp]                    ; load qs0
    ldr         r9, [sp, #4]                ; load ps0

    ldr         r10, c0x01010101

    qsub8       r8, r8, r7                  ; u = vp8_signed_char_clamp(qs0 - vp8_filter)
    qadd8       r9, r9, lr                  ; u = vp8_signed_char_clamp(ps0 + Filter2)
    ;end of modification for vp8

    eor         r8, r8, r12
    eor         r9, r9, r12

    mov         lr, #0

    sadd8       r7, r7, r10
    shadd8      r7, r7, lr

    ldr         r10, [sp, #8]               ; load qs1
    ldr         r11, [sp, #12]              ; load ps1

    bic         r7, r7, r6                  ; r7: vp8_filter

    qsub8       r10 , r10, r7               ; u = vp8_signed_char_clamp(qs1 - vp8_filter)
    qadd8       r11, r11, r7                ; u = vp8_signed_char_clamp(ps1 + vp8_filter)
    eor         r10, r10, r12
    eor         r11, r11, r12

    sub         src, src, pstep, lsl #2

    ;we can use TRANSPOSE_MATRIX macro to transpose output - input: q1, q0, p0, p1
    ;output is b0, b1, b2, b3
    ;b0: 03 02 01 00
    ;b1: 13 12 11 10
    ;b2: 23 22 21 20
    ;b3: 33 32 31 30
    ;    p1 p0 q0 q1
    ;   (a3 a2 a1 a0)
    TRANSPOSE_MATRIX r11, r9, r8, r10, r6, r7, r12, lr

    strh        r6, [src, #-2]              ; store the result
    mov         r6, r6, lsr #16
    strh        r6, [src], pstep

    strh        r7, [src, #-2]
    mov         r7, r7, lsr #16
    strh        r7, [src], pstep

    strh        r12, [src, #-2]
    mov         r12, r12, lsr #16
    strh        r12, [src], pstep

    strh        lr, [src, #-2]
    mov         lr, lr, lsr #16
    strh        lr, [src], pstep

|vskip_filter|
    sub         src, src, #4
    subs        count, count, #1

    ldrne       r6, [src], pstep            ; load source data
    ldrne       r7, [src], pstep
    ldrne       r8, [src], pstep
    ldrne       lr, [src], pstep

    bne         Vnext8

    add         sp, sp, #16

    ldmia       sp!, {r4 - r11, pc}
    ENDP        ; |vp8_loop_filter_vertical_edge_armv6|



;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
|vp8_mbloop_filter_vertical_edge_armv6| PROC
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
    stmdb       sp!, {r4 - r11, lr}

    sub         src, src, #4                ; move src pointer down by 4
    ldr         count, [sp, #40]            ; count for 8-in-parallel
    ldr         r12, [sp, #36]              ; load thresh address
    sub         sp, sp, #16                 ; create temp buffer

    ldr         r6, [src], pstep            ; load source data
    ldr         r4, [r2], #4                ; flimit
    ldr         r7, [src], pstep
    ldr         r2, [r3], #4                ; limit
    ldr         r8, [src], pstep
    uadd8       r4, r4, r4                  ; flimit * 2
    ldr         r3, [r12], #4               ; thresh
    ldr         lr, [src], pstep
    mov         count, count, lsl #1        ; 4-in-parallel
    uadd8       r4, r4, r2                  ; flimit * 2 + limit

|MBVnext8|
    ; vp8_filter_mask() function
    ; calculate breakout conditions
    ; transpose the source data for 4-in-parallel operation
    TRANSPOSE_MATRIX r6, r7, r8, lr, r9, r10, r11, r12

    uqsub8      r7, r9, r10                 ; p3 - p2
    uqsub8      r8, r10, r9                 ; p2 - p3
    uqsub8      r9, r10, r11                ; p2 - p1
    uqsub8      r10, r11, r10               ; p1 - p2
    orr         r7, r7, r8                  ; abs (p3-p2)
    orr         r10, r9, r10                ; abs (p2-p1)
    uqsub8      lr, r7, r2                  ; compare to limit. lr: vp8_filter_mask
    uqsub8      r10, r10, r2                ; compare to limit

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines

    orr         lr, lr, r10

    uqsub8      r6, r11, r12                ; p1 - p0
    uqsub8      r7, r12, r11                ; p0 - p1
    add         src, src, #4                ; move src pointer up by 4
    orr         r6, r6, r7                  ; abs (p1-p0)
    str         r11, [sp, #12]              ; save p1
    uqsub8      r10, r6, r2                 ; compare to limit
    uqsub8      r11, r6, r3                 ; compare to thresh
    orr         lr, lr, r10

    ; transpose uses 8 regs(r6 - r12 and lr). Need to save reg value now
    ; transpose the source data for 4-in-parallel operation
    ldr         r6, [src], pstep            ; load source data
    str         r11, [sp]                   ; push r11 to stack
    ldr         r7, [src], pstep
    str         r12, [sp, #4]               ; save current reg before load q0 - q3 data
    ldr         r8, [src], pstep
    str         lr, [sp, #8]
    ldr         lr, [src], pstep

    TRANSPOSE_MATRIX r6, r7, r8, lr, r9, r10, r11, r12

    ldr         lr, [sp, #8]                ; load back (f)limit accumulator

    uqsub8      r6, r12, r11                ; q3 - q2
    uqsub8      r7, r11, r12                ; q2 - q3
    uqsub8      r12, r11, r10               ; q2 - q1
    uqsub8      r11, r10, r11               ; q1 - q2
    orr         r6, r6, r7                  ; abs (q3-q2)
    orr         r7, r12, r11                ; abs (q2-q1)
    uqsub8      r6, r6, r2                  ; compare to limit
    uqsub8      r7, r7, r2                  ; compare to limit
    ldr         r11, [sp, #4]               ; load back p0
    ldr         r12, [sp, #12]              ; load back p1
    orr         lr, lr, r6
    orr         lr, lr, r7

    uqsub8      r6, r11, r9                 ; p0 - q0
    uqsub8      r7, r9, r11                 ; q0 - p0
    uqsub8      r8, r12, r10                ; p1 - q1
    uqsub8      r11, r10, r12               ; q1 - p1
    orr         r6, r6, r7                  ; abs (p0-q0)
    ldr         r7, c0x7F7F7F7F
    orr         r8, r8, r11                 ; abs (p1-q1)
    uqadd8      r6, r6, r6                  ; abs (p0-q0) * 2
    and         r8, r7, r8, lsr #1          ; abs (p1-q1) / 2
    uqsub8      r11, r10, r9                ; q1 - q0
    uqadd8      r6, r8, r6                  ; abs (p0-q0)*2 + abs (p1-q1)/2
    uqsub8      r12, r9, r10                ; q0 - q1
    uqsub8      r6, r6, r4                  ; compare to flimit

    orr         r9, r11, r12                ; abs (q1-q0)
    uqsub8      r8, r9, r2                  ; compare to limit
    uqsub8      r10, r9, r3                 ; compare to thresh
    orr         lr, lr, r6
    orr         lr, lr, r8

    mvn         r11, #0                     ; r11 == -1
    mov         r12, #0

    usub8       lr, r12, lr
    ldr         r9, [sp]                    ; load the compared result
    sel         lr, r11, r12                ; filter mask: lr

    cmp         lr, #0
    beq         mbvskip_filter               ; skip filtering


    ;vp8_hevmask() function
    ;calculate high edge variance

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines

    orr         r9, r9, r10

    ldrh        r7, [src, #-2]
    ldrh        r8, [src], pstep

    usub8       r9, r12, r9
    sel         r6, r12, r11                ; hev mask: r6


    ; vp8_mbfilter() function
    ; p2, q2 are only needed at the end. Don't need to load them in now.
    ; Transpose needs 8 regs(r6 - r12, and lr). Save r6 and lr first
    ; load soure data to r6, r11, r12, lr
    ldrh        r9, [src, #-2]
    ldrh        r10, [src], pstep

    pkhbt       r12, r7, r8, lsl #16

    ldrh        r7, [src, #-2]
    ldrh        r8, [src], pstep

    pkhbt       r11, r9, r10, lsl #16

    ldrh        r9, [src, #-2]
    ldrh        r10, [src], pstep

    str         r6, [sp]                    ; save r6
    str         lr, [sp, #4]                ; save lr

    pkhbt       r6, r7, r8, lsl #16
    pkhbt       lr, r9, r10, lsl #16

    ;transpose r12, r11, r6, lr to p1, p0, q0, q1
    TRANSPOSE_MATRIX r12, r11, r6, lr, r7, r8, r9, r10

    ;load back hev_mask r6 and filter_mask lr
    ldr         r12, c0x80808080
    ldr         r6, [sp]
    ldr         lr, [sp, #4]

    eor         r7, r7, r12                 ; ps1
    eor         r8, r8, r12                 ; ps0
    eor         r9, r9, r12                 ; qs0
    eor         r10, r10, r12               ; qs1

    qsub8       r12, r9, r8                 ; vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    str         r7, [sp, #12]               ; store ps1 temporarily
    qsub8       r7, r7, r10                 ; vp8_signed_char_clamp(ps1-qs1)
    str         r10, [sp, #8]               ; store qs1 temporarily
    qadd8       r7, r7, r12
    str         r9, [sp]                    ; store qs0 temporarily
    qadd8       r7, r7, r12
    str         r8, [sp, #4]                ; store ps0 temporarily
    qadd8       r7, r7, r12                 ; vp8_filter: r7

    ldr         r10, c0x03030303            ; r10 = 3 --modified for vp8
    ldr         r9, c0x04040404
    ;mvn         r11, #0                     ; r11 == -1

    and         r7, r7, lr                  ; vp8_filter &= mask (lr is free)

    mov         r12, r7                     ; Filter2: r12
    and         r12, r12, r6                ; Filter2 &= hev

    ;modify code for vp8
    ;save bottom 3 bits so that we round one side +4 and the other +3
    qadd8       r8 , r12 , r9               ; Filter1 (r8) = vp8_signed_char_clamp(Filter2+4)
    qadd8       r12 , r12 , r10             ; Filter2 (r12) = vp8_signed_char_clamp(Filter2+3)

    mov         r10, #0
    shadd8      r8 , r8 , r10               ; Filter1 >>= 3
    shadd8      r12 , r12 , r10             ; Filter2 >>= 3
    shadd8      r8 , r8 , r10
    shadd8      r12 , r12 , r10
    shadd8      r8 , r8 , r10               ; r8: Filter1
    shadd8      r12 , r12 , r10             ; r12: Filter2

    ldr         r9, [sp]                    ; load qs0
    ldr         r11, [sp, #4]               ; load ps0

    qsub8       r9 , r9, r8                 ; qs0 = vp8_signed_char_clamp(qs0 - Filter1)
    qadd8       r11, r11, r12               ; ps0 = vp8_signed_char_clamp(ps0 + Filter2)

    ;save bottom 3 bits so that we round one side +4 and the other +3
    ;and            r8, r12, r10                ; s = Filter2 & 7 (s: r8)
    ;qadd8      r12 , r12 , r9              ; Filter2 = vp8_signed_char_clamp(Filter2+4)
    ;mov            r10, #0
    ;shadd8     r12 , r12 , r10             ; Filter2 >>= 3
    ;usub8      lr, r8, r9                  ; s = (s==4)*-1
    ;sel            lr, r11, r10
    ;shadd8     r12 , r12 , r10
    ;usub8      r8, r9, r8
    ;sel            r8, r11, r10
    ;ldr            r9, [sp]                    ; load qs0
    ;ldr            r11, [sp, #4]               ; load ps0
    ;shadd8     r12 , r12 , r10
    ;and            r8, r8, lr                  ; -1 for each element that equals 4
    ;qadd8      r10, r8, r12                ; u = vp8_signed_char_clamp(s + Filter2)
    ;qsub8      r9 , r9, r12                ; qs0 = vp8_signed_char_clamp(qs0 - Filter2)
    ;qadd8      r11, r11, r10               ; ps0 = vp8_signed_char_clamp(ps0 + u)

    ;end of modification for vp8

    bic         r12, r7, r6                 ;vp8_filter &= ~hev    ( r6 is free)
    ;mov            r12, r7

    ;roughly 3/7th difference across boundary
    mov         lr, #0x1b                   ; 27
    mov         r7, #0x3f                   ; 63

    sxtb16      r6, r12
    sxtb16      r10, r12, ror #8
    smlabb      r8, r6, lr, r7
    smlatb      r6, r6, lr, r7
    smlabb      r7, r10, lr, r7
    smultb      r10, r10, lr
    ssat        r8, #8, r8, asr #7
    ssat        r6, #8, r6, asr #7
    add         r10, r10, #63
    ssat        r7, #8, r7, asr #7
    ssat        r10, #8, r10, asr #7

    ldr         lr, c0x80808080

    pkhbt       r6, r8, r6, lsl #16
    pkhbt       r10, r7, r10, lsl #16
    uxtb16      r6, r6
    uxtb16      r10, r10

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines

    orr         r10, r6, r10, lsl #8        ; u = vp8_signed_char_clamp((63 + Filter2 * 27)>>7)

    qsub8       r8, r9, r10                 ; s = vp8_signed_char_clamp(qs0 - u)
    qadd8       r10, r11, r10               ; s = vp8_signed_char_clamp(ps0 + u)
    eor         r8, r8, lr                  ; *oq0 = s^0x80
    eor         r10, r10, lr                ; *op0 = s^0x80

    strb        r10, [src, #-1]             ; store op0 result
    strb        r8, [src], pstep            ; store oq0 result
    mov         r10, r10, lsr #8
    mov         r8, r8, lsr #8
    strb        r10, [src, #-1]
    strb        r8, [src], pstep
    mov         r10, r10, lsr #8
    mov         r8, r8, lsr #8
    strb        r10, [src, #-1]
    strb        r8, [src], pstep
    mov         r10, r10, lsr #8
    mov         r8, r8, lsr #8
    strb        r10, [src, #-1]
    strb        r8, [src], pstep

    ;roughly 2/7th difference across boundary
    mov         lr, #0x12                   ; 18
    mov         r7, #0x3f                   ; 63

    sxtb16      r6, r12
    sxtb16      r10, r12, ror #8
    smlabb      r8, r6, lr, r7
    smlatb      r6, r6, lr, r7
    smlabb      r9, r10, lr, r7
    smlatb      r10, r10, lr, r7
    ssat        r8, #8, r8, asr #7
    ssat        r6, #8, r6, asr #7
    ssat        r9, #8, r9, asr #7
    ssat        r10, #8, r10, asr #7

    sub         src, src, pstep, lsl #2     ; move src pointer down by 4 lines

    pkhbt       r6, r8, r6, lsl #16
    pkhbt       r10, r9, r10, lsl #16

    ldr         r9, [sp, #8]                ; load qs1
    ldr         r11, [sp, #12]              ; load ps1
    ldr         lr, c0x80808080

    uxtb16      r6, r6
    uxtb16      r10, r10

    add         src, src, #2

    orr         r10, r6, r10, lsl #8        ; u = vp8_signed_char_clamp((63 + Filter2 * 18)>>7)

    qsub8       r8, r9, r10                 ; s = vp8_signed_char_clamp(qs1 - u)
    qadd8       r10, r11, r10               ; s = vp8_signed_char_clamp(ps1 + u)
    eor         r8, r8, lr                  ; *oq1 = s^0x80
    eor         r10, r10, lr                ; *op1 = s^0x80

    ldrb        r11, [src, #-5]             ; load p2 for 1/7th difference across boundary
    strb        r10, [src, #-4]             ; store op1
    strb        r8, [src, #-1]              ; store oq1
    ldrb        r9, [src], pstep            ; load q2 for 1/7th difference across boundary

    mov         r10, r10, lsr #8
    mov         r8, r8, lsr #8

    ldrb        r6, [src, #-5]
    strb        r10, [src, #-4]
    strb        r8, [src, #-1]
    ldrb        r7, [src], pstep

    mov         r10, r10, lsr #8
    mov         r8, r8, lsr #8
    orr         r11, r11, r6, lsl #8
    orr         r9, r9, r7, lsl #8

    ldrb        r6, [src, #-5]
    strb        r10, [src, #-4]
    strb        r8, [src, #-1]
    ldrb        r7, [src], pstep

    mov         r10, r10, lsr #8
    mov         r8, r8, lsr #8
    orr         r11, r11, r6, lsl #16
    orr         r9, r9, r7, lsl #16

    ldrb        r6, [src, #-5]
    strb        r10, [src, #-4]
    strb        r8, [src, #-1]
    ldrb        r7, [src], pstep
    orr         r11, r11, r6, lsl #24
    orr         r9, r9, r7, lsl #24

    ;roughly 1/7th difference across boundary
    eor         r9, r9, lr
    eor         r11, r11, lr

    mov         lr, #0x9                    ; 9
    mov         r7, #0x3f                   ; 63

    sxtb16      r6, r12
    sxtb16      r10, r12, ror #8
    smlabb      r8, r6, lr, r7
    smlatb      r6, r6, lr, r7
    smlabb      r12, r10, lr, r7
    smlatb      r10, r10, lr, r7
    ssat        r8, #8, r8, asr #7
    ssat        r6, #8, r6, asr #7
    ssat        r12, #8, r12, asr #7
    ssat        r10, #8, r10, asr #7

    sub         src, src, pstep, lsl #2

    pkhbt       r6, r8, r6, lsl #16
    pkhbt       r10, r12, r10, lsl #16

    uxtb16      r6, r6
    uxtb16      r10, r10

    ldr         lr, c0x80808080

    orr         r10, r6, r10, lsl #8        ; u = vp8_signed_char_clamp((63 + Filter2 * 9)>>7)

    qadd8       r8, r11, r10                ; s = vp8_signed_char_clamp(ps2 + u)
    qsub8       r10, r9, r10                ; s = vp8_signed_char_clamp(qs2 - u)
    eor         r8, r8, lr                  ; *op2 = s^0x80
    eor         r10, r10, lr                ; *oq2 = s^0x80

    strb        r8, [src, #-5]              ; store *op2
    strb        r10, [src], pstep           ; store *oq2
    mov         r8, r8, lsr #8
    mov         r10, r10, lsr #8
    strb        r8, [src, #-5]
    strb        r10, [src], pstep
    mov         r8, r8, lsr #8
    mov         r10, r10, lsr #8
    strb        r8, [src, #-5]
    strb        r10, [src], pstep
    mov         r8, r8, lsr #8
    mov         r10, r10, lsr #8
    strb        r8, [src, #-5]
    strb        r10, [src], pstep

    ;adjust src pointer for next loop
    sub         src, src, #2

|mbvskip_filter|
    sub         src, src, #4
    subs        count, count, #1

    ldrne       r6, [src], pstep            ; load source data
    ldrne       r7, [src], pstep
    ldrne       r8, [src], pstep
    ldrne       lr, [src], pstep

    bne         MBVnext8

    add         sp, sp, #16

    ldmia       sp!, {r4 - r11, pc}
    ENDP        ; |vp8_mbloop_filter_vertical_edge_armv6|

; Constant Pool
c0x80808080 DCD     0x80808080
c0x03030303 DCD     0x03030303
c0x04040404 DCD     0x04040404
c0x01010101 DCD     0x01010101
c0x7F7F7F7F DCD     0x7F7F7F7F

    END
