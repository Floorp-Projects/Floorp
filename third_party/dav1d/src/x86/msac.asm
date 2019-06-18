; Copyright © 2019, VideoLAN and dav1d authors
; Copyright © 2019, Two Orioles, LLC
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

SECTION_RODATA 64 ; avoids cacheline splits

dw 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0
pw_0xff00: times 8 dw 0xff00
pw_32:     times 8 dw 32

struc msac
    .buf:        resq 1
    .end:        resq 1
    .dif:        resq 1
    .rng:        resd 1
    .cnt:        resd 1
    .update_cdf: resd 1
endstruc

%define m(x) mangle(private_prefix %+ _ %+ x %+ SUFFIX)

SECTION .text

%if WIN64
DECLARE_REG_TMP 3
%define buf rsp+8 ; shadow space
%else
DECLARE_REG_TMP 0
%define buf rsp-40 ; red zone
%endif

INIT_XMM sse2
cglobal msac_decode_symbol_adapt4, 3, 7, 6, s, cdf, ns
    movd           m2, [sq+msac.rng]
    movq           m1, [cdfq]
    lea           rax, [pw_0xff00]
    movq           m3, [sq+msac.dif]
    mov           r3d, [sq+msac.update_cdf]
    mov           r4d, nsd
    neg           nsq
    pshuflw        m2, m2, q0000
    movd     [buf+12], m2
    pand           m2, [rax]
    mova           m0, m1
    psrlw          m1, 6
    psllw          m1, 7
    pmulhuw        m1, m2
    movq           m2, [rax+nsq*2]
    pshuflw        m3, m3, q3333
    paddw          m1, m2
    mova     [buf+16], m1
    psubusw        m1, m3
    pxor           m2, m2
    pcmpeqw        m1, m2 ; c >= v
    pmovmskb      eax, m1
    test          r3d, r3d
    jz .renorm ; !allow_update_cdf

; update_cdf:
    movzx         r3d, word [cdfq+r4*2] ; count
    pcmpeqw        m2, m2
    mov           r2d, r3d
    shr           r3d, 4
    cmp           r4d, 4
    sbb           r3d, -5 ; (count >> 4) + (n_symbols > 3) + 4
    cmp           r2d, 32
    adc           r2d, 0  ; count + (count < 32)
    movd           m3, r3d
    pavgw          m2, m1 ; i >= val ? -1 : 32768
    psubw          m2, m0 ; for (i = 0; i < val; i++)
    psubw          m0, m1 ;     cdf[i] += (32768 - cdf[i]) >> rate;
    psraw          m2, m3 ; for (; i < n_symbols - 1; i++)
    paddw          m0, m2 ;     cdf[i] += ((  -1 - cdf[i]) >> rate) + 1;
    movq       [cdfq], m0
    mov   [cdfq+r4*2], r2w

.renorm:
    tzcnt         eax, eax
    mov            r4, [sq+msac.dif]
    movzx         r1d, word [buf+rax+16] ; v
    movzx         r2d, word [buf+rax+14] ; u
    shr           eax, 1
.renorm2:
    not            r4
    sub           r2d, r1d ; rng
    shl            r1, 48
    add            r4, r1  ; ~dif
    mov           r1d, [sq+msac.cnt]
    movifnidn      t0, sq
    bsr           ecx, r2d
    xor           ecx, 15  ; d
    shl           r2d, cl
    shl            r4, cl
    mov [t0+msac.rng], r2d
    not            r4
    sub           r1d, ecx
    jge .end ; no refill required

; refill:
    mov            r2, [t0+msac.buf]
    mov           rcx, [t0+msac.end]
    lea            r5, [r2+8]
    cmp            r5, rcx
    jg .refill_eob
    mov            r2, [r2]
    lea           ecx, [r1+23]
    add           r1d, 16
    shr           ecx, 3   ; shift_bytes
    bswap          r2
    sub            r5, rcx
    shl           ecx, 3   ; shift_bits
    shr            r2, cl
    sub           ecx, r1d ; shift_bits - 16 - cnt
    mov           r1d, 48
    shl            r2, cl
    mov [t0+msac.buf], r5
    sub           r1d, ecx ; cnt + 64 - shift_bits
    xor            r4, r2
.end:
    mov [t0+msac.cnt], r1d
    mov [t0+msac.dif], r4
    RET
.refill_eob: ; avoid overreading the input buffer
    mov            r5, rcx
    mov           ecx, 40
    sub           ecx, r1d ; c
.refill_eob_loop:
    cmp            r2, r5
    jge .refill_eob_end    ; eob reached
    movzx         r1d, byte [r2]
    inc            r2
    shl            r1, cl
    xor            r4, r1
    sub           ecx, 8
    jge .refill_eob_loop
.refill_eob_end:
    mov           r1d, 40
    sub           r1d, ecx
    mov [t0+msac.buf], r2
    mov [t0+msac.dif], r4
    mov [t0+msac.cnt], r1d
    RET

cglobal msac_decode_symbol_adapt8, 3, 7, 6, s, cdf, ns
    movd           m2, [sq+msac.rng]
    movu           m1, [cdfq]
    lea           rax, [pw_0xff00]
    movq           m3, [sq+msac.dif]
    mov           r3d, [sq+msac.update_cdf]
    mov           r4d, nsd
    neg           nsq
    pshuflw        m2, m2, q0000
    movd     [buf+12], m2
    punpcklqdq     m2, m2
    mova           m0, m1
    psrlw          m1, 6
    pand           m2, [rax]
    psllw          m1, 7
    pmulhuw        m1, m2
    movu           m2, [rax+nsq*2]
    pshuflw        m3, m3, q3333
    paddw          m1, m2
    punpcklqdq     m3, m3
    mova     [buf+16], m1
    psubusw        m1, m3
    pxor           m2, m2
    pcmpeqw        m1, m2
    pmovmskb      eax, m1
    test          r3d, r3d
    jz m(msac_decode_symbol_adapt4).renorm
    movzx         r3d, word [cdfq+r4*2]
    pcmpeqw        m2, m2
    mov           r2d, r3d
    shr           r3d, 4
    cmp           r4d, 4 ; may be called with n_symbols < 4
    sbb           r3d, -5
    cmp           r2d, 32
    adc           r2d, 0
    movd           m3, r3d
    pavgw          m2, m1
    psubw          m2, m0
    psubw          m0, m1
    psraw          m2, m3
    paddw          m0, m2
    movu       [cdfq], m0
    mov   [cdfq+r4*2], r2w
    jmp m(msac_decode_symbol_adapt4).renorm

cglobal msac_decode_symbol_adapt16, 3, 7, 6, s, cdf, ns
    movd           m4, [sq+msac.rng]
    movu           m2, [cdfq]
    lea           rax, [pw_0xff00]
    movu           m3, [cdfq+16]
    movq           m5, [sq+msac.dif]
    mov           r3d, [sq+msac.update_cdf]
    mov           r4d, nsd
    neg           nsq
%if WIN64
    sub           rsp, 48 ; need 36 bytes, shadow space is only 32
%endif
    pshuflw        m4, m4, q0000
    movd      [buf-4], m4
    punpcklqdq     m4, m4
    mova           m0, m2
    psrlw          m2, 6
    mova           m1, m3
    psrlw          m3, 6
    pand           m4, [rax]
    psllw          m2, 7
    psllw          m3, 7
    pmulhuw        m2, m4
    pmulhuw        m3, m4
    movu           m4, [rax+nsq*2]
    pshuflw        m5, m5, q3333
    paddw          m2, m4
    psubw          m4, [rax-pw_0xff00+pw_32]
    punpcklqdq     m5, m5
    paddw          m3, m4
    mova        [buf], m2
    mova     [buf+16], m3
    psubusw        m2, m5
    psubusw        m3, m5
    pxor           m4, m4
    pcmpeqw        m2, m4
    pcmpeqw        m3, m4
    packsswb       m5, m2, m3
    pmovmskb      eax, m5
    test          r3d, r3d
    jz .renorm
    movzx         r3d, word [cdfq+r4*2]
    pcmpeqw        m4, m4
    mova           m5, m4
    lea           r2d, [r3+80] ; only support n_symbols >= 4
    shr           r2d, 4
    cmp           r3d, 32
    adc           r3d, 0
    pavgw          m4, m2
    pavgw          m5, m3
    psubw          m4, m0
    psubw          m0, m2
    movd           m2, r2d
    psubw          m5, m1
    psubw          m1, m3
    psraw          m4, m2
    psraw          m5, m2
    paddw          m0, m4
    paddw          m1, m5
    movu       [cdfq], m0
    movu    [cdfq+16], m1
    mov   [cdfq+r4*2], r3w
.renorm:
    tzcnt         eax, eax
    mov            r4, [sq+msac.dif]
    movzx         r1d, word [buf+rax*2]
    movzx         r2d, word [buf+rax*2-2]
%if WIN64
    add           rsp, 48
%endif
    jmp m(msac_decode_symbol_adapt4).renorm2

%endif
