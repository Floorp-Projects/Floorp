;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_intra4x4_predict_armv6|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2


;void vp8_intra4x4_predict(unsigned char *src, int src_stride, int b_mode,
;                          unsigned char *dst, int dst_stride)

|vp8_intra4x4_predict_armv6| PROC
    push        {r4-r12, lr}


    cmp         r2, #10
    addlt       pc, pc, r2, lsl #2       ; position independent switch
    pop         {r4-r12, pc}             ; default
    b           b_dc_pred
    b           b_tm_pred
    b           b_ve_pred
    b           b_he_pred
    b           b_ld_pred
    b           b_rd_pred
    b           b_vr_pred
    b           b_vl_pred
    b           b_hd_pred
    b           b_hu_pred

b_dc_pred
    ; load values
    ldr         r8, [r0, -r1]            ; Above
    ldrb        r4, [r0, #-1]!           ; Left[0]
    mov         r9, #0
    ldrb        r5, [r0, r1]             ; Left[1]
    ldrb        r6, [r0, r1, lsl #1]!    ; Left[2]
    usad8       r12, r8, r9
    ldrb        r7, [r0, r1]             ; Left[3]

    ; calculate dc
    add         r4, r4, r5
    add         r4, r4, r6
    add         r4, r4, r7
    add         r4, r4, r12
    add         r4, r4, #4
    ldr         r0, [sp, #40]           ; load stride
    mov         r12, r4, asr #3         ; (expected_dc + 4) >> 3

    add         r12, r12, r12, lsl #8
    add         r3, r3, r0
    add         r12, r12, r12, lsl #16

    ; store values
    str         r12, [r3, -r0]
    str         r12, [r3]
    str         r12, [r3, r0]
    str         r12, [r3, r0, lsl #1]

    pop        {r4-r12, pc}

b_tm_pred
    sub         r10, r0, #1             ; Left
    ldr         r8, [r0, -r1]           ; Above
    ldrb        r9, [r10, -r1]          ; top_left
    ldrb        r4, [r0, #-1]!          ; Left[0]
    ldrb        r5, [r10, r1]!          ; Left[1]
    ldrb        r6, [r0, r1, lsl #1]    ; Left[2]
    ldrb        r7, [r10, r1, lsl #1]   ; Left[3]
    ldr         r0, [sp, #40]           ; load stride


    add         r9, r9, r9, lsl #16     ; [tl|tl]
    uxtb16      r10, r8                 ; a[2|0]
    uxtb16      r11, r8, ror #8         ; a[3|1]
    ssub16      r10, r10, r9            ; a[2|0] - [tl|tl]
    ssub16      r11, r11, r9            ; a[3|1] - [tl|tl]

    add         r4, r4, r4, lsl #16     ; l[0|0]
    add         r5, r5, r5, lsl #16     ; l[1|1]
    add         r6, r6, r6, lsl #16     ; l[2|2]
    add         r7, r7, r7, lsl #16     ; l[3|3]

    sadd16      r1, r4, r10             ; l[0|0] + a[2|0] - [tl|tl]
    sadd16      r2, r4, r11             ; l[0|0] + a[3|1] - [tl|tl]
    usat16      r1, #8, r1
    usat16      r2, #8, r2

    sadd16      r4, r5, r10             ; l[1|1] + a[2|0] - [tl|tl]
    sadd16      r5, r5, r11             ; l[1|1] + a[3|1] - [tl|tl]

    add         r12, r1, r2, lsl #8     ; [3|2|1|0]
    str         r12, [r3], r0

    usat16      r4, #8, r4
    usat16      r5, #8, r5

    sadd16      r1, r6, r10             ; l[2|2] + a[2|0] - [tl|tl]
    sadd16      r2, r6, r11             ; l[2|2] + a[3|1] - [tl|tl]

    add         r12, r4, r5, lsl #8     ; [3|2|1|0]
    str         r12, [r3], r0

    usat16      r1, #8, r1
    usat16      r2, #8, r2

    sadd16      r4, r7, r10             ; l[3|3] + a[2|0] - [tl|tl]
    sadd16      r5, r7, r11             ; l[3|3] + a[3|1] - [tl|tl]

    add         r12, r1, r2, lsl #8     ; [3|2|1|0]

    usat16      r4, #8, r4
    usat16      r5, #8, r5

    str         r12, [r3], r0

    add         r12, r4, r5, lsl #8     ; [3|2|1|0]
    str         r12, [r3], r0

    pop        {r4-r12, pc}

b_ve_pred
    ldr         r8, [r0, -r1]!          ; a[3|2|1|0]
    ldr         r11, c00FF00FF
    ldrb        r9, [r0, #-1]           ; top_left
    ldrb        r10, [r0, #4]           ; a[4]

    ldr         r0, c00020002

    uxtb16      r4, r8                  ; a[2|0]
    uxtb16      r5, r8, ror #8          ; a[3|1]
    ldr         r2, [sp, #40]           ; stride
    pkhbt       r9, r9, r5, lsl #16     ; a[1|-1]

    add         r9, r9, r4, lsl #1      ;[a[1]+2*a[2]       | tl+2*a[0]       ]
    uxtab16     r9, r9, r5              ;[a[1]+2*a[2]+a[3]  | tl+2*a[0]+a[1]  ]
    uxtab16     r9, r9, r0              ;[a[1]+2*a[2]+a[3]+2| tl+2*a[0]+a[1]+2]

    add         r0, r0, r10, lsl #16    ;[a[4]+2            |                 2]
    add         r0, r0, r4, asr #16     ;[a[4]+2            |            a[2]+2]
    add         r0, r0, r5, lsl #1      ;[a[4]+2*a[3]+2     |     a[2]+2*a[1]+2]
    uadd16      r4, r4, r0              ;[a[4]+2*a[3]+a[2]+2|a[2]+2*a[1]+a[0]+2]

    and         r9, r11, r9, asr #2
    and         r4, r11, r4, asr #2
    add         r3, r3, r2              ; dst + dst_stride
    add         r9, r9, r4, lsl #8

    ; store values
    str         r9, [r3, -r2]
    str         r9, [r3]
    str         r9, [r3, r2]
    str         r9, [r3, r2, lsl #1]

    pop        {r4-r12, pc}


b_he_pred
    sub         r10, r0, #1             ; Left
    ldrb        r4, [r0, #-1]!          ; Left[0]
    ldrb        r8, [r10, -r1]          ; top_left
    ldrb        r5, [r10, r1]!          ; Left[1]
    ldrb        r6, [r0, r1, lsl #1]    ; Left[2]
    ldrb        r7, [r10, r1, lsl #1]   ; Left[3]

    add         r8, r8, r4              ; tl   + l[0]
    add         r9, r4, r5              ; l[0] + l[1]
    add         r10, r5, r6             ; l[1] + l[2]
    add         r11, r6, r7             ; l[2] + l[3]

    mov         r0, #2<<14

    add         r8, r8, r9              ; tl + 2*l[0] + l[1]
    add         r4, r9, r10             ; l[0] + 2*l[1] + l[2]
    add         r5, r10, r11            ; l[1] + 2*l[2] + l[3]
    add         r6, r11, r7, lsl #1     ; l[2] + 2*l[3] + l[3]


    add         r8, r0, r8, lsl #14     ; (tl + 2*l[0] + l[1])>>2 in top half
    add         r9, r0, r4, lsl #14     ; (l[0] + 2*l[1] + l[2])>>2 in top half
    add         r10,r0, r5, lsl #14     ; (l[1] + 2*l[2] + l[3])>>2 in top half
    add         r11,r0, r6, lsl #14     ; (l[2] + 2*l[3] + l[3])>>2 in top half

    pkhtb       r8, r8, r8, asr #16     ; l[-|0|-|0]
    pkhtb       r9, r9, r9, asr #16     ; l[-|1|-|1]
    pkhtb       r10, r10, r10, asr #16  ; l[-|2|-|2]
    pkhtb       r11, r11, r11, asr #16  ; l[-|3|-|3]

    ldr         r0, [sp, #40]           ; stride

    add         r8, r8, r8, lsl #8      ; l[0|0|0|0]
    add         r9, r9, r9, lsl #8      ; l[1|1|1|1]
    add         r10, r10, r10, lsl #8   ; l[2|2|2|2]
    add         r11, r11, r11, lsl #8   ; l[3|3|3|3]

    ; store values
    str         r8, [r3], r0
    str         r9, [r3]
    str         r10, [r3, r0]
    str         r11, [r3, r0, lsl #1]

    pop        {r4-r12, pc}

b_ld_pred
    ldr         r4, [r0, -r1]!          ; Above
    ldr         r12, c00020002
    ldr         r5, [r0, #4]
    ldr         lr,  c00FF00FF

    uxtb16      r6, r4                  ; a[2|0]
    uxtb16      r7, r4, ror #8          ; a[3|1]
    uxtb16      r8, r5                  ; a[6|4]
    uxtb16      r9, r5, ror #8          ; a[7|5]
    pkhtb       r10, r6, r8             ; a[2|4]
    pkhtb       r11, r7, r9             ; a[3|5]


    add         r4, r6, r7, lsl #1      ; [a2+2*a3      |      a0+2*a1]
    add         r4, r4, r10, ror #16    ; [a2+2*a3+a4   |   a0+2*a1+a2]
    uxtab16     r4, r4, r12             ; [a2+2*a3+a4+2 | a0+2*a1+a2+2]

    add         r5, r7, r10, ror #15    ; [a3+2*a4      |      a1+2*a2]
    add         r5, r5, r11, ror #16    ; [a3+2*a4+a5   |   a1+2*a2+a3]
    uxtab16     r5, r5, r12             ; [a3+2*a4+a5+2 | a1+2*a2+a3+2]

    pkhtb       r7, r9, r8, asr #16
    add         r6, r8, r9, lsl #1      ; [a6+2*a7      |      a4+2*a5]
    uadd16      r6, r6, r7              ; [a6+2*a7+a7   |   a4+2*a5+a6]
    uxtab16     r6, r6, r12             ; [a6+2*a7+a7+2 | a4+2*a5+a6+2]

    uxth        r7, r9                  ; [                         a5]
    add         r7, r7, r8, asr #15     ; [                    a5+2*a6]
    add         r7, r7, r9, asr #16     ; [                 a5+2*a6+a7]
    uxtah       r7, r7, r12             ; [               a5+2*a6+a7+2]

    ldr         r0, [sp, #40]           ; stride

    ; scale down
    and         r4, lr, r4, asr #2
    and         r5, lr, r5, asr #2
    and         r6, lr, r6, asr #2
    mov         r7, r7, asr #2

    add         r8, r4, r5, lsl #8      ; [3|2|1|0]
    str         r8, [r3], r0

    mov         r9, r8, lsr #8
    add         r9, r9, r6, lsl #24     ; [4|3|2|1]
    str         r9, [r3], r0

    mov         r10, r9, lsr #8
    add         r10, r10, r7, lsl #24   ; [5|4|3|2]
    str         r10, [r3], r0

    mov         r6, r6, lsr #16
    mov         r11, r10, lsr #8
    add         r11, r11, r6, lsl #24   ; [6|5|4|3]
    str         r11, [r3], r0

    pop        {r4-r12, pc}

b_rd_pred
    sub         r12, r0, r1             ; Above = src - src_stride
    ldrb        r7, [r0, #-1]!          ; l[0] = pp[3]
    ldr         lr, [r12]               ; Above = pp[8|7|6|5]
    ldrb        r8, [r12, #-1]!         ; tl   = pp[4]
    ldrb        r6, [r12, r1, lsl #1]   ; l[1] = pp[2]
    ldrb        r5, [r0, r1, lsl #1]    ; l[2] = pp[1]
    ldrb        r4, [r12, r1, lsl #2]   ; l[3] = pp[0]


    uxtb16      r9, lr                  ; p[7|5]
    uxtb16      r10, lr, ror #8         ; p[8|6]
    add         r4, r4, r6, lsl #16     ; p[2|0]
    add         r5, r5, r7, lsl #16     ; p[3|1]
    add         r6, r6, r8, lsl #16     ; p[4|2]
    pkhbt       r7, r7, r9, lsl #16     ; p[5|3]
    pkhbt       r8, r8, r10, lsl #16    ; p[6|4]

    ldr         r12, c00020002
    ldr         lr,  c00FF00FF

    add         r4, r4, r5, lsl #1      ; [p2+2*p3      |      p0+2*p1]
    add         r4, r4, r6              ; [p2+2*p3+p4   |   p0+2*p1+p2]
    uxtab16     r4, r4, r12             ; [p2+2*p3+p4+2 | p0+2*p1+p2+2]

    add         r5, r5, r6, lsl #1      ; [p3+2*p4      |      p1+2*p2]
    add         r5, r5, r7              ; [p3+2*p4+p5   |   p1+2*p2+p3]
    uxtab16     r5, r5, r12             ; [p3+2*p4+p5+2 | p1+2*p2+p3+2]

    add         r6, r7, r8, lsl #1      ; [p5+2*p6      |      p3+2*p4]
    add         r6, r6, r9              ; [p5+2*p6+p7   |   p3+2*p4+p5]
    uxtab16     r6, r6, r12             ; [p5+2*p6+p7+2 | p3+2*p4+p5+2]

    add         r7, r8, r9, lsl #1      ; [p6+2*p7      |      p4+2*p5]
    add         r7, r7, r10             ; [p6+2*p7+p8   |   p4+2*p5+p6]
    uxtab16     r7, r7, r12             ; [p6+2*p7+p8+2 | p4+2*p5+p6+2]

    ldr         r0, [sp, #40]           ; stride

    ; scale down
    and         r7, lr, r7, asr #2
    and         r6, lr, r6, asr #2
    and         r5, lr, r5, asr #2
    and         r4, lr, r4, asr #2

    add         r8, r6, r7, lsl #8      ; [6|5|4|3]
    str         r8, [r3], r0

    mov         r9, r8, lsl #8          ; [5|4|3|-]
    uxtab       r9, r9, r4, ror #16     ; [5|4|3|2]
    str         r9, [r3], r0

    mov         r10, r9, lsl #8         ; [4|3|2|-]
    uxtab       r10, r10, r5            ; [4|3|2|1]
    str         r10, [r3], r0

    mov         r11, r10, lsl #8        ; [3|2|1|-]
    uxtab       r11, r11, r4            ; [3|2|1|0]
    str         r11, [r3], r0

    pop        {r4-r12, pc}

b_vr_pred
    sub         r12, r0, r1             ; Above = src - src_stride
    ldrb        r7, [r0, #-1]!          ; l[0] = pp[3]
    ldr         lr, [r12]               ; Above = pp[8|7|6|5]
    ldrb        r8, [r12, #-1]!         ; tl   = pp[4]
    ldrb        r6, [r12, r1, lsl #1]   ; l[1] = pp[2]
    ldrb        r5, [r0, r1, lsl #1]    ; l[2] = pp[1]
    ldrb        r4, [r12, r1, lsl #2]   ; l[3] = pp[0]

    add         r5, r5, r7, lsl #16     ; p[3|1]
    add         r6, r6, r8, lsl #16     ; p[4|2]
    uxtb16      r9, lr                  ; p[7|5]
    uxtb16      r10, lr, ror #8         ; p[8|6]
    pkhbt       r7, r7, r9, lsl #16     ; p[5|3]
    pkhbt       r8, r8, r10, lsl #16    ; p[6|4]

    ldr         r4,  c00010001
    ldr         r12, c00020002
    ldr         lr,  c00FF00FF

    add         r5, r5, r6, lsl #1      ; [p3+2*p4      |      p1+2*p2]
    add         r5, r5, r7              ; [p3+2*p4+p5   |   p1+2*p2+p3]
    uxtab16     r5, r5, r12             ; [p3+2*p4+p5+2 | p1+2*p2+p3+2]

    add         r6, r6, r7, lsl #1      ; [p4+2*p5      |      p2+2*p3]
    add         r6, r6, r8              ; [p4+2*p5+p6   |   p2+2*p3+p4]
    uxtab16     r6, r6, r12             ; [p4+2*p5+p6+2 | p2+2*p3+p4+2]

    uadd16      r11, r8, r9             ; [p6+p7        |        p4+p5]
    uhadd16     r11, r11, r4            ; [(p6+p7+1)>>1 | (p4+p5+1)>>1]
                                        ; [F|E]

    add         r7, r7, r8, lsl #1      ; [p5+2*p6      |      p3+2*p4]
    add         r7, r7, r9              ; [p5+2*p6+p7   |   p3+2*p4+p5]
    uxtab16     r7, r7, r12             ; [p5+2*p6+p7+2 | p3+2*p4+p5+2]

    uadd16      r2, r9, r10             ; [p7+p8        |        p5+p6]
    uhadd16     r2, r2, r4              ; [(p7+p8+1)>>1 | (p5+p6+1)>>1]
                                        ; [J|I]

    add         r8, r8, r9, lsl #1      ; [p6+2*p7      |      p4+2*p5]
    add         r8, r8, r10             ; [p6+2*p7+p8   |   p4+2*p5+p6]
    uxtab16     r8, r8, r12             ; [p6+2*p7+p8+2 | p4+2*p5+p6+2]

    ldr         r0, [sp, #40]           ; stride

    ; scale down
    and         r5, lr, r5, asr #2      ; [B|A]
    and         r6, lr, r6, asr #2      ; [D|C]
    and         r7, lr, r7, asr #2      ; [H|G]
    and         r8, lr, r8, asr #2      ; [L|K]

    add         r12, r11, r2, lsl #8    ; [J|F|I|E]
    str         r12, [r3], r0

    add         r12, r7, r8, lsl #8     ; [L|H|K|G]
    str         r12, [r3], r0

    pkhbt       r2, r6, r2, lsl #16     ; [-|I|-|C]
    add         r2, r2, r11, lsl #8     ; [F|I|E|C]

    pkhtb       r12, r6, r5             ; [-|D|-|A]
    pkhtb       r10, r7, r5, asr #16    ; [-|H|-|B]
    str         r2, [r3], r0
    add         r12, r12, r10, lsl #8   ; [H|D|B|A]
    str         r12, [r3], r0

    pop        {r4-r12, pc}

b_vl_pred
    ldr         r4, [r0, -r1]!          ; [3|2|1|0]
    ldr         r12, c00020002
    ldr         r5, [r0, #4]            ; [7|6|5|4]
    ldr         lr,  c00FF00FF
    ldr         r2,  c00010001

    mov         r0, r4, lsr #16         ; [-|-|3|2]
    add         r0, r0, r5, lsl #16     ; [5|4|3|2]
    uxtb16      r6, r4                  ; [2|0]
    uxtb16      r7, r4, ror #8          ; [3|1]
    uxtb16      r8, r0                  ; [4|2]
    uxtb16      r9, r0, ror #8          ; [5|3]
    uxtb16      r10, r5                 ; [6|4]
    uxtb16      r11, r5, ror #8         ; [7|5]

    uadd16      r4, r6, r7              ; [p2+p3        |        p0+p1]
    uhadd16     r4, r4, r2              ; [(p2+p3+1)>>1 | (p0+p1+1)>>1]
                                        ; [B|A]

    add         r5, r6, r7, lsl #1      ; [p2+2*p3      |      p0+2*p1]
    add         r5, r5, r8              ; [p2+2*p3+p4   |   p0+2*p1+p2]
    uxtab16     r5, r5, r12             ; [p2+2*p3+p4+2 | p0+2*p1+p2+2]

    uadd16      r6, r7, r8              ; [p3+p4        |        p1+p2]
    uhadd16     r6, r6, r2              ; [(p3+p4+1)>>1 | (p1+p2+1)>>1]
                                        ; [F|E]

    add         r7, r7, r8, lsl #1      ; [p3+2*p4      |      p1+2*p2]
    add         r7, r7, r9              ; [p3+2*p4+p5   |   p1+2*p2+p3]
    uxtab16     r7, r7, r12             ; [p3+2*p4+p5+2 | p1+2*p2+p3+2]

    add         r8, r8, r9, lsl #1      ; [p4+2*p5      |      p2+2*p3]
    add         r8, r8, r10             ; [p4+2*p5+p6   |   p2+2*p3+p4]
    uxtab16     r8, r8, r12             ; [p4+2*p5+p6+2 | p2+2*p3+p4+2]

    add         r9, r9, r10, lsl #1     ; [p5+2*p6      |      p3+2*p4]
    add         r9, r9, r11             ; [p5+2*p6+p7   |   p3+2*p4+p5]
    uxtab16     r9, r9, r12             ; [p5+2*p6+p7+2 | p3+2*p4+p5+2]

    ldr         r0, [sp, #40]           ; stride

    ; scale down
    and         r5, lr, r5, asr #2      ; [D|C]
    and         r7, lr, r7, asr #2      ; [H|G]
    and         r8, lr, r8, asr #2      ; [I|D]
    and         r9, lr, r9, asr #2      ; [J|H]


    add         r10, r4, r6, lsl #8     ; [F|B|E|A]
    str         r10, [r3], r0

    add         r5, r5, r7, lsl #8      ; [H|C|G|D]
    str         r5, [r3], r0

    pkhtb       r12, r8, r4, asr #16    ; [-|I|-|B]
    pkhtb       r10, r9, r8             ; [-|J|-|D]

    add         r12, r6, r12, lsl #8    ; [I|F|B|E]
    str         r12, [r3], r0

    add         r10, r7, r10, lsl #8    ; [J|H|D|G]
    str         r10, [r3], r0

    pop        {r4-r12, pc}

b_hd_pred
    sub         r12, r0, r1             ; Above = src - src_stride
    ldrb        r7, [r0, #-1]!          ; l[0] = pp[3]
    ldr         lr, [r12]               ; Above = pp[8|7|6|5]
    ldrb        r8, [r12, #-1]!         ; tl   = pp[4]
    ldrb        r6, [r0, r1]            ; l[1] = pp[2]
    ldrb        r5, [r0, r1, lsl #1]    ; l[2] = pp[1]
    ldrb        r4, [r12, r1, lsl #2]   ; l[3] = pp[0]

    uxtb16      r9, lr                  ; p[7|5]
    uxtb16      r10, lr, ror #8         ; p[8|6]

    add         r4, r4, r5, lsl #16     ; p[1|0]
    add         r5, r5, r6, lsl #16     ; p[2|1]
    add         r6, r6, r7, lsl #16     ; p[3|2]
    add         r7, r7, r8, lsl #16     ; p[4|3]

    ldr         r12, c00020002
    ldr         lr,  c00FF00FF
    ldr         r2,  c00010001

    pkhtb       r8, r7, r9              ; p[4|5]
    pkhtb       r1, r9, r10             ; p[7|6]
    pkhbt       r10, r8, r10, lsl #16   ; p[6|5]


    uadd16      r11, r4, r5             ; [p1+p2        |        p0+p1]
    uhadd16     r11, r11, r2            ; [(p1+p2+1)>>1 | (p0+p1+1)>>1]
                                        ; [B|A]

    add         r4, r4, r5, lsl #1      ; [p1+2*p2      |      p0+2*p1]
    add         r4, r4, r6              ; [p1+2*p2+p3   |   p0+2*p1+p2]
    uxtab16     r4, r4, r12             ; [p1+2*p2+p3+2 | p0+2*p1+p2+2]

    uadd16      r0, r6, r7              ; [p3+p4        |        p2+p3]
    uhadd16     r0, r0, r2              ; [(p3+p4+1)>>1 | (p2+p3+1)>>1]
                                        ; [F|E]

    add         r5, r6, r7, lsl #1      ; [p3+2*p4      |      p2+2*p3]
    add         r5, r5, r8, ror #16     ; [p3+2*p4+p5   |   p2+2*p3+p4]
    uxtab16     r5, r5, r12             ; [p3+2*p4+p5+2 | p2+2*p3+p4+2]

    add         r6, r12, r8, ror #16    ; [p5+2         |         p4+2]
    add         r6, r6, r10, lsl #1     ; [p5+2+2*p6    |    p4+2+2*p5]
    uxtab16     r6, r6, r1              ; [p5+2+2*p6+p7 | p4+2+2*p5+p6]

    ; scale down
    and         r4, lr, r4, asr #2      ; [D|C]
    and         r5, lr, r5, asr #2      ; [H|G]
    and         r6, lr, r6, asr #2      ; [J|I]

    ldr         lr, [sp, #40]           ; stride

    pkhtb       r2, r0, r6              ; [-|F|-|I]
    pkhtb       r12, r6, r5, asr #16    ; [-|J|-|H]
    add         r12, r12, r2, lsl #8    ; [F|J|I|H]
    add         r2, r0, r5, lsl #8      ; [H|F|G|E]
    mov         r12, r12, ror #24       ; [J|I|H|F]
    str         r12, [r3], lr


    mov         r7, r11, asr #16        ; [-|-|-|B]
    str         r2, [r3], lr
    add         r7, r7, r0, lsl #16     ; [-|E|-|B]
    add         r7, r7, r4, asr #8      ; [-|E|D|B]
    add         r7, r7, r5, lsl #24     ; [G|E|D|B]
    str         r7, [r3], lr

    add         r5, r11, r4, lsl #8     ; [D|B|C|A]
    str         r5, [r3], lr

    pop        {r4-r12, pc}



b_hu_pred
    ldrb        r4, [r0, #-1]!          ; Left[0]
    ldr         r12, c00020002
    ldrb        r5, [r0, r1]!           ; Left[1]
    ldr         lr,  c00FF00FF
    ldrb        r6, [r0, r1]!           ; Left[2]
    ldr         r2,  c00010001
    ldrb        r7, [r0, r1]            ; Left[3]


    add         r4, r4, r5, lsl #16     ; [1|0]
    add         r5, r5, r6, lsl #16     ; [2|1]
    add         r9, r6, r7, lsl #16     ; [3|2]

    uadd16      r8, r4, r5              ; [p1+p2        |        p0+p1]
    uhadd16     r8, r8, r2              ; [(p1+p2+1)>>1 | (p0+p1+1)>>1]
                                        ; [B|A]

    add         r4, r4, r5, lsl #1      ; [p1+2*p2      |      p0+2*p1]
    add         r4, r4, r9              ; [p1+2*p2+p3   |   p0+2*p1+p2]
    uxtab16     r4, r4, r12             ; [p1+2*p2+p3+2 | p0+2*p1+p2+2]
    ldr         r2, [sp, #40]           ; stride
    and         r4, lr, r4, asr #2      ; [D|C]

    add         r10, r6, r7             ; [p2+p3]
    add         r11, r10, r7, lsl #1    ; [p2+3*p3]
    add         r10, r10, #1
    add         r11, r11, #2
    mov         r10, r10, asr #1        ; [E]
    mov         r11, r11, asr #2        ; [F]

    add         r9, r7, r9, asr #8      ; [-|-|G|G]
    add         r0, r8, r4, lsl #8      ; [D|B|C|A]
    add         r7, r9, r9, lsl #16     ; [G|G|G|G]

    str         r0, [r3], r2

    mov         r1, r8, asr #16         ; [-|-|-|B]
    add         r1, r1, r4, asr #8      ; [-|-|D|B]
    add         r1, r1, r10, lsl #16    ; [-|E|D|B]
    add         r1, r1, r11, lsl #24    ; [F|E|D|B]
    str         r1, [r3], r2

    add         r10, r11, lsl #8        ; [-|-|F|E]
    add         r10, r10, r9, lsl #16   ; [G|G|F|E]
    str         r10, [r3]

    str         r7, [r3, r2]

    pop        {r4-r12, pc}

    ENDP

; constants
c00010001
    DCD         0x00010001
c00020002
    DCD         0x00020002
c00FF00FF
    DCD         0x00FF00FF

    END
