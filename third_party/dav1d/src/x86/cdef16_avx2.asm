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

SECTION .text

cextern cdef_dir_8bpc_avx2

INIT_YMM avx2
cglobal cdef_dir_16bpc, 4, 4, 3, 32 + 8*8, src, ss, var, bdmax
  popcnt  bdmaxd, bdmaxd
  movzx   bdmaxq, bdmaxw
  sub     bdmaxq, 8
  movq       xm2, bdmaxq
  DEFINE_ARGS src, ss, var, ss3
  lea       ss3q, [ssq*3]
  mova       xm0, [srcq + ssq*0]
  mova       xm1, [srcq + ssq*1]
  vinserti128 m0, [srcq + ssq*2], 1
  vinserti128 m1, [srcq + ss3q], 1
  psraw       m0, xm2
  psraw       m1, xm2
  vpackuswb   m0, m1
  mova [rsp + 32 + 0*8], m0
  lea       srcq, [srcq + ssq*4]
  mova       xm0, [srcq + ssq*0]
  mova       xm1, [srcq + ssq*1]
  vinserti128 m0, [srcq + ssq*2], 1
  vinserti128 m1, [srcq + ss3q], 1
  psraw       m0, xm2
  psraw       m1, xm2
  vpackuswb   m0, m1
  mova [rsp + 32 + 4*8], m0
  lea       srcq, [rsp + 32] ; WIN64 shadow space
  mov        ssq, 8
  call mangle(private_prefix %+ _cdef_dir_8bpc %+ SUFFIX)
  RET

%endif ; ARCH_X86_64
