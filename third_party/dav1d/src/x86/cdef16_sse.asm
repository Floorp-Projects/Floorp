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

%ifn ARCH_X86_64
SECTION_RODATA 16

pq_dir_shr:      dq  2,  4
%endif

SECTION .text

cextern cdef_dir_8bpc_ssse3

INIT_XMM ssse3
cglobal cdef_dir_16bpc, 2, 4, 4, 32 + 8*8, src, ss, var, bdmax
  bsr   bdmaxd, bdmaxm
%if ARCH_X86_64
  movzx bdmaxq, bdmaxw
  sub   bdmaxq, 7
  movq      m4, bdmaxq
%else
  push      r4
  sub   bdmaxd, 9
  LEA       r4, pq_dir_shr
  movq      m4, [r4 + bdmaxd*4]
  pop       r4
%endif
  DEFINE_ARGS src, ss, var, ss3
  lea     ss3q, [ssq*3]
  mova      m0, [srcq + ssq*0]
  mova      m1, [srcq + ssq*1]
  mova      m2, [srcq + ssq*2]
  mova      m3, [srcq + ss3q]
  psraw     m0, m4
  psraw     m1, m4
  psraw     m2, m4
  psraw     m3, m4
  packuswb  m0, m1
  packuswb  m2, m3
  mova [rsp + 32 + 0*8], m0
  mova [rsp + 32 + 2*8], m2
  lea     srcq, [srcq + ssq*4]
  mova      m0, [srcq + ssq*0]
  mova      m1, [srcq + ssq*1]
  mova      m2, [srcq + ssq*2]
  mova      m3, [srcq + ss3q]
  psraw     m0, m4
  psraw     m1, m4
  psraw     m2, m4
  psraw     m3, m4
  packuswb  m0, m1
  packuswb  m2, m3
  mova [rsp + 32 + 4*8], m0
  mova [rsp + 32 + 6*8], m2
  lea     srcq, [rsp + 32] ; WIN64 shadow space
  mov      ssq, 8
%if ARCH_X86_64
  call mangle(private_prefix %+ _cdef_dir_8bpc %+ SUFFIX)
%else
  movifnidn vard, varm
  push     eax ; align stack
  push    vard
  push     ssd
  push    srcd
  call mangle(private_prefix %+ _cdef_dir_8bpc)
  add      esp, 0x10
%endif
  RET
