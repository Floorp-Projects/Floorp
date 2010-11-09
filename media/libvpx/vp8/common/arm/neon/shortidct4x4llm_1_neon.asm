;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_short_idct4x4llm_1_neon|
    EXPORT  |vp8_dc_only_idct_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;void vp8_short_idct4x4llm_1_c(short *input, short *output, int pitch);
; r0    short *input;
; r1    short *output;
; r2    int pitch;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
|vp8_short_idct4x4llm_1_neon| PROC
    vld1.16         {d0[]}, [r0]            ;load input[0]

    add             r3, r1, r2
    add             r12, r3, r2

    vrshr.s16       d0, d0, #3

    add             r0, r12, r2

    vst1.16         {d0}, [r1]
    vst1.16         {d0}, [r3]
    vst1.16         {d0}, [r12]
    vst1.16         {d0}, [r0]

    bx             lr
    ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;void vp8_dc_only_idct_c(short input_dc, short *output, int pitch);
; r0    short input_dc;
; r1    short *output;
; r2    int pitch;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
|vp8_dc_only_idct_neon| PROC
    vdup.16         d0, r0

    add             r3, r1, r2
    add             r12, r3, r2

    vrshr.s16       d0, d0, #3

    add             r0, r12, r2

    vst1.16         {d0}, [r1]
    vst1.16         {d0}, [r3]
    vst1.16         {d0}, [r12]
    vst1.16         {d0}, [r0]

    bx             lr

    ENDP
    END
