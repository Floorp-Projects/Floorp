; ***** BEGIN LICENSE BLOCK *****
; Version: MPL 1.1/GPL 2.0/LGPL 2.1
; 
; The contents of this file are subject to the Mozilla Public License Version
; 1.1 (the "License"); you may not use this file except in compliance with
; the License. You may obtain a copy of the License at
; http://www.mozilla.org/MPL/
; 
; Software distributed under the License is distributed on an "AS IS" basis,
; WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
; for the specific language governing rights and limitations under the
; License.
; 
; The Original Code is the Solaris software cryptographic token.
; 
; The Initial Developer of the Original Code is
; Sun Microsystems, Inc.
; Portions created by the Initial Developer are Copyright (C) 2005
; the Initial Developer. All Rights Reserved.
; 
; Contributor(s):
;   Sun Microsystems, Inc.
;   Makoto Kato <m_kato@ga2.so-net.ne.jp>
; 
; Alternatively, the contents of this file may be used under the terms of
; either the GNU General Public License Version 2 or later (the "GPL"), or
; the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
; in which case the provisions of the GPL or the LGPL are applicable instead
; of those above. If you wish to allow use of your version of this file only
; under the terms of either the GPL or the LGPL, and not to allow others to
; use your version of this file under the terms of the MPL, indicate your
; decision by deleting the provisions above and replace them with the notice
; and other provisions required by the GPL or the LGPL. If you do not delete
; the provisions above, a recipient may use your version of this file under
; the terms of any one of the MPL, the GPL or the LGPL.
; 
; ***** END LICENSE BLOCK ***** */

;
; This code is converted from mpi_amd64_gas.asm for MASM for x64.
;

; ------------------------------------------------------------------------
;
;  Implementation of s_mpv_mul_set_vec which exploits
;  the 64X64->128 bit  unsigned multiply instruction.
;
; ------------------------------------------------------------------------

; r = a * digit, r and a are vectors of length len
; returns the carry digit
; r and a are 64 bit aligned.
;
; uint64_t
; s_mpv_mul_set_vec64(uint64_t *r, uint64_t *a, int len, uint64_t digit)
;

.CODE

s_mpv_mul_set_vec64 PROC

        ; compatibilities for paramenter registers
        ;
        ; About GAS and MASM, the usage of parameter registers are different.

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov edx, r8d
        mov rcx, r9

        xor rax, rax
        test rdx, rdx
        jz L17
        mov r8, rdx
        xor r9, r9

L15:
        cmp r8, 8
        jb  L16
        mov rax, [rsi]
        mov r11, [8+rsi]
        mul rcx
        add rax, r9
        adc rdx, 0
        mov [0+rdi], rax
        mov r9, rdx
        mov rax,r11
        mov r11, [16+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [8+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [24+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [16+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [32+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [24+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [40+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [32+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [48+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [40+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [56+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [48+rdi],rax
        mov r9,rdx
        mov rax,r11
        mul rcx
        add rax,r9
        adc rdx,0
        mov [56+rdi],rax
        mov r9,rdx
        add rsi, 64
        add rdi, 64
        sub r8, 8
        jz L17
        jmp L15

L16:
        mov rax, [0+rsi]
        mul rcx
        add rax, r9
        adc rdx,0
        mov [0+rdi],rax
        mov r9,rdx
        dec r8
        jz L17
        mov rax, [8+rsi]
        mul rcx
        add rax,r9
        adc rdx,0
        mov [8+rdi], rax
        mov r9, rdx
        dec r8
        jz L17
        mov rax, [16+rsi]
        mul rcx
        add rax, r9
        adc rdx, 0
        mov [16+rdi],rax
        mov r9,rdx
        dec r8
        jz L17
        mov rax, [24+rsi]
        mul rcx
        add rax, r9
        adc rdx, 0
        mov [24+rdi], rax
        mov r9, rdx
        dec r8
        jz L17
        mov rax, [32+rsi]
        mul rcx
        add rax, r9
        adc rdx, 0
        mov [32+rdi],rax
        mov r9, rdx
        dec r8
        jz L17
        mov rax, [40+rsi]
        mul rcx
        add rax, r9
        adc rdx, 0
        mov [40+rdi], rax
        mov r9, rdx
        dec r8
        jz L17
        mov rax, [48+rsi]
        mul rcx
        add rax, r9
        adc rdx, 0
        mov [48+rdi], rax
        mov r9, rdx
        dec r8
        jz L17

L17:
        mov rax, r9
        pop rsi
        pop rdi
        ret

s_mpv_mul_set_vec64 ENDP


;------------------------------------------------------------------------
;
; Implementation of s_mpv_mul_add_vec which exploits
; the 64X64->128 bit  unsigned multiply instruction.
;
;------------------------------------------------------------------------

; r += a * digit, r and a are vectors of length len
; returns the carry digit
; r and a are 64 bit aligned.
;
; uint64_t
; s_mpv_mul_add_vec64(uint64_t *r, uint64_t *a, int len, uint64_t digit)
; 

s_mpv_mul_add_vec64 PROC

        ; compatibilities for paramenter registers
        ;
        ; About GAS and MASM, the usage of parameter registers are different.

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov edx, r8d
        mov rcx, r9

        xor rax, rax
        test rdx, rdx
        jz L27
        mov r8, rdx
        xor r9, r9

L25:
        cmp r8, 8
        jb L26
        mov rax, [0+rsi]
        mov r10, [0+rdi]
        mov r11, [8+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [8+rdi]
        add rax,r9
        adc rdx,0
        mov [0+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [16+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [16+rdi]
        add rax,r9
        adc rdx,0
        mov [8+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [24+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [24+rdi]
        add rax,r9
        adc rdx,0
        mov [16+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [32+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [32+rdi]
        add rax,r9
        adc rdx,0
        mov [24+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [40+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [40+rdi]
        add rax,r9
        adc rdx,0
        mov [32+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [48+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [48+rdi]
        add rax,r9
        adc rdx,0
        mov [40+rdi],rax
        mov r9,rdx
        mov rax,r11
        mov r11, [56+rsi]
        mul rcx
        add rax,r10
        adc rdx,0
        mov r10, [56+rdi]
        add rax,r9
        adc rdx,0
        mov [48+rdi],rax
        mov r9,rdx
        mov rax,r11
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [56+rdi],rax
        mov r9,rdx
        add rsi,64
        add rdi,64
        sub r8, 8
        jz L27
        jmp L25

L26:
        mov rax, [0+rsi]
        mov r10, [0+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [0+rdi],rax
        mov r9,rdx
        dec r8
        jz L27
        mov rax, [8+rsi]
        mov r10, [8+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [8+rdi],rax
        mov r9,rdx
        dec r8
        jz L27
        mov rax, [16+rsi]
        mov r10, [16+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [16+rdi],rax
        mov r9,rdx
        dec r8
        jz L27
        mov rax, [24+rsi]
        mov r10, [24+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [24+rdi],rax
        mov r9,rdx
        dec r8
        jz L27
        mov rax, [32+rsi]
        mov r10, [32+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [32+rdi],rax
        mov r9,rdx
        dec r8
        jz L27
        mov rax, [40+rsi]
        mov r10, [40+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax,r9
        adc rdx,0
        mov [40+rdi],rax
        mov r9,rdx
        dec r8
        jz L27
        mov rax, [48+rsi]
        mov r10, [48+rdi]
        mul rcx
        add rax,r10
        adc rdx,0
        add rax, r9
        adc rdx, 0
        mov [48+rdi], rax
        mov r9, rdx
        dec r8
        jz L27

L27:
        mov rax, r9

        pop rsi
        pop rdi
        ret

s_mpv_mul_add_vec64 ENDP

END
