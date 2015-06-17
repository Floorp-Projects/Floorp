;
;  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;
%include "third_party/x86inc/x86inc.asm"

SECTION .text

%macro TRANSFORM_COLS 0
  paddw           m0,        m1
  movq            m4,        m0
  psubw           m3,        m2
  psubw           m4,        m3
  psraw           m4,        1
  movq            m5,        m4
  psubw           m5,        m1 ;b1
  psubw           m4,        m2 ;c1
  psubw           m0,        m4
  paddw           m3,        m5
                                ; m0 a0
  SWAP            1,         4  ; m1 c1
  SWAP            2,         3  ; m2 d1
  SWAP            3,         5  ; m3 b1
%endmacro

%macro TRANSPOSE_4X4 0
  movq            m4,        m0
  movq            m5,        m2
  punpcklwd       m4,        m1
  punpckhwd       m0,        m1
  punpcklwd       m5,        m3
  punpckhwd       m2,        m3
  movq            m1,        m4
  movq            m3,        m0
  punpckldq       m1,        m5
  punpckhdq       m4,        m5
  punpckldq       m3,        m2
  punpckhdq       m0,        m2
  SWAP            2, 3, 0, 1, 4
%endmacro

INIT_MMX mmx
cglobal fwht4x4, 3, 4, 8, input, output, stride
  lea             r3q,       [inputq + strideq*4]
  movq            m0,        [inputq] ;a1
  movq            m1,        [inputq + strideq*2] ;b1
  movq            m2,        [r3q] ;c1
  movq            m3,        [r3q + strideq*2] ;d1

  TRANSFORM_COLS
  TRANSPOSE_4X4
  TRANSFORM_COLS
  TRANSPOSE_4X4

  psllw           m0,        2
  psllw           m1,        2
  psllw           m2,        2
  psllw           m3,        2

%if CONFIG_VP9_HIGHBITDEPTH
  pxor            m4,             m4
  pxor            m5,             m5
  pcmpgtw         m4,             m0
  pcmpgtw         m5,             m1
  movq            m6,             m0
  movq            m7,             m1
  punpcklwd       m0,             m4
  punpcklwd       m1,             m5
  punpckhwd       m6,             m4
  punpckhwd       m7,             m5
  movq            [outputq],      m0
  movq            [outputq + 8],  m6
  movq            [outputq + 16], m1
  movq            [outputq + 24], m7
  pxor            m4,             m4
  pxor            m5,             m5
  pcmpgtw         m4,             m2
  pcmpgtw         m5,             m3
  movq            m6,             m2
  movq            m7,             m3
  punpcklwd       m2,             m4
  punpcklwd       m3,             m5
  punpckhwd       m6,             m4
  punpckhwd       m7,             m5
  movq            [outputq + 32], m2
  movq            [outputq + 40], m6
  movq            [outputq + 48], m3
  movq            [outputq + 56], m7
%else
  movq            [outputq],      m0
  movq            [outputq + 8],  m1
  movq            [outputq + 16], m2
  movq            [outputq + 24], m3
%endif

  RET
