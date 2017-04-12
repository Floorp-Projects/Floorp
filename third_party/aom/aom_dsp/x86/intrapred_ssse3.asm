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

%include "third_party/x86inc/x86inc.asm"

SECTION_RODATA

pb_1: times 16 db 1
sh_b12345677: db 1, 2, 3, 4, 5, 6, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0
sh_b23456777: db 2, 3, 4, 5, 6, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0
sh_b0123456777777777: db 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7
sh_b1234567777777777: db 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
sh_b2345677777777777: db 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
sh_b123456789abcdeff: db 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15
sh_b23456789abcdefff: db 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15
sh_b32104567: db 3, 2, 1, 0, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0
sh_b8091a2b345: db 8, 0, 9, 1, 10, 2, 11, 3, 4, 5, 0, 0, 0, 0, 0, 0
sh_b76543210: db 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
sh_b65432108: db 6, 5, 4, 3, 2, 1, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0
sh_b54321089: db 5, 4, 3, 2, 1, 0, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0
sh_b89abcdef: db 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0
sh_bfedcba9876543210: db 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

SECTION .text

; ------------------------------------------
; input: x, y, z, result
;
; trick from pascal
; (x+2y+z+2)>>2 can be calculated as:
; result = avg(x,z)
; result -= xor(x,z) & 1
; result = avg(result,y)
; ------------------------------------------
%macro X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 4
  pavgb               %4, %1, %3
  pxor                %3, %1
  pand                %3, [GLOBAL(pb_1)]
  psubb               %4, %3
  pavgb               %4, %2
%endmacro

INIT_XMM ssse3
cglobal d63e_predictor_4x4, 3, 4, 5, dst, stride, above, goffset
  GET_GOT     goffsetq

  movq                m3, [aboveq]
  pshufb              m1, m3, [GLOBAL(sh_b23456777)]
  pshufb              m2, m3, [GLOBAL(sh_b12345677)]

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m3, m2, m1, m4
  pavgb               m3, m2

  ; store 4 lines
  movd    [dstq        ], m3
  movd    [dstq+strideq], m4
  lea               dstq, [dstq+strideq*2]
  psrldq              m3, 1
  psrldq              m4, 1
  movd    [dstq        ], m3
  movd    [dstq+strideq], m4
  RESTORE_GOT
  RET

INIT_XMM ssse3
cglobal d153_predictor_4x4, 4, 5, 4, dst, stride, above, left, goffset
  GET_GOT     goffsetq
  movd                m0, [leftq]               ; l1, l2, l3, l4
  movd                m1, [aboveq-1]            ; tl, t1, t2, t3
  punpckldq           m0, m1                    ; l1, l2, l3, l4, tl, t1, t2, t3
  pshufb              m0, [GLOBAL(sh_b32104567)]; l4, l3, l2, l1, tl, t1, t2, t3
  psrldq              m1, m0, 1                 ; l3, l2, l1, tl, t1, t2, t3
  psrldq              m2, m0, 2                 ; l2, l1, tl, t1, t2, t3
  ; comments below are for a predictor like this
  ; A1 B1 C1 D1
  ; A2 B2 A1 B1
  ; A3 B3 A2 B2
  ; A4 B4 A3 B3
  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m0, m1, m2, m3  ; 3-tap avg B4 B3 B2 B1 C1 D1
  pavgb               m1, m0                    ; 2-tap avg A4 A3 A2 A1

  punpcklqdq          m3, m1                    ; B4 B3 B2 B1 C1 D1 x x A4 A3 A2 A1 ..

  DEFINE_ARGS dst, stride, stride3
  lea           stride3q, [strideq*3]
  pshufb              m3, [GLOBAL(sh_b8091a2b345)] ; A4 B4 A3 B3 A2 B2 A1 B1 C1 D1 ..
  movd  [dstq+stride3q ], m3
  psrldq              m3, 2                     ; A3 B3 A2 B2 A1 B1 C1 D1 ..
  movd  [dstq+strideq*2], m3
  psrldq              m3, 2                     ; A2 B2 A1 B1 C1 D1 ..
  movd  [dstq+strideq  ], m3
  psrldq              m3, 2                     ; A1 B1 C1 D1 ..
  movd  [dstq          ], m3
  RESTORE_GOT
  RET

INIT_XMM ssse3
cglobal d153_predictor_8x8, 4, 5, 8, dst, stride, above, left, goffset
  GET_GOT     goffsetq
  movq                m0, [leftq]                     ; [0- 7] l1-8 [byte]
  movhps              m0, [aboveq-1]                  ; [8-15] tl, t1-7 [byte]
  pshufb              m1, m0, [GLOBAL(sh_b76543210)]  ; l8-1 [word]
  pshufb              m2, m0, [GLOBAL(sh_b65432108)]  ; l7-1,tl [word]
  pshufb              m3, m0, [GLOBAL(sh_b54321089)]  ; l6-1,tl,t1 [word]
  pshufb              m0, [GLOBAL(sh_b89abcdef)]      ; tl,t1-7 [word]
  psrldq              m4, m0, 1                       ; t1-7 [word]
  psrldq              m5, m0, 2                       ; t2-7 [word]
  ; comments below are for a predictor like this
  ; A1 B1 C1 D1 E1 F1 G1 H1
  ; A2 B2 A1 B1 C1 D1 E1 F1
  ; A3 B3 A2 B2 A1 B1 C1 D1
  ; A4 B4 A3 B3 A2 B2 A1 B1
  ; A5 B5 A4 B4 A3 B3 A2 B2
  ; A6 B6 A5 B5 A4 B4 A3 B3
  ; A7 B7 A6 B6 A5 B5 A4 B4
  ; A8 B8 A7 B7 A6 B6 A5 B5
  pavgb               m6, m1, m2                ; 2-tap avg A8-A1

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m0, m4, m5, m7  ; 3-tap avg C-H1

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m1, m2, m3, m0  ; 3-tap avg B8-1

  punpcklbw           m6, m0                    ; A-B8, A-B7 ... A-B2, A-B1

  DEFINE_ARGS dst, stride, stride3
  lea           stride3q, [strideq*3]

  movhps [dstq+stride3q], m6                    ; A-B4, A-B3, A-B2, A-B1
  palignr             m0, m7, m6, 10            ; A-B3, A-B2, A-B1, C-H1
  movq  [dstq+strideq*2], m0
  psrldq              m0, 2                     ; A-B2, A-B1, C-H1
  movq  [dstq+strideq  ], m0
  psrldq              m0, 2                     ; A-H1
  movq  [dstq          ], m0
  lea               dstq, [dstq+strideq*4]
  movq  [dstq+stride3q ], m6                    ; A-B8, A-B7, A-B6, A-B5
  psrldq              m6, 2                     ; A-B7, A-B6, A-B5, A-B4
  movq  [dstq+strideq*2], m6
  psrldq              m6, 2                     ; A-B6, A-B5, A-B4, A-B3
  movq  [dstq+strideq  ], m6
  psrldq              m6, 2                     ; A-B5, A-B4, A-B3, A-B2
  movq  [dstq          ], m6
  RESTORE_GOT
  RET

INIT_XMM ssse3
cglobal d153_predictor_16x16, 4, 5, 8, dst, stride, above, left, goffset
  GET_GOT     goffsetq
  mova                m0, [leftq]
  movu                m7, [aboveq-1]
  ; comments below are for a predictor like this
  ; A1 B1 C1 D1 E1 F1 G1 H1 I1 J1 K1 L1 M1 N1 O1 P1
  ; A2 B2 A1 B1 C1 D1 E1 F1 G1 H1 I1 J1 K1 L1 M1 N1
  ; A3 B3 A2 B2 A1 B1 C1 D1 E1 F1 G1 H1 I1 J1 K1 L1
  ; A4 B4 A3 B3 A2 B2 A1 B1 C1 D1 E1 F1 G1 H1 I1 J1
  ; A5 B5 A4 B4 A3 B3 A2 B2 A1 B1 C1 D1 E1 F1 G1 H1
  ; A6 B6 A5 B5 A4 B4 A3 B3 A2 B2 A1 B1 C1 D1 E1 F1
  ; A7 B7 A6 B6 A5 B5 A4 B4 A3 B3 A2 B2 A1 B1 C1 D1
  ; A8 B8 A7 B7 A6 B6 A5 B5 A4 B4 A3 B3 A2 B2 A1 B1
  ; A9 B9 A8 B8 A7 B7 A6 B6 A5 B5 A4 B4 A3 B3 A2 B2
  ; Aa Ba A9 B9 A8 B8 A7 B7 A6 B6 A5 B5 A4 B4 A3 B3
  ; Ab Bb Aa Ba A9 B9 A8 B8 A7 B7 A6 B6 A5 B5 A4 B4
  ; Ac Bc Ab Bb Aa Ba A9 B9 A8 B8 A7 B7 A6 B6 A5 B5
  ; Ad Bd Ac Bc Ab Bb Aa Ba A9 B9 A8 B8 A7 B7 A6 B6
  ; Ae Be Ad Bd Ac Bc Ab Bb Aa Ba A9 B9 A8 B8 A7 B7
  ; Af Bf Ae Be Ad Bd Ac Bc Ab Bb Aa Ba A9 B9 A8 B8
  ; Ag Bg Af Bf Ae Be Ad Bd Ac Bc Ab Bb Aa Ba A9 B9
  pshufb              m6, m7, [GLOBAL(sh_bfedcba9876543210)]
  palignr             m5, m0, m6, 15
  palignr             m3, m0, m6, 14

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m0, m5, m3, m4          ; 3-tap avg B3-Bg
  pshufb              m1, m0, [GLOBAL(sh_b123456789abcdeff)]
  pavgb               m5, m0                            ; A1 - Ag

  punpcklbw           m0, m4, m5                        ; A-B8 ... A-B1
  punpckhbw           m4, m5                            ; A-B9 ... A-Bg

  pshufb              m3, m7, [GLOBAL(sh_b123456789abcdeff)]
  pshufb              m5, m7, [GLOBAL(sh_b23456789abcdefff)]

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m7, m3, m5, m1          ; 3-tap avg C1-P1

  pshufb              m6, m0, [GLOBAL(sh_bfedcba9876543210)]
  DEFINE_ARGS dst, stride, stride3
  lea           stride3q, [strideq*3]
  palignr             m2, m1, m6, 14
  mova  [dstq          ], m2
  palignr             m2, m1, m6, 12
  mova  [dstq+strideq  ], m2
  palignr             m2, m1, m6, 10
  mova  [dstq+strideq*2], m2
  palignr             m2, m1, m6, 8
  mova  [dstq+stride3q ], m2
  lea               dstq, [dstq+strideq*4]
  palignr             m2, m1, m6, 6
  mova  [dstq          ], m2
  palignr             m2, m1, m6, 4
  mova  [dstq+strideq  ], m2
  palignr             m2, m1, m6, 2
  mova  [dstq+strideq*2], m2
  pshufb              m4, [GLOBAL(sh_bfedcba9876543210)]
  mova  [dstq+stride3q ], m6
  lea               dstq, [dstq+strideq*4]

  palignr             m2, m6, m4, 14
  mova  [dstq          ], m2
  palignr             m2, m6, m4, 12
  mova  [dstq+strideq  ], m2
  palignr             m2, m6, m4, 10
  mova  [dstq+strideq*2], m2
  palignr             m2, m6, m4, 8
  mova  [dstq+stride3q ], m2
  lea               dstq, [dstq+strideq*4]
  palignr             m2, m6, m4, 6
  mova  [dstq          ], m2
  palignr             m2, m6, m4, 4
  mova  [dstq+strideq  ], m2
  palignr             m2, m6, m4, 2
  mova  [dstq+strideq*2], m2
  mova  [dstq+stride3q ], m4
  RESTORE_GOT
  RET

INIT_XMM ssse3
cglobal d153_predictor_32x32, 4, 5, 8, dst, stride, above, left, goffset
  GET_GOT     goffsetq
  mova                  m0, [leftq]
  movu                  m7, [aboveq-1]
  movu                  m1, [aboveq+15]

  pshufb                m4, m1, [GLOBAL(sh_b123456789abcdeff)]
  pshufb                m6, m1, [GLOBAL(sh_b23456789abcdefff)]

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m1, m4, m6, m2          ; 3-tap avg above [high]

  palignr               m3, m1, m7, 1
  palignr               m5, m1, m7, 2

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m7, m3, m5, m1          ; 3-tap avg above [low]

  pshufb                m7, [GLOBAL(sh_bfedcba9876543210)]
  palignr               m5, m0, m7, 15
  palignr               m3, m0, m7, 14

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m0, m5, m3, m4          ; 3-tap avg B3-Bg
  pavgb                 m5, m0                            ; A1 - Ag
  punpcklbw             m6, m4, m5                        ; A-B8 ... A-B1
  punpckhbw             m4, m5                            ; A-B9 ... A-Bg
  pshufb                m6, [GLOBAL(sh_bfedcba9876543210)]
  pshufb                m4, [GLOBAL(sh_bfedcba9876543210)]

  DEFINE_ARGS dst, stride, stride3, left, line
  lea             stride3q, [strideq*3]

  palignr               m5, m2, m1, 14
  palignr               m7, m1, m6, 14
  mova  [dstq            ], m7
  mova  [dstq+16         ], m5
  palignr               m5, m2, m1, 12
  palignr               m7, m1, m6, 12
  mova  [dstq+strideq    ], m7
  mova  [dstq+strideq+16 ], m5
  palignr                m5, m2, m1, 10
  palignr                m7, m1, m6, 10
  mova  [dstq+strideq*2   ], m7
  mova  [dstq+strideq*2+16], m5
  palignr                m5, m2, m1, 8
  palignr                m7, m1, m6, 8
  mova  [dstq+stride3q    ], m7
  mova  [dstq+stride3q+16 ], m5
  lea                  dstq, [dstq+strideq*4]
  palignr                m5, m2, m1, 6
  palignr                m7, m1, m6, 6
  mova  [dstq             ], m7
  mova  [dstq+16          ], m5
  palignr                m5, m2, m1, 4
  palignr                m7, m1, m6, 4
  mova  [dstq+strideq     ], m7
  mova  [dstq+strideq+16  ], m5
  palignr                m5, m2, m1, 2
  palignr                m7, m1, m6, 2
  mova  [dstq+strideq*2   ], m7
  mova  [dstq+strideq*2+16], m5
  mova  [dstq+stride3q    ], m6
  mova  [dstq+stride3q+16 ], m1
  lea                  dstq, [dstq+strideq*4]

  palignr                m5, m1, m6, 14
  palignr                m3, m6, m4, 14
  mova  [dstq             ], m3
  mova  [dstq+16          ], m5
  palignr                m5, m1, m6, 12
  palignr                m3, m6, m4, 12
  mova  [dstq+strideq     ], m3
  mova  [dstq+strideq+16  ], m5
  palignr                m5, m1, m6, 10
  palignr                m3, m6, m4, 10
  mova  [dstq+strideq*2   ], m3
  mova  [dstq+strideq*2+16], m5
  palignr                m5, m1, m6, 8
  palignr                m3, m6, m4, 8
  mova  [dstq+stride3q    ], m3
  mova  [dstq+stride3q+16 ], m5
  lea                  dstq, [dstq+strideq*4]
  palignr                m5, m1, m6, 6
  palignr                m3, m6, m4, 6
  mova  [dstq             ], m3
  mova  [dstq+16          ], m5
  palignr                m5, m1, m6, 4
  palignr                m3, m6, m4, 4
  mova  [dstq+strideq     ], m3
  mova  [dstq+strideq+16  ], m5
  palignr                m5, m1, m6, 2
  palignr                m3, m6, m4, 2
  mova  [dstq+strideq*2   ], m3
  mova  [dstq+strideq*2+16], m5
  mova  [dstq+stride3q    ], m4
  mova  [dstq+stride3q+16 ], m6
  lea               dstq, [dstq+strideq*4]

  mova                   m7, [leftq]
  mova                   m3, [leftq+16]
  palignr                m5, m3, m7, 15
  palignr                m0, m3, m7, 14

  X_PLUS_2Y_PLUS_Z_PLUS_2_RSH_2 m3, m5, m0, m2          ; 3-tap avg Bh -
  pavgb                  m5, m3                            ; Ah -
  punpcklbw              m3, m2, m5                        ; A-B8 ... A-B1
  punpckhbw              m2, m5                            ; A-B9 ... A-Bg
  pshufb                 m3, [GLOBAL(sh_bfedcba9876543210)]
  pshufb                 m2, [GLOBAL(sh_bfedcba9876543210)]

  palignr                m7, m6, m4, 14
  palignr                m0, m4, m3, 14
  mova  [dstq             ], m0
  mova  [dstq+16          ], m7
  palignr                m7, m6, m4, 12
  palignr                m0, m4, m3, 12
  mova  [dstq+strideq     ], m0
  mova  [dstq+strideq+16  ], m7
  palignr                m7, m6, m4, 10
  palignr                m0, m4, m3, 10
  mova  [dstq+strideq*2   ], m0
  mova  [dstq+strideq*2+16], m7
  palignr                m7, m6, m4, 8
  palignr                m0, m4, m3, 8
  mova  [dstq+stride3q    ], m0
  mova  [dstq+stride3q+16 ], m7
  lea                  dstq, [dstq+strideq*4]
  palignr                m7, m6, m4, 6
  palignr                m0, m4, m3, 6
  mova  [dstq             ], m0
  mova  [dstq+16          ], m7
  palignr                m7, m6, m4, 4
  palignr                m0, m4, m3, 4
  mova  [dstq+strideq     ], m0
  mova  [dstq+strideq+16  ], m7
  palignr                m7, m6, m4, 2
  palignr                m0, m4, m3, 2
  mova  [dstq+strideq*2   ], m0
  mova  [dstq+strideq*2+16], m7
  mova  [dstq+stride3q    ], m3
  mova  [dstq+stride3q+16 ], m4
  lea                  dstq, [dstq+strideq*4]

  palignr                m7, m4, m3, 14
  palignr                m0, m3, m2, 14
  mova  [dstq             ], m0
  mova  [dstq+16          ], m7
  palignr                m7, m4, m3, 12
  palignr                m0, m3, m2, 12
  mova  [dstq+strideq     ], m0
  mova  [dstq+strideq+16  ], m7
  palignr                m7, m4, m3, 10
  palignr                m0, m3, m2, 10
  mova  [dstq+strideq*2   ], m0
  mova  [dstq+strideq*2+16], m7
  palignr                m7, m4, m3, 8
  palignr                m0, m3, m2, 8
  mova  [dstq+stride3q    ], m0
  mova  [dstq+stride3q+16 ], m7
  lea                  dstq, [dstq+strideq*4]
  palignr                m7, m4, m3, 6
  palignr                m0, m3, m2, 6
  mova  [dstq             ], m0
  mova  [dstq+16          ], m7
  palignr                m7, m4, m3, 4
  palignr                m0, m3, m2, 4
  mova  [dstq+strideq     ], m0
  mova  [dstq+strideq+16  ], m7
  palignr                m7, m4, m3, 2
  palignr                m0, m3, m2, 2
  mova  [dstq+strideq*2   ], m0
  mova  [dstq+strideq*2+16], m7
  mova  [dstq+stride3q    ], m2
  mova  [dstq+stride3q+16 ], m3

  RESTORE_GOT
  RET
