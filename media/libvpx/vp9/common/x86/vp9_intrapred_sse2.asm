;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

%include "third_party/x86inc/x86inc.asm"

SECTION_RODATA
pw_4:  times 8 dw 4
pw_8:  times 8 dw 8
pw_16: times 8 dw 16
pw_32: times 8 dw 32

SECTION .text

INIT_MMX sse
cglobal dc_predictor_4x4, 4, 5, 2, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  movd                  m0, [aboveq]
  punpckldq             m0, [leftq]
  psadbw                m0, m1
  paddw                 m0, [GLOBAL(pw_4)]
  psraw                 m0, 3
  pshufw                m0, m0, 0x0
  packuswb              m0, m0
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  lea                 dstq, [dstq+strideq*2]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0

  RESTORE_GOT
  RET

INIT_MMX sse
cglobal dc_predictor_8x8, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  movq                  m0, [aboveq]
  movq                  m2, [leftq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  psadbw                m0, m1
  psadbw                m2, m1
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw_8)]
  psraw                 m0, 4
  pshufw                m0, m0, 0x0
  packuswb              m0, m0
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0

  RESTORE_GOT
  RET

INIT_XMM sse2
cglobal dc_predictor_16x16, 4, 5, 3, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m2, [leftq]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 4
  psadbw                m0, m1
  psadbw                m2, m1
  paddw                 m0, m2
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw_16)]
  psraw                 m0, 5
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_XMM sse2
cglobal dc_predictor_32x32, 4, 5, 5, dst, stride, above, left, goffset
  GET_GOT     goffsetq

  pxor                  m1, m1
  mova                  m0, [aboveq]
  mova                  m2, [aboveq+16]
  mova                  m3, [leftq]
  mova                  m4, [leftq+16]
  DEFINE_ARGS dst, stride, stride3, lines4
  lea             stride3q, [strideq*3]
  mov              lines4d, 8
  psadbw                m0, m1
  psadbw                m2, m1
  psadbw                m3, m1
  psadbw                m4, m1
  paddw                 m0, m2
  paddw                 m0, m3
  paddw                 m0, m4
  movhlps               m2, m0
  paddw                 m0, m2
  paddw                 m0, [GLOBAL(pw_32)]
  psraw                 m0, 6
  pshuflw               m0, m0, 0x0
  punpcklqdq            m0, m0
  packuswb              m0, m0
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m0
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m0
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m0
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m0
  lea                 dstq, [dstq+strideq*4]
  dec              lines4d
  jnz .loop

  RESTORE_GOT
  REP_RET

INIT_MMX sse
cglobal v_predictor_4x4, 3, 3, 1, dst, stride, above
  movd                  m0, [aboveq]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  lea                 dstq, [dstq+strideq*2]
  movd      [dstq        ], m0
  movd      [dstq+strideq], m0
  RET

INIT_MMX sse
cglobal v_predictor_8x8, 3, 3, 1, dst, stride, above
  movq                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3
  lea             stride3q, [strideq*3]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  movq    [dstq          ], m0
  movq    [dstq+strideq  ], m0
  movq    [dstq+strideq*2], m0
  movq    [dstq+stride3q ], m0
  RET

INIT_XMM sse2
cglobal v_predictor_16x16, 3, 4, 1, dst, stride, above
  mova                  m0, [aboveq]
  DEFINE_ARGS dst, stride, stride3, nlines4
  lea             stride3q, [strideq*3]
  mov              nlines4d, 4
.loop:
  mova    [dstq          ], m0
  mova    [dstq+strideq  ], m0
  mova    [dstq+strideq*2], m0
  mova    [dstq+stride3q ], m0
  lea                 dstq, [dstq+strideq*4]
  dec             nlines4d
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal v_predictor_32x32, 3, 4, 2, dst, stride, above
  mova                  m0, [aboveq]
  mova                  m1, [aboveq+16]
  DEFINE_ARGS dst, stride, stride3, nlines4
  lea             stride3q, [strideq*3]
  mov              nlines4d, 8
.loop:
  mova [dstq             ], m0
  mova [dstq          +16], m1
  mova [dstq+strideq     ], m0
  mova [dstq+strideq  +16], m1
  mova [dstq+strideq*2   ], m0
  mova [dstq+strideq*2+16], m1
  mova [dstq+stride3q    ], m0
  mova [dstq+stride3q +16], m1
  lea                 dstq, [dstq+strideq*4]
  dec             nlines4d
  jnz .loop
  REP_RET

INIT_MMX sse
cglobal tm_predictor_4x4, 4, 4, 4, dst, stride, above, left
  pxor                  m1, m1
  movd                  m2, [aboveq-1]
  movd                  m0, [aboveq]
  punpcklbw             m2, m1
  punpcklbw             m0, m1
  pshufw                m2, m2, 0x0
  DEFINE_ARGS dst, stride, line, left
  mov                lineq, -2
  add                leftq, 4
  psubw                 m0, m2
.loop:
  movd                  m2, [leftq+lineq*2]
  movd                  m3, [leftq+lineq*2+1]
  punpcklbw             m2, m1
  punpcklbw             m3, m1
  pshufw                m2, m2, 0x0
  pshufw                m3, m3, 0x0
  paddw                 m2, m0
  paddw                 m3, m0
  packuswb              m2, m2
  packuswb              m3, m3
  movd      [dstq        ], m2
  movd      [dstq+strideq], m3
  lea                 dstq, [dstq+strideq*2]
  inc                lineq
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal tm_predictor_8x8, 4, 4, 4, dst, stride, above, left
  pxor                  m1, m1
  movd                  m2, [aboveq-1]
  movq                  m0, [aboveq]
  punpcklbw             m2, m1
  punpcklbw             m0, m1
  pshuflw               m2, m2, 0x0
  DEFINE_ARGS dst, stride, line, left
  mov                lineq, -4
  punpcklqdq            m2, m2
  add                leftq, 8
  psubw                 m0, m2
.loop:
  movd                  m2, [leftq+lineq*2]
  movd                  m3, [leftq+lineq*2+1]
  punpcklbw             m2, m1
  punpcklbw             m3, m1
  pshuflw               m2, m2, 0x0
  pshuflw               m3, m3, 0x0
  punpcklqdq            m2, m2
  punpcklqdq            m3, m3
  paddw                 m2, m0
  paddw                 m3, m0
  packuswb              m2, m3
  movq      [dstq        ], m2
  movhps    [dstq+strideq], m2
  lea                 dstq, [dstq+strideq*2]
  inc                lineq
  jnz .loop
  REP_RET

INIT_XMM sse2
cglobal tm_predictor_16x16, 4, 4, 7, dst, stride, above, left
  pxor                  m1, m1
  movd                  m2, [aboveq-1]
  mova                  m0, [aboveq]
  punpcklbw             m2, m1
  punpckhbw             m4, m0, m1
  punpcklbw             m0, m1
  pshuflw               m2, m2, 0x0
  DEFINE_ARGS dst, stride, line, left
  mov                lineq, -8
  punpcklqdq            m2, m2
  add                leftq, 16
  psubw                 m0, m2
  psubw                 m4, m2
.loop:
  movd                  m2, [leftq+lineq*2]
  movd                  m3, [leftq+lineq*2+1]
  punpcklbw             m2, m1
  punpcklbw             m3, m1
  pshuflw               m2, m2, 0x0
  pshuflw               m3, m3, 0x0
  punpcklqdq            m2, m2
  punpcklqdq            m3, m3
  paddw                 m5, m2, m0
  paddw                 m6, m3, m0
  paddw                 m2, m4
  paddw                 m3, m4
  packuswb              m5, m2
  packuswb              m6, m3
  mova      [dstq        ], m5
  mova      [dstq+strideq], m6
  lea                 dstq, [dstq+strideq*2]
  inc                lineq
  jnz .loop
  REP_RET

%if ARCH_X86_64
INIT_XMM sse2
cglobal tm_predictor_32x32, 4, 4, 10, dst, stride, above, left
  pxor                  m1, m1
  movd                  m2, [aboveq-1]
  mova                  m0, [aboveq]
  mova                  m4, [aboveq+16]
  punpcklbw             m2, m1
  punpckhbw             m3, m0, m1
  punpckhbw             m5, m4, m1
  punpcklbw             m0, m1
  punpcklbw             m4, m1
  pshuflw               m2, m2, 0x0
  DEFINE_ARGS dst, stride, line, left
  mov                lineq, -16
  punpcklqdq            m2, m2
  add                leftq, 32
  psubw                 m0, m2
  psubw                 m3, m2
  psubw                 m4, m2
  psubw                 m5, m2
.loop:
  movd                  m2, [leftq+lineq*2]
  movd                  m6, [leftq+lineq*2+1]
  punpcklbw             m2, m1
  punpcklbw             m6, m1
  pshuflw               m2, m2, 0x0
  pshuflw               m6, m6, 0x0
  punpcklqdq            m2, m2
  punpcklqdq            m6, m6
  paddw                 m7, m2, m0
  paddw                 m8, m2, m3
  paddw                 m9, m2, m4
  paddw                 m2, m5
  packuswb              m7, m8
  packuswb              m9, m2
  paddw                 m2, m6, m0
  paddw                 m8, m6, m3
  mova   [dstq           ], m7
  paddw                 m7, m6, m4
  paddw                 m6, m5
  mova   [dstq        +16], m9
  packuswb              m2, m8
  packuswb              m7, m6
  mova   [dstq+strideq   ], m2
  mova   [dstq+strideq+16], m7
  lea                 dstq, [dstq+strideq*2]
  inc                lineq
  jnz .loop
  REP_RET
%endif
