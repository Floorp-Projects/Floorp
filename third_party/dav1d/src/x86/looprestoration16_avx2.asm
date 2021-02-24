; Copyright (c) 2017-2021, The rav1e contributors
; Copyright (c) 2021, Nathan Egge
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
; ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "config.asm"
%include "ext/x86/x86inc.asm"

%if ARCH_X86_64

SECTION_RODATA 32

wiener5_shufB:  db  0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9
wiener5_shufC:  db  8,  9,  6,  7, 10, 11,  8,  9, 12, 13, 10, 11, 14, 15, 12, 13
wiener5_shufD:  db  4,  5, -1, -1,  6,  7, -1, -1,  8,  9, -1, -1, 10, 11, -1, -1
wiener5_l_shuf: db  4,  5,  4,  5,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
pb_0to31:       db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
                db 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31

wiener7_shufC:  db  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11,  8,  9
wiener7_shufD:  db  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13
wiener7_shufE:  db  8,  9, -1, -1, 10, 11, -1, -1, 12, 13, -1, -1, 14, 15, -1, -1
rev_w:          db 14, 15, 12, 13, 10, 11,  8,  9,  6,  7,  4,  5,  2,  3,  0,  1
rev_d:          db 12, 13, 14, 15,  8,  9, 10, 11,  4,  5,  6,  7,  0,  1,  2,  3
wiener7_l_shuf: db  6,  7,  6,  7,  6,  7,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
                db 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31

pq_3:      dq (6 - 4) + 1
pq_5:      dq (6 - 2) + 1
pd_65540:  dd (1 << (8 + (6 - 4) + 6)) + (1 << (6 - 4))
pd_262160: dd (1 << (8 + (6 - 2) + 6)) + (1 << (6 - 2))

pq_11:      dq 12 - (6 - 4) + 1
pq_9:       dq 12 - (6 - 2) + 1
nd_1047552: dd (1 << (12 - (6 - 4))) - (1 << (12 + 8))
nd_1048320: dd (1 << (12 - (6 - 2))) - (1 << (12 + 8))

pb_wiener5_l: times 2 db  2,  3
pb_wiener5_r: times 2 db -6, -5

pb_wiener7_l: times 2 db  4,  5
pb_wiener7_m: times 2 db -4, -3
pb_wiener7_r: times 2 db -8, -7

SECTION .text

INIT_YMM avx2
cglobal wiener_filter5_h_16bpc, 6, 9, 14, dst, left, src, ss, f, w, h, edge, bdmax
  movifnidn      wd, wm
  movifnidn      hd, hm
  movifnidn   edgeb, edgem
  vbroadcasti128 m6, [wiener5_shufB]
  vpbroadcastd  m12, [fq + 2]
  vbroadcasti128 m7, [wiener5_shufC]
  vpbroadcastw  m13, [fq + 6]
  vbroadcasti128 m8, [wiener5_shufD]
  popcnt     bdmaxd, bdmaxm
  vpbroadcastd   m9, [pd_65540]
  movq         xm10, [pq_3]
  cmp        bdmaxd, 10
  je .bits10
  vpbroadcastd   m9, [pd_262160]
  movq         xm10, [pq_5]
.bits10:
  pxor          m11, m11
  add            wq, wq
  add          srcq, wq
  add          dstq, wq
  neg            wq
  DEFINE_ARGS dst, left, src, ss, f, w, h, edge, x
.v_loop:
  mov            xq, wq
  test        edgeb, 1 ; LR_HAVE_LEFT
  jz .h_extend_left
  test        leftq, leftq
  jz .h_loop
  movd          xm4, [leftq + 4]
  vpblendd       m4, [srcq + xq - 4], 0xfe
  add         leftq, 8
  jmp .h_main
.h_extend_left:
  vbroadcasti128 m5, [srcq + xq]
  mova           m4, [srcq + xq]
  palignr        m4, m5, 12
  pshufb         m4, [wiener5_l_shuf]
  jmp .h_main
.h_loop:
  movu           m4, [srcq + xq - 4]
.h_main:
  movu           m5, [srcq + xq + 4]
  test        edgeb, 2 ; LR_HAVE_RIGHT
  jnz .h_have_right
  cmp            xd, -36
  jl .h_have_right
  movd          xm2, xd
  vpbroadcastd   m0, [pb_wiener5_l]
  vpbroadcastd   m1, [pb_wiener5_r]
  vpbroadcastb   m2, xm2
  movu           m3, [pb_0to31]
  psubb          m0, m2
  psubb          m1, m2
  pminub         m0, m3
  pminub         m1, m3
  pshufb         m4, m0
  pshufb         m5, m1
.h_have_right:
  pshufb         m0, m4, m6
  pshufb         m2, m4, m7
  paddw          m0, m2
  pmaddwd        m0, m12
  pshufb         m1, m5, m6
  pshufb         m3, m5, m7
  paddw          m1, m3
  pmaddwd        m1, m12
  pshufb         m4, m8
  pmaddwd        m4, m13
  pshufb         m5, m8
  pmaddwd        m5, m13
  paddd          m0, m4
  paddd          m1, m5
  paddd          m0, m9
  paddd          m1, m9
  psrad          m0, xm10
  psrad          m1, xm10
  packssdw       m0, m1
  pmaxsw         m0, m11
  mova  [dstq + xq], m0
  add            xq, 32
  jl .h_loop
  add          srcq, ssq
  add          dstq, 384*2
  dec            hd
  jg .v_loop
  RET

DECLARE_REG_TMP 8, 9, 10, 11, 12, 13, 14

INIT_YMM avx2
cglobal wiener_filter5_v_16bpc, 6, 13, 12, dst, ds, mid, f, w, h, edge, bdmax
  movifnidn    wd, wm
  movifnidn    hd, hm
  movifnidn edgeb, edgem
  pxor         m6, m6
  vpbroadcastd m7, [fq + 2]
  vpbroadcastd m8, [fq + 6]
  popcnt   bdmaxd, bdmaxm
  vpbroadcastd m9, [nd_1047552]
  movq       xm10, [pq_11]
  cmp      bdmaxd, 10
  je .bits10
  vpbroadcastd m9, [nd_1048320]
  movq       xm10, [pq_9]
.bits10:
  vpbroadcastw m11, bdmaxm
  add          wq, wq
  add        midq, wq
  add        dstq, wq
  neg          wq
  DEFINE_ARGS dst, ds, mid, ms, w, h, edge, x
  mov         msq, 2*384
  mov          t0, midq
  lea          t1, [t0 + msq]
  lea          t2, [t1 + msq]
  lea          t3, [t2 + msq]
  lea          t4, [t3 + msq]
  test      edgeb, 4 ; LR_HAVE_TOP
  jnz .have_top
  mov          t0, t2
  mov          t1, t2
.have_top:
  test      edgeb, 8 ; LR_HAVE_BOTTOM
  jnz .v_loop
  cmp          hd, 2
  jg .v_loop
  cmp          hd, 1
  jne .limit_v
  mov          t3, t2
.limit_v:
  mov          t4, t3
.v_loop:
  mov          xq, wq
.h_loop:
  mova         m1, [t0 + xq]
  mova         m2, [t1 + xq]
  mova         m3, [t2 + xq]
  mova         m4, [t3 + xq]
  mova         m5, [t4 + xq]
  punpcklwd    m0, m1, m2
  pmaddwd      m0, m7
  punpckhwd    m1, m2
  pmaddwd      m1, m7
  punpcklwd    m2, m5, m4
  pmaddwd      m2, m7
  punpckhwd    m5, m4
  pmaddwd      m5, m7
  paddd        m0, m2
  paddd        m1, m5
  punpcklwd    m2, m3, m6
  pmaddwd      m2, m8
  punpckhwd    m3, m6
  pmaddwd      m3, m8
  paddd        m0, m2
  paddd        m1, m3
  paddd        m0, m9
  paddd        m1, m9
  psrad        m0, xm10
  psrad        m1, xm10
  packusdw     m0, m1
  pminuw       m0, m11
  mova [dstq + xq], m0
  add          xq, 32
  jl .h_loop
  add        dstq, dsq
  mov          t0, t1
  mov          t1, t2
  mov          t2, t3
  mov          t3, t4
  add          t4, msq
  test      edgeb, 8 ; LR_HAVE_BOTTOM
  jnz .have_bottom
  cmp          hd, 3
  jg .have_bottom
  mov          t4, t3
.have_bottom:
  dec          hd
  jg .v_loop
  RET

INIT_YMM avx2
cglobal wiener_filter7_h_16bpc, 6, 10, 16, dst, left, src, ss, f, w, h, edge, bdmax, rh
  movifnidn       wd, wm
  movifnidn       hd, hm
  movifnidn    edgeb, edgem
  vpbroadcastd    m7, [fq]
  vpbroadcastd    m8, [fq + 4]
  vbroadcasti128 m10, [rev_w]
  vbroadcasti128 m11, [wiener5_shufB]
  vbroadcasti128 m12, [wiener7_shufC]
  vbroadcasti128 m13, [wiener7_shufD]
  vbroadcasti128 m14, [wiener7_shufE]
  vbroadcasti128 m15, [rev_d]
  popcnt      bdmaxd, bdmaxm
  vpbroadcastd    m9, [pd_65540]
  mov            rhq, [pq_3]
  cmp         bdmaxd, 10
  je .bits10
  vpbroadcastd    m9, [pd_262160]
  mov            rhq, [pq_5]
.bits10:
  add             wq, wq
  add           srcq, wq
  add           dstq, wq
  neg             wq
  DEFINE_ARGS dst, left, src, ss, f, w, h, edge, x, rh
.v_loop:
  mov             xq, wq
  test         edgeb, 1 ; LR_HAVE_LEFT
  jz .h_extend_left
  test         leftq, leftq
  jz .h_loop
  movq           xm4, [leftq + 2]
  vpblendw       xm4, [srcq + xq - 6], 0xf8
  vinserti128     m4, [srcq + xq + 10], 1
  add          leftq, 8
  jmp .h_main
.h_extend_left:
  vbroadcasti128  m5, [srcq + xq]
  mova            m4, [srcq + xq]
  palignr         m4, m5, 10
  pshufb          m4, [wiener7_l_shuf]
  jmp .h_main
.h_loop:
  movu            m4, [srcq + xq - 6]
.h_main:
  movu            m5, [srcq + xq + 2]
  movu            m6, [srcq + xq + 6]
  test         edgeb, 2 ; LR_HAVE_RIGHT
  jnz .h_have_right
  cmp             xd, -38
  jl .h_have_right
  movd           xm3, xd
  vpbroadcastd    m0, [pb_wiener7_l]
  vpbroadcastd    m1, [pb_wiener7_m]
  vpbroadcastd    m2, [pb_wiener7_r]
  vpbroadcastb    m3, xm3
  psubb           m0, m3
  psubb           m1, m3
  psubb           m2, m3
  movu            m3, [pb_0to31]
  pminub          m0, m3
  pminub          m1, m3
  pminub          m2, m3
  pshufb          m4, m0
  pshufb          m5, m1
  pshufb          m6, m2
  cmp             xd, -9*2
  jne .hack
  vpbroadcastw   xm3, [srcq + xq + 16]
  vinserti128     m5, xm3, 1
  jmp .h_have_right
.hack:
  cmp             xd, -1*2
  jne .h_have_right
  vpbroadcastw   xm5, [srcq + xq]
.h_have_right:
  pshufb          m6, m10
  pshufb          m0, m4, m11
  pshufb          m2, m5, m12
  paddw           m0, m2
  pmaddwd         m0, m7
  pshufb          m2, m4, m13
  pshufb          m4, m14
  paddw           m2, m4
  pmaddwd         m2, m8
  pshufb          m1, m6, m11
  pshufb          m5, m11
  pmaddwd         m1, m7
  pmaddwd         m5, m7
  pshufb          m3, m6, m13
  pshufb          m6, m14
  paddw           m3, m6
  pmaddwd         m3, m8
  paddd           m0, m2
  paddd           m1, m3
  pshufb          m1, m15
  paddd           m1, m5
  movq           xm4, rhq
  pxor            m5, m5
  paddd           m0, m9
  paddd           m1, m9
  psrad           m0, xm4
  psrad           m1, xm4
  packssdw        m0, m1
  pmaxsw          m0, m5
  mova   [dstq + xq], m0
  add             xq, 32
  jl .h_loop
  add           srcq, ssq
  add           dstq, 384*2
  dec             hd
  jg .v_loop
  RET

INIT_YMM avx2
cglobal wiener_filter7_v_16bpc, 6, 15, 13, dst, ds, mid, f, w, h, edge, bdmax
  movifnidn     wd, wm
  movifnidn     hd, hm
  movifnidn  edgeb, edgem
  pxor          m6, m6
  vpbroadcastd  m7, [fq]
  vpbroadcastw  m8, [fq + 4]
  vpbroadcastd  m9, [fq + 6]
  popcnt    bdmaxd, bdmaxm
  vpbroadcastd m10, [nd_1047552]
  movq        xm11, [pq_11]
  cmp       bdmaxd, 10
  je .bits10
  vpbroadcastd m10, [nd_1048320]
  movq        xm11, [pq_9]
.bits10:
  vpbroadcastw m12, bdmaxm
  add           wq, wq
  add         midq, wq
  add         dstq, wq
  neg           wq
  DEFINE_ARGS dst, ds, mid, ms, w, h, edge, x
  mov          msq, 2*384
  mov           t0, midq
  mov           t1, t0
  lea           t2, [t1 + msq]
  lea           t3, [t2 + msq]
  lea           t4, [t3 + msq]
  lea           t5, [t4 + msq]
  lea           t6, [t5 + msq]
  test       edgeb, 4 ; LR_HAVE_TOP
  jnz .have_top
  mov           t0, t3
  mov           t1, t3
  mov           t2, t3
.have_top:
  cmp           hd, 3
  jg .v_loop
  test       edgeb, 8 ; LR_HAVE_BOTTOM
  jz .no_bottom0
  cmp           hd, 1
  jg .v_loop
  jmp .h3
.no_bottom0:
  cmp           hd, 2
  je .h2
  jns .h3
.h1:
  mov           t4, t3
.h2:
  mov           t5, t4
.h3:
  mov           t6, t5
.v_loop:
  mov           xq, wq
.h_loop:
  mova          m1, [t0 + xq]
  mova          m2, [t1 + xq]
  mova          m3, [t5 + xq]
  mova          m4, [t6 + xq]
  punpcklwd     m0, m1, m2
  pmaddwd       m0, m7
  punpckhwd     m1, m2
  pmaddwd       m1, m7
  punpcklwd     m2, m4, m3
  pmaddwd       m2, m7
  punpckhwd     m4, m3
  pmaddwd       m4, m7
  paddd         m0, m2
  paddd         m1, m4
  mova          m3, [t2 + xq]
  mova          m4, [t4 + xq]
  punpcklwd     m2, m3, m4
  pmaddwd       m2, m8
  punpckhwd     m3, m4
  pmaddwd       m3, m8
  paddd         m0, m2
  paddd         m1, m3
  mova          m3, [t3 + xq]
  punpcklwd     m2, m3, m6
  pmaddwd       m2, m9
  punpckhwd     m3, m6
  pmaddwd       m3, m9
  paddd         m0, m2
  paddd         m1, m3
  paddd         m0, m10
  paddd         m1, m10
  psrad         m0, xm11
  psrad         m1, xm11
  packusdw      m0, m1
  pminuw        m0, m12
  mova [dstq + xq], m0
  add           xq, 32
  jl .h_loop
  add         dstq, dsq
  mov           t0, t1
  mov           t1, t2
  mov           t2, t3
  mov           t3, t4
  mov           t4, t5
  mov           t5, t6
  add           t6, msq
  cmp           hd, 4
  jg .next_row
  test       edgeb, 8 ; LR_HAVE_BOTTOM
  jz .no_bottom
  cmp           hd, 2
  jg .next_row
.no_bottom:
  mov           t6, t5
.next_row:
  dec           hd
  jg .v_loop
  RET

%endif ; ARCH_X86_64
