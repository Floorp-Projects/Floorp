;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

    EXPORT  |aom_convolve_copy_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

|aom_convolve_copy_neon| PROC
    push                {r4-r5, lr}
    ldrd                r4, r5, [sp, #28]

    cmp                 r4, #32
    bgt                 copy64
    beq                 copy32
    cmp                 r4, #8
    bgt                 copy16
    beq                 copy8
    b                   copy4

copy64
    sub                 lr, r1, #32
    sub                 r3, r3, #32
copy64_h
    pld                 [r0, r1, lsl #1]
    vld1.8              {q0-q1}, [r0]!
    vld1.8              {q2-q3}, [r0], lr
    vst1.8              {q0-q1}, [r2@128]!
    vst1.8              {q2-q3}, [r2@128], r3
    subs                r5, r5, #1
    bgt                 copy64_h
    pop                 {r4-r5, pc}

copy32
    pld                 [r0, r1, lsl #1]
    vld1.8              {q0-q1}, [r0], r1
    pld                 [r0, r1, lsl #1]
    vld1.8              {q2-q3}, [r0], r1
    vst1.8              {q0-q1}, [r2@128], r3
    vst1.8              {q2-q3}, [r2@128], r3
    subs                r5, r5, #2
    bgt                 copy32
    pop                 {r4-r5, pc}

copy16
    pld                 [r0, r1, lsl #1]
    vld1.8              {q0}, [r0], r1
    pld                 [r0, r1, lsl #1]
    vld1.8              {q1}, [r0], r1
    vst1.8              {q0}, [r2@128], r3
    vst1.8              {q1}, [r2@128], r3
    subs                r5, r5, #2
    bgt                 copy16
    pop                 {r4-r5, pc}

copy8
    pld                 [r0, r1, lsl #1]
    vld1.8              {d0}, [r0], r1
    pld                 [r0, r1, lsl #1]
    vld1.8              {d2}, [r0], r1
    vst1.8              {d0}, [r2@64], r3
    vst1.8              {d2}, [r2@64], r3
    subs                r5, r5, #2
    bgt                 copy8
    pop                 {r4-r5, pc}

copy4
    ldr                 r12, [r0], r1
    str                 r12, [r2], r3
    subs                r5, r5, #1
    bgt                 copy4
    pop                 {r4-r5, pc}
    ENDP

    END
