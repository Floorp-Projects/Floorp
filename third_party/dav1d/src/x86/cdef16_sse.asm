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

SECTION_RODATA

dir_shift: times 4 dw 0x4000
           times 4 dw 0x1000

pw_128:    times 4 dw 128

cextern cdef_dir_8bpc_ssse3.main
cextern cdef_dir_8bpc_sse4.main
cextern shufw_6543210x

SECTION .text

%macro REPX 2-*
    %xdefine %%f(x) %1
%rep %0 - 1
    %rotate 1
    %%f(%1)
%endrep
%endmacro

%macro CDEF_DIR 0
%if ARCH_X86_64
cglobal cdef_dir_16bpc, 4, 7, 16, src, stride, var, bdmax
    lea             r6, [dir_shift]
    shr         bdmaxd, 11 ; 0 for 10bpc, 1 for 12bpc
    movddup         m7, [r6+bdmaxq*8]
    lea             r6, [strideq*3]
    mova            m0, [srcq+strideq*0]
    mova            m1, [srcq+strideq*1]
    mova            m2, [srcq+strideq*2]
    mova            m3, [srcq+r6       ]
    lea           srcq, [srcq+strideq*4]
    mova            m4, [srcq+strideq*0]
    mova            m5, [srcq+strideq*1]
    mova            m6, [srcq+strideq*2]
    REPX {pmulhuw x, m7}, m0, m1, m2, m3, m4, m5, m6
    pmulhuw         m7, [srcq+r6       ]
    pxor            m8, m8
    packuswb        m9, m0, m1
    packuswb       m10, m2, m3
    packuswb       m11, m4, m5
    packuswb       m12, m6, m7
    REPX {psadbw x, m8}, m9, m10, m11, m12
    packssdw        m9, m10
    packssdw       m11, m12
    packssdw        m9, m11
    jmp mangle(private_prefix %+ _cdef_dir_8bpc %+ SUFFIX).main
%else
cglobal cdef_dir_16bpc, 2, 4, 8, 96, src, stride, var, bdmax
    mov         bdmaxd, bdmaxm
    LEA             r2, dir_shift
    shr         bdmaxd, 11
    movddup         m7, [r2+bdmaxq*8]
    lea             r3, [strideq*3]
    pmulhuw         m3, m7, [srcq+strideq*0]
    pmulhuw         m4, m7, [srcq+strideq*1]
    pmulhuw         m5, m7, [srcq+strideq*2]
    pmulhuw         m6, m7, [srcq+r3       ]
    movddup         m1, [r2-dir_shift+pw_128]
    lea           srcq, [srcq+strideq*4]
    pxor            m0, m0
    packuswb        m2, m3, m4
    psubw           m3, m1
    psubw           m4, m1
    mova    [esp+0x00], m3
    mova    [esp+0x10], m4
    packuswb        m3, m5, m6
    psadbw          m2, m0
    psadbw          m3, m0
    psubw           m5, m1
    psubw           m6, m1
    packssdw        m2, m3
    mova    [esp+0x20], m5
    mova    [esp+0x50], m6
    pmulhuw         m4, m7, [srcq+strideq*0]
    pmulhuw         m5, m7, [srcq+strideq*1]
    pmulhuw         m6, m7, [srcq+strideq*2]
    pmulhuw         m7,     [srcq+r3       ]
    packuswb        m3, m4, m5
    packuswb        m1, m6, m7
    psadbw          m3, m0
    psadbw          m1, m0
    packssdw        m3, m1
    movddup         m1, [r2-dir_shift+pw_128]
    LEA             r2, shufw_6543210x
    jmp mangle(private_prefix %+ _cdef_dir_8bpc %+ SUFFIX).main
%endif
%endmacro

INIT_XMM ssse3
CDEF_DIR

INIT_XMM sse4
CDEF_DIR
