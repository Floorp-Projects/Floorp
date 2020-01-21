; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

;/* TomsFastMath, a fast ISO C bignum library.
; * 
; * This project is meant to fill in where LibTomMath
; * falls short.  That is speed ;-)
; *
; * This project is public domain and free for all purposes.
; * 
; * Tom St Denis, tomstdenis@iahu.ca
; */

;/*
; * The source file from which this assembly was derived
; * comes from TFM v0.03, which has the above license.
; * This source was from mp_comba_amd64.sun.s and convert to
; * MASM code set.
; */

.CODE

externdef memcpy:PROC

public s_mp_mul_comba_4
public s_mp_mul_comba_8
public s_mp_mul_comba_16
public s_mp_mul_comba_32
public s_mp_sqr_comba_8
public s_mp_sqr_comba_16
public s_mp_sqr_comba_32


; void s_mp_mul_comba_4(const mp_int *A, const mp_int *B, mp_int *C)

        ALIGN 16
s_mp_mul_comba_4 PROC

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov rdx, r8

        push r12
        push rbp
        push rbx
        sub rsp, 64
        mov r9, qword ptr [16+rdi]
        mov rbx, rdx
        mov rdx, qword ptr [16+rsi]
        mov rax, qword ptr [r9]
        mov qword ptr [-64+64+rsp], rax
        mov r8, qword ptr [8+r9]
        mov qword ptr [-56+64+rsp], r8
        mov rbp, qword ptr [16+r9]
        mov qword ptr [-48+64+rsp], rbp
        mov r12, qword ptr [24+r9]
        mov qword ptr [-40+64+rsp], r12
        mov rcx, qword ptr [rdx]
        mov qword ptr [-32+64+rsp], rcx
        mov r10, qword ptr [8+rdx]
        mov qword ptr [-24+64+rsp], r10
        mov r11, qword ptr [16+rdx]
        xor r10d, r10d
        mov r8, r10
        mov r9, r10
        mov rbp, r10
        mov qword ptr [-16+64+rsp], r11
        mov r11, qword ptr [16+rbx]
        mov rax, qword ptr [24+rdx]
        mov qword ptr [-8+64+rsp], rax
        mov rax, qword ptr [-64+64+rsp]
        mul qword ptr [-32+64+rsp]
        add r8, rax
        adc r9, rdx
        adc rbp, 0
        mov qword ptr [r11], r8
        mov r8, rbp
        mov rbp, r10
        mov rax, qword ptr [-64+64+rsp]
        mul qword ptr [-24+64+rsp]
        add r9, rax
        adc r8, rdx
        adc rbp, 0
        mov r12, rbp
        mov rax, qword ptr [-56+64+rsp]
        mul qword ptr [-32+64+rsp]
        add r9, rax
        adc r8, rdx
        adc r12, 0
        mov qword ptr [8+r11], r9
        mov r9, r12
        mov r12, r10
        mov rax, qword ptr [-64+64+rsp]
        mul qword ptr [-16+64+rsp]
        add r8, rax
        adc r9, rdx
        adc r12, 0
        mov rcx, r12
        mov rax, qword ptr [-56+64+rsp]
        mul qword ptr [-24+64+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-48+64+rsp]
        mul qword ptr [-32+64+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [16+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-64+64+rsp]
        mul qword ptr [-8+64+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+64+rsp]
        mul qword ptr [-16+64+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+64+rsp]
        mul qword ptr [-24+64+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-40+64+rsp]
        mul qword ptr [-32+64+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [24+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-56+64+rsp]
        mul qword ptr [-8+64+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+64+rsp]
        mul qword ptr [-16+64+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-40+64+rsp]
        mul qword ptr [-24+64+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [32+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-48+64+rsp]
        mul qword ptr [-8+64+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov r12, r8
        mov rbp, r9
        mov rax, qword ptr [-40+64+rsp]
        mul qword ptr [-16+64+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [40+r11], rcx
        mov r8, rbp
        mov rcx, r12
        mov rax, qword ptr [-40+64+rsp]
        mul qword ptr [-8+64+rsp]
        add r8, rax
        adc rcx, rdx
        adc r10, 0
        mov qword ptr [48+r11], r8
        mov esi, dword ptr [rsi]
        xor esi, dword ptr [rdi]
        test rcx, rcx
        mov qword ptr [56+r11], rcx
        mov dword ptr [8+rbx], 8
        jne L9
        ALIGN 16
L18:
        mov edx, dword ptr [8+rbx]
        lea edi, dword ptr [-1+rdx]
        test edi, edi
        mov dword ptr [8+rbx], edi
        je L9
        lea r10d, dword ptr [-2+rdx]
        cmp qword ptr [r11+r10*8], 0
        je L18
L9:
        mov edx, dword ptr [8+rbx]
        xor r11d, r11d
        test edx, edx
        cmovne r11d, esi
        mov dword ptr [rbx], r11d
        add rsp, 64
        pop rbx
        pop rbp
        pop r12

        pop rsi
        pop rdi

        ret

s_mp_mul_comba_4 ENDP


; void s_mp_mul_comba_8(const mp_int *A, const mp_int *B, mp_int *C)

        ALIGN 16
s_mp_mul_comba_8 PROC

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov rdx, r8

        push r12
        push rbp
        push rbx
        mov rbx, rdx
        sub rsp, 8+128
        mov rdx, qword ptr [16+rdi]
        mov r8, qword ptr [rdx]
        mov qword ptr [-120+128+rsp], r8
        mov rbp, qword ptr [8+rdx]
        mov qword ptr [-112+128+rsp], rbp
        mov r9, qword ptr [16+rdx]
        mov qword ptr [-104+128+rsp], r9
        mov r12, qword ptr [24+rdx]
        mov qword ptr [-96+128+rsp], r12
        mov rcx, qword ptr [32+rdx]
        mov qword ptr [-88+128+rsp], rcx
        mov r10, qword ptr [40+rdx]
        mov qword ptr [-80+128+rsp], r10
        mov r11, qword ptr [48+rdx]
        mov qword ptr [-72+128+rsp], r11
        mov rax, qword ptr [56+rdx]
        mov rdx, qword ptr [16+rsi]
        mov qword ptr [-64+128+rsp], rax
        mov r8, qword ptr [rdx]
        mov qword ptr [-56+128+rsp], r8
        mov rbp, qword ptr [8+rdx]
        mov qword ptr [-48+128+rsp], rbp
        mov r9, qword ptr [16+rdx]
        mov qword ptr [-40+128+rsp], r9
        mov r12, qword ptr [24+rdx]
        mov qword ptr [-32+128+rsp], r12
        mov rcx, qword ptr [32+rdx]
        mov qword ptr [-24+128+rsp], rcx
        mov r10, qword ptr [40+rdx]
        mov qword ptr [-16+128+rsp], r10
        mov r11, qword ptr [48+rdx]
        xor r10d, r10d
        mov r8, r10
        mov r9, r10
        mov rbp, r10
        mov qword ptr [-8+128+rsp], r11
        mov r11, qword ptr [16+rbx]
        mov rax, qword ptr [56+rdx]
        mov qword ptr [128+rsp], rax
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rbp, 0
        mov qword ptr [r11], r8
        mov r8, rbp
        mov rbp, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-48+128+rsp]
        add r9, rax
        adc r8, rdx
        adc rbp, 0
        mov r12, rbp
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-56+128+rsp]
        add r9, rax
        adc r8, rdx
        adc r12, 0
        mov qword ptr [8+r11], r9
        mov r9, r12
        mov r12, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc r12, 0
        mov rcx, r12
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-56+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [16+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-56+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [24+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-56+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [32+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-56+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [40+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [-8+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-56+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [48+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [-8+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-56+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [56+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [-8+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-48+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [64+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [-8+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-40+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [72+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [-8+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-32+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [80+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [-8+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-24+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [88+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [-8+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-16+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [96+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov r12, r8
        mov rbp, r9
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [-8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [104+r11], rcx
        mov r8, rbp
        mov rcx, r12
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [128+rsp]
        add r8, rax
        adc rcx, rdx
        adc r10, 0
        mov qword ptr [112+r11], r8
        mov esi, dword ptr [rsi]
        xor esi, dword ptr [rdi]
        test rcx, rcx
        mov qword ptr [120+r11], rcx
        mov dword ptr [8+rbx], 16
        jne L35
        ALIGN 16
L43:
        mov edx, dword ptr [8+rbx]
        lea edi, dword ptr [-1+rdx]
        test edi, edi
        mov dword ptr [8+rbx], edi
        je L35
        lea eax, dword ptr [-2+rdx]
        cmp qword ptr [r11+rax*8], 0
        je L43
L35:
        mov r11d, dword ptr [8+rbx]
        xor edx, edx
        test r11d, r11d
        cmovne edx, esi
        mov dword ptr [rbx], edx
        add rsp, 8+128
        pop rbx
        pop rbp
        pop r12

        pop rsi
        pop rdi

        ret

s_mp_mul_comba_8 ENDP


; void s_mp_mul_comba_16(const mp_int *A, const mp_int *B, mp_int *C);

        ALIGN 16
s_mp_mul_comba_16 PROC

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov rdx, r8

        push r12
        push rbp
        push rbx
        mov rbx, rdx
        sub rsp, 136+128
        mov rax, qword ptr [16+rdi]
        mov r8, qword ptr [rax]
        mov qword ptr [-120+128+rsp], r8
        mov rbp, qword ptr [8+rax]
        mov qword ptr [-112+128+rsp], rbp
        mov r9, qword ptr [16+rax]
        mov qword ptr [-104+128+rsp], r9
        mov r12, qword ptr [24+rax]
        mov qword ptr [-96+128+rsp], r12
        mov rcx, qword ptr [32+rax]
        mov qword ptr [-88+128+rsp], rcx
        mov r10, qword ptr [40+rax]
        mov qword ptr [-80+128+rsp], r10
        mov rdx, qword ptr [48+rax]
        mov qword ptr [-72+128+rsp], rdx
        mov r11, qword ptr [56+rax]
        mov qword ptr [-64+128+rsp], r11
        mov r8, qword ptr [64+rax]
        mov qword ptr [-56+128+rsp], r8
        mov rbp, qword ptr [72+rax]
        mov qword ptr [-48+128+rsp], rbp
        mov r9, qword ptr [80+rax]
        mov qword ptr [-40+128+rsp], r9
        mov r12, qword ptr [88+rax]
        mov qword ptr [-32+128+rsp], r12
        mov rcx, qword ptr [96+rax]
        mov qword ptr [-24+128+rsp], rcx
        mov r10, qword ptr [104+rax]
        mov qword ptr [-16+128+rsp], r10
        mov rdx, qword ptr [112+rax]
        mov qword ptr [-8+128+rsp], rdx
        mov r11, qword ptr [120+rax]
        mov qword ptr [128+rsp], r11
        mov r11, qword ptr [16+rsi]
        mov r8, qword ptr [r11]
        mov qword ptr [8+128+rsp], r8
        mov rbp, qword ptr [8+r11]
        mov qword ptr [16+128+rsp], rbp
        mov r9, qword ptr [16+r11]
        mov qword ptr [24+128+rsp], r9
        mov r12, qword ptr [24+r11]
        mov qword ptr [32+128+rsp], r12
        mov rcx, qword ptr [32+r11]
        mov qword ptr [40+128+rsp], rcx
        mov r10, qword ptr [40+r11]
        mov qword ptr [48+128+rsp], r10
        mov rdx, qword ptr [48+r11]
        mov qword ptr [56+128+rsp], rdx
        mov rax, qword ptr [56+r11]
        mov qword ptr [64+128+rsp], rax
        mov r8, qword ptr [64+r11]
        mov qword ptr [72+128+rsp], r8
        mov rbp, qword ptr [72+r11]
        mov qword ptr [80+128+rsp], rbp
        mov r9, qword ptr [80+r11]
        mov qword ptr [88+128+rsp], r9
        mov r12, qword ptr [88+r11]
        mov qword ptr [96+128+rsp], r12
        mov rcx, qword ptr [96+r11]
        mov qword ptr [104+128+rsp], rcx
        mov r10, qword ptr [104+r11]
        mov qword ptr [112+128+rsp], r10
        mov rdx, qword ptr [112+r11]
        xor r10d, r10d
        mov r8, r10
        mov r9, r10
        mov rbp, r10
        mov qword ptr [120+128+rsp], rdx
        mov rax, qword ptr [120+r11]
        mov qword ptr [128+128+rsp], rax
        mov r11, qword ptr [16+rbx]
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rbp, 0
        mov qword ptr [r11], r8
        mov r8, rbp
        mov rbp, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [16+128+rsp]
        add r9, rax
        adc r8, rdx
        adc rbp, 0
        mov r12, rbp
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [8+128+rsp]
        add r9, rax
        adc r8, rdx
        adc r12, 0
        mov qword ptr [8+r11], r9
        mov r9, r12
        mov r12, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc r12, 0
        mov rcx, r12
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [16+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [24+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [32+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [40+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [48+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [56+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [64+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [72+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [80+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [88+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [96+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [104+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [8+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [112+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-120+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [16+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [8+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [120+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-112+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [24+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [16+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [128+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-104+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [32+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [24+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [136+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-96+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [40+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [32+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [144+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-88+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [48+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [40+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [152+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-80+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [56+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [48+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [160+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-72+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [64+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [56+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [168+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-64+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [72+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [64+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [176+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-56+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [80+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [72+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [184+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-48+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [88+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [80+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [192+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-40+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [96+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [88+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [200+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-32+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [104+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [96+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [208+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-24+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [112+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov rbp, r9
        mov r12, r8
        mov rax, qword ptr [128+rsp]
        mul qword ptr [104+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [216+r11], rcx
        mov r9, r12
        mov r8, rbp
        mov rcx, r10
        mov rax, qword ptr [-16+128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [120+128+rsp]
        add r8, rax
        adc r9, rdx
        adc rcx, 0
        mov rbp, r9
        mov r12, rcx
        mov rax, qword ptr [128+rsp]
        mul qword ptr [112+128+rsp]
        add r8, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [224+r11], r8
        mov r9, r12
        mov rcx, rbp
        mov r8, r10
        mov rax, qword ptr [-8+128+rsp]
        mul qword ptr [128+128+rsp]
        add rcx, rax
        adc r9, rdx
        adc r8, 0
        mov r12, r8
        mov rbp, r9
        mov rax, qword ptr [128+rsp]
        mul qword ptr [120+128+rsp]
        add rcx, rax
        adc rbp, rdx
        adc r12, 0
        mov qword ptr [232+r11], rcx
        mov r8, rbp
        mov rcx, r12
        mov rax, qword ptr [128+rsp]
        mul qword ptr [128+128+rsp]
        add r8, rax
        adc rcx, rdx
        adc r10, 0
        mov qword ptr [240+r11], r8
        mov esi, dword ptr [rsi]
        xor esi, dword ptr [rdi]
        test rcx, rcx
        mov qword ptr [248+r11], rcx
        mov dword ptr [8+rbx], 32
        jne L76
        ALIGN 16
L84:
        mov edx, dword ptr [8+rbx]
        lea edi, dword ptr [-1+rdx]
        test edi, edi
        mov dword ptr [8+rbx], edi
        je L76
        lea eax, dword ptr [-2+rdx]
        cmp qword ptr [r11+rax*8], 0
        je L84
L76:
        mov edx, dword ptr [8+rbx]
        xor r11d, r11d
        test edx, edx
        cmovne r11d, esi
        mov dword ptr [rbx], r11d
        add rsp, 136+128
        pop rbx
        pop rbp
        pop r12

        pop rsi
        pop rdi

        ret

s_mp_mul_comba_16 ENDP

; void s_mp_mul_comba_32(const mp_int *A, const mp_int *B, mp_int *C)


        ALIGN 16
s_mp_mul_comba_32 PROC ; a "FRAME" function

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov rdx, r8

        push rbp
        mov rbp, rsp
        push r13
        mov r13, rdx
;        mov edx, 256
        mov r8d, 256
        push r12
        mov r12, rsi
        push rbx
        mov rbx, rdi
        sub rsp, 520+32			; +32 for "home" storage
;        mov rsi, qword ptr [16+rdi]
;        lea rdi, qword ptr [-544+rbp]
        mov rdx, qword ptr [16+rdi]
        lea rcx, qword ptr [-544+rbp]
        call memcpy
;        mov rsi, qword ptr [16+r12]
;        lea rdi, qword ptr [-288+rbp]
;        mov edx, 256
        mov rdx, qword ptr [16+r12]
        lea rcx, qword ptr [-288+rbp]
        mov r8d, 256
        call memcpy
        mov r9, qword ptr [16+r13]
        xor r8d, r8d
        mov rsi, r8
        mov rdi, r8
        mov r10, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc rdi, rdx
        adc r10, 0
        mov qword ptr [r9], rsi
        mov rsi, r10
        mov r10, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-280+rbp]
        add rdi, rax
        adc rsi, rdx
        adc r10, 0
        mov r11, r10
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-288+rbp]
        add rdi, rax
        adc rsi, rdx
        adc r11, 0
        mov qword ptr [8+r9], rdi
        mov rdi, r11
        mov r11, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc r11, 0
        mov rcx, r11
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [16+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [24+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [32+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [40+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [48+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [56+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [64+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [72+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [80+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [88+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [96+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [104+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [112+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [120+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [128+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [136+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [144+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [152+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [160+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [168+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [176+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [184+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [192+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [200+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [208+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [216+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [224+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [232+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-288+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [240+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-544+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-280+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-288+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [248+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-536+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-272+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-280+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [256+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-528+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-264+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-272+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [264+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-520+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-256+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-264+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [272+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-512+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-248+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-256+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [280+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-504+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-240+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-248+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [288+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-496+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-232+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-240+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [296+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-488+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-224+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-232+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [304+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-480+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-216+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-224+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [312+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-472+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-184+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-192+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-200+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-208+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-216+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [320+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-464+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-192+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-200+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-208+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [328+r9], rcx
        mov rdi, r11
        mov r11, r10
        mov r10, r8
        mov rax, qword ptr [-456+rbp]
        mul qword ptr [-40+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-48+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-56+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-64+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-72+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-80+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-88+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-96+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-104+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-112+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-120+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-128+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-136+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-144+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-152+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-160+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-168+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-176+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-184+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-192+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-200+rbp]
        add r11, rax
        adc rdi, rdx
        adc r10, 0
        mov qword ptr [336+r9], r11
        mov rsi, r10
        mov r10, r8
        mov rax, qword ptr [-448+rbp]
        mul qword ptr [-40+rbp]
        add rdi, rax
        adc rsi, rdx
        adc r10, 0
        mov rcx, r10
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-48+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-56+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-64+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-72+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-80+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-88+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-96+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-104+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-112+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-120+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-128+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-136+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-144+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-152+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-160+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-168+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-176+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-184+rbp]
        add rdi, rax
        adc rsi, rdx
        adc rcx, 0
        mov r11, rsi
        mov r10, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-192+rbp]
        add rdi, rax
        adc r11, rdx
        adc r10, 0
        mov qword ptr [344+r9], rdi
        mov rcx, r11
        mov rdi, r10
        mov r11, r8
        mov rax, qword ptr [-440+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc r11, 0
        mov rsi, r11
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-176+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-184+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [352+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-432+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-168+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-176+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [360+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-424+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-160+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-168+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [368+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-416+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-152+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-160+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [376+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-408+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-144+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-152+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [384+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-400+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-136+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-144+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [392+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-392+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-128+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-136+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [400+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-384+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-120+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-128+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [408+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-376+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-112+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-120+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [416+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-368+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-104+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-112+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [424+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-360+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-96+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-104+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [432+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-352+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-88+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-96+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [440+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-344+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-80+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-88+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [448+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-336+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-72+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-80+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [456+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-328+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-64+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-72+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [464+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-320+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-56+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r10, rdi
        mov r11, rcx
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-64+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [472+r9], rsi
        mov rdi, r11
        mov rcx, r10
        mov rsi, r8
        mov rax, qword ptr [-312+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-48+rbp]
        add rcx, rax
        adc rdi, rdx
        adc rsi, 0
        mov r10, rdi
        mov r11, rsi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-56+rbp]
        add rcx, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [480+r9], rcx
        mov rdi, r11
        mov rsi, r10
        mov rcx, r8
        mov rax, qword ptr [-304+rbp]
        mul qword ptr [-40+rbp]
        add rsi, rax
        adc rdi, rdx
        adc rcx, 0
        mov r11, rcx
        mov r10, rdi
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-48+rbp]
        add rsi, rax
        adc r10, rdx
        adc r11, 0
        mov qword ptr [488+r9], rsi
        mov rcx, r10
        mov rsi, r11
        mov rax, qword ptr [-296+rbp]
        mul qword ptr [-40+rbp]
        add rcx, rax
        adc rsi, rdx
        adc r8, 0
        mov qword ptr [496+r9], rcx
        mov ecx, dword ptr [r12]
        xor ecx, dword ptr [rbx]
        test rsi, rsi
        mov qword ptr [504+r9], rsi
        mov dword ptr [8+r13], 64
        jne L149
        ALIGN 16
L157:
        mov edx, dword ptr [8+r13]
        lea ebx, dword ptr [-1+rdx]
        test ebx, ebx
        mov dword ptr [8+r13], ebx
        je L149
        lea r12d, dword ptr [-2+rdx]
        cmp qword ptr [r9+r12*8], 0
        je L157
L149:
        mov r9d, dword ptr [8+r13]
        xor edx, edx
        test r9d, r9d
        cmovne edx, ecx
        mov dword ptr [r13], edx
        add rsp, 520+32			; +32 for "home" storage
        pop rbx
        pop r12
        pop r13
        pop rbp
        pop rsi
        pop rdi

        ret

s_mp_mul_comba_32 ENDP


; void s_mp_sqr_comba_4(const mp_int *A, mp_int *B);

        ALIGN 16
s_mp_sqr_comba_4 PROC

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx

        push rbp
        push rbx
        sub rsp, 80
        mov r11, rsi
        xor esi, esi
        mov r10, rsi
        mov rbp, rsi
        mov r8, rsi
        mov rbx, rsi
        mov rcx, qword ptr [16+rdi]
        mov rdi, rsi
        mov rax, qword ptr [rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc rdi, 0
        mov qword ptr [-72+80+rsp], r10
        mov rax, qword ptr [rcx]
        mul qword ptr [8+rcx]
        add rbx, rax
        adc rdi, rdx
        adc rbp, 0
        add rbx, rax
        adc rdi, rdx
        adc rbp, 0
        mov qword ptr [-64+80+rsp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [16+rcx]
        add rdi, rax
        adc rbp, rdx
        adc r8, 0
        add rdi, rax
        adc rbp, rdx
        adc r8, 0
        mov rbx, rbp
        mov rbp, r8
        mov rax, qword ptr [8+rcx]
        mul rax
        add rdi, rax
        adc rbx, rdx
        adc rbp, 0
        mov qword ptr [-56+80+rsp], rdi
        mov r9, rbp
        mov r8, rbx
        mov rdi, rsi
        mov rax, qword ptr [rcx]
        mul qword ptr [24+rcx]
        add r8, rax
        adc r9, rdx
        adc rdi, 0
        add r8, rax
        adc r9, rdx
        adc rdi, 0
        mov rbx, r9
        mov rbp, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [16+rcx]
        add r8, rax
        adc rbx, rdx
        adc rbp, 0
        add r8, rax
        adc rbx, rdx
        adc rbp, 0
        mov qword ptr [-48+80+rsp], r8
        mov r9, rbp
        mov rdi, rbx
        mov r8, rsi
        mov dword ptr [8+r11], 8
        mov dword ptr [r11], 0
        mov rax, qword ptr [8+rcx]
        mul qword ptr [24+rcx]
        add rdi, rax
        adc r9, rdx
        adc r8, 0
        add rdi, rax
        adc r9, rdx
        adc r8, 0
        mov rbx, r9
        mov rbp, r8
        mov rax, qword ptr [16+rcx]
        mul rax
        add rdi, rax
        adc rbx, rdx
        adc rbp, 0
        mov rax, rbp
        mov qword ptr [-40+80+rsp], rdi
        mov rbp, rbx
        mov rdi, rax
        mov rbx, rsi
        mov rax, qword ptr [16+rcx]
        mul qword ptr [24+rcx]
        add rbp, rax
        adc rdi, rdx
        adc rbx, 0
        add rbp, rax
        adc rdi, rdx
        adc rbx, 0
        mov qword ptr [-32+80+rsp], rbp
        mov r9, rbx
        mov rax, qword ptr [24+rcx]
        mul rax
        add rdi, rax
        adc r9, rdx
        adc rsi, 0
        mov rdx, qword ptr [16+r11]
        mov qword ptr [-24+80+rsp], rdi
        mov qword ptr [-16+80+rsp], r9
        mov qword ptr [rdx], r10
        mov r8, qword ptr [-64+80+rsp]
        mov qword ptr [8+rdx], r8
        mov rbp, qword ptr [-56+80+rsp]
        mov qword ptr [16+rdx], rbp
        mov rdi, qword ptr [-48+80+rsp]
        mov qword ptr [24+rdx], rdi
        mov rsi, qword ptr [-40+80+rsp]
        mov qword ptr [32+rdx], rsi
        mov rbx, qword ptr [-32+80+rsp]
        mov qword ptr [40+rdx], rbx
        mov rcx, qword ptr [-24+80+rsp]
        mov qword ptr [48+rdx], rcx
        mov rax, qword ptr [-16+80+rsp]
        mov qword ptr [56+rdx], rax
        mov edx, dword ptr [8+r11]
        test edx, edx
        je L168
        lea ecx, dword ptr [-1+rdx]
        mov rsi, qword ptr [16+r11]
        mov r10d, ecx
        cmp qword ptr [rsi+r10*8], 0
        jne L166
        mov edx, ecx
        ALIGN 16
L167:
        test edx, edx
        mov ecx, edx
        je L171
        dec edx
        mov eax, edx
        cmp qword ptr [rsi+rax*8], 0
        je L167
        mov dword ptr [8+r11], ecx
        mov edx, ecx
L166:
        test edx, edx
        je L168
        mov eax, dword ptr [r11]
        jmp L169

L171:
        mov dword ptr [8+r11], edx
L168:
        xor eax, eax
L169:
        add rsp, 80
        pop rbx
        pop rbp
        mov dword ptr [r11], eax

        pop rsi
        pop rdi

        ret

s_mp_sqr_comba_4 ENDP


; void s_mp_sqr_comba_8(const mp_int *A, mp_int *B);

        ALIGN 16
s_mp_sqr_comba_8 PROC

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx
        mov rdx, r8
        mov rcx, r9

        push r14
        xor r9d, r9d
        mov r14, r9
        mov r10, r9
        push r13
        mov r13, r9
        push r12
        mov r12, r9
        push rbp
        mov rbp, rsi
        mov rsi, r9
        push rbx
        mov rbx, r9
        sub rsp, 8+128
        mov rcx, qword ptr [16+rdi]
        mov rax, qword ptr [rcx]
        mul rax
        add r14, rax
        adc rbx, rdx
        adc r12, 0
        mov qword ptr [-120+128+rsp], r14
        mov rax, qword ptr [rcx]
        mul qword ptr [8+rcx]
        add rbx, rax
        adc r12, rdx
        adc r10, 0
        add rbx, rax
        adc r12, rdx
        adc r10, 0
        mov qword ptr [-112+128+rsp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [16+rcx]
        add r12, rax
        adc r10, rdx
        adc r13, 0
        add r12, rax
        adc r10, rdx
        adc r13, 0
        mov rbx, r10
        mov r10, r13
        mov r13, r9
        mov rax, qword ptr [8+rcx]
        mul rax
        add r12, rax
        adc rbx, rdx
        adc r10, 0
        mov qword ptr [-104+128+rsp], r12
        mov rdi, r10
        mov r11, rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [24+rcx]
        add r11, rax
        adc rdi, rdx
        adc rsi, 0
        add r11, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, rdi
        mov r10, rsi
        mov rdi, r9
        mov rax, qword ptr [8+rcx]
        mul qword ptr [16+rcx]
        add r11, rax
        adc rbx, rdx
        adc r10, 0
        add r11, rax
        adc rbx, rdx
        adc r10, 0
        mov rsi, r9
        mov qword ptr [-96+128+rsp], r11
        mov r8, r10
        mov r12, rbx
        mov r11, r9
        mov rax, qword ptr [rcx]
        mul qword ptr [32+rcx]
        add r12, rax
        adc r8, rdx
        adc r13, 0
        add r12, rax
        adc r8, rdx
        adc r13, 0
        mov rax, qword ptr [8+rcx]
        mul qword ptr [24+rcx]
        add r12, rax
        adc r8, rdx
        adc r13, 0
        add r12, rax
        adc r8, rdx
        adc r13, 0
        mov rbx, r8
        mov r10, r13
        mov r8, r9
        mov rax, qword ptr [16+rcx]
        mul rax
        add r12, rax
        adc rbx, rdx
        adc r10, 0
        mov qword ptr [-88+128+rsp], r12
        mov rax, qword ptr [rcx]
        mul qword ptr [40+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [24+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r10, rdi
        adc r11, rsi
        add rbx, r8
        adc r10, rdi
        adc r11, rsi
        mov qword ptr [-80+128+rsp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [48+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rax, r12
        add r10, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov rax, qword ptr [24+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-72+128+rsp], r10
        mov r10, r11
        mov rax, qword ptr [rcx]
        mul qword ptr [56+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        add rbx, r8
        adc r10, rdi
        adc rax, rsi
        add rbx, r8
        adc r10, rdi
        adc rax, rsi
        mov qword ptr [-64+128+rsp], rbx
        mov r11, rax
        mov rbx, r9
        mov rax, qword ptr [8+rcx]
        mul qword ptr [56+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [16+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rbx, r12
        add r10, r8
        adc r11, r13
        adc rbx, r12
        mov rsi, rbx
        mov rdi, r13
        mov rbx, r11
        mov r13, r12
        mov r11, rsi
        mov rax, qword ptr [32+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-56+128+rsp], r10
        mov r10, r9
        mov rax, qword ptr [16+rcx]
        mul qword ptr [56+rcx]
        mov r8, rax
        mov rdi, rdx
        xor r13, r13
        mov rax, qword ptr [24+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc r13, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc r13, 0
        mov r12, rdi
        mov rax, r13
        add rbx, r8
        adc r11, r12
        adc r10, rax
        add rbx, r8
        adc r11, r12
        adc r10, rax
        mov qword ptr [-48+128+rsp], rbx
        mov r12, r11
        mov rsi, r10
        mov rbx, r9
        mov r11, r9
        mov rax, qword ptr [24+rcx]
        mul qword ptr [56+rcx]
        add r12, rax
        adc rsi, rdx
        adc rbx, 0
        add r12, rax
        adc rsi, rdx
        adc rbx, 0
        mov r13, rbx
        mov rax, qword ptr [32+rcx]
        mul qword ptr [48+rcx]
        add r12, rax
        adc rsi, rdx
        adc r13, 0
        add r12, rax
        adc rsi, rdx
        adc r13, 0
        mov r10, rsi
        mov rbx, r13
        mov r13, r9
        mov rax, qword ptr [40+rcx]
        mul rax
        add r12, rax
        adc r10, rdx
        adc rbx, 0
        mov qword ptr [-40+128+rsp], r12
        mov r8, rbx
        mov rdi, r10
        mov rax, qword ptr [32+rcx]
        mul qword ptr [56+rcx]
        add rdi, rax
        adc r8, rdx
        adc r11, 0
        add rdi, rax
        adc r8, rdx
        adc r11, 0
        mov r10, r8
        mov rbx, r11
        mov rax, qword ptr [40+rcx]
        mul qword ptr [48+rcx]
        add rdi, rax
        adc r10, rdx
        adc rbx, 0
        add rdi, rax
        adc r10, rdx
        adc rbx, 0
        mov qword ptr [-32+128+rsp], rdi
        mov rsi, rbx
        mov r12, r10
        mov rax, qword ptr [40+rcx]
        mul qword ptr [56+rcx]
        add r12, rax
        adc rsi, rdx
        adc r13, 0
        add r12, rax
        adc rsi, rdx
        adc r13, 0
        mov r10, rsi
        mov rbx, r13
        mov rax, qword ptr [48+rcx]
        mul rax
        add r12, rax
        adc r10, rdx
        adc rbx, 0
        mov qword ptr [-24+128+rsp], r12
        mov rdi, r10
        mov rsi, rbx
        mov r10, r9
        mov dword ptr [8+rbp], 16
        mov dword ptr [rbp], 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [56+rcx]
        add rdi, rax
        adc rsi, rdx
        adc r10, 0
        add rdi, rax
        adc rsi, rdx
        adc r10, 0
        mov qword ptr [-16+128+rsp], rdi
        mov r8, r10
        mov rax, qword ptr [56+rcx]
        mul rax
        add rsi, rax
        adc r8, rdx
        adc r9, 0
        mov rax, qword ptr [16+rbp]
        mov qword ptr [-8+128+rsp], rsi
        mov qword ptr [128+rsp], r8
        mov qword ptr [rax], r14
        mov rbx, qword ptr [-112+128+rsp]
        mov qword ptr [8+rax], rbx
        mov rcx, qword ptr [-104+128+rsp]
        mov qword ptr [16+rax], rcx
        mov rdx, qword ptr [-96+128+rsp]
        mov qword ptr [24+rax], rdx
        mov r14, qword ptr [-88+128+rsp]
        mov qword ptr [32+rax], r14
        mov r13, qword ptr [-80+128+rsp]
        mov qword ptr [40+rax], r13
        mov r12, qword ptr [-72+128+rsp]
        mov qword ptr [48+rax], r12
        mov r11, qword ptr [-64+128+rsp]
        mov qword ptr [56+rax], r11
        mov r10, qword ptr [-56+128+rsp]
        mov qword ptr [64+rax], r10
        mov r9, qword ptr [-48+128+rsp]
        mov qword ptr [72+rax], r9
        mov r8, qword ptr [-40+128+rsp]
        mov qword ptr [80+rax], r8
        mov rdi, qword ptr [-32+128+rsp]
        mov qword ptr [88+rax], rdi
        mov rsi, qword ptr [-24+128+rsp]
        mov qword ptr [96+rax], rsi
        mov rbx, qword ptr [-16+128+rsp]
        mov qword ptr [104+rax], rbx
        mov rcx, qword ptr [-8+128+rsp]
        mov qword ptr [112+rax], rcx
        mov rdx, qword ptr [128+rsp]
        mov qword ptr [120+rax], rdx
        mov edx, dword ptr [8+rbp]
        test edx, edx
        je L192
        lea ecx, dword ptr [-1+rdx]
        mov rsi, qword ptr [16+rbp]
        mov r14d, ecx
        cmp qword ptr [rsi+r14*8], 0
        jne L190
        mov edx, ecx
        ALIGN 16
L191:
        test edx, edx
        mov ecx, edx
        je L195
        dec edx
        mov r9d, edx
        cmp qword ptr [rsi+r9*8], 0
        je L191
        mov dword ptr [8+rbp], ecx
        mov edx, ecx
L190:
        test edx, edx
        je L192
        mov eax, dword ptr [rbp]
        jmp L193

L195:
        mov dword ptr [8+rbp], edx
L192:
        xor eax, eax
L193:
        mov dword ptr [rbp], eax
        add rsp, 8+128
        pop rbx
        pop rbp
        pop r12
        pop r13
        pop r14

        pop rsi
        pop rdi

        ret

s_mp_sqr_comba_8 ENDP


; void s_mp_sqr_comba_16(const mp_int *A, mp_int *B)

        ALIGN 16
s_mp_sqr_comba_16 PROC ; A "FRAME" function

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx

        push rbp
        xor r9d, r9d
        mov r8, r9
        mov r11, r9
        mov rbp, rsp
        push r14
        mov r14, rsi
        mov rsi, r9
        push r13
        mov r13, r9
        push r12
        mov r12, r9
        push rbx
        mov rbx, r9
        sub rsp, 256+32			; +32 for "home" storage
        mov rcx, qword ptr [16+rdi]
        mov rax, qword ptr [rcx]
        mul rax
        add r8, rax
        adc rbx, rdx
        adc rsi, 0
        mov qword ptr [-288+rbp], r8
        mov rax, qword ptr [rcx]
        mul qword ptr [8+rcx]
        add rbx, rax
        adc rsi, rdx
        adc r12, 0
        add rbx, rax
        adc rsi, rdx
        adc r12, 0
        mov qword ptr [-280+rbp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [16+rcx]
        add rsi, rax
        adc r12, rdx
        adc r13, 0
        add rsi, rax
        adc r12, rdx
        adc r13, 0
        mov rbx, r12
        mov r10, r13
        mov rax, qword ptr [8+rcx]
        mul rax
        add rsi, rax
        adc rbx, rdx
        adc r10, 0
        mov qword ptr [-272+rbp], rsi
        mov rdi, r10
        mov rsi, r9
        mov r10, rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [24+rcx]
        add r10, rax
        adc rdi, rdx
        adc r11, 0
        add r10, rax
        adc rdi, rdx
        adc r11, 0
        mov r12, rdi
        mov rbx, r11
        mov rdi, r9
        mov rax, qword ptr [8+rcx]
        mul qword ptr [16+rcx]
        add r10, rax
        adc r12, rdx
        adc rbx, 0
        add r10, rax
        adc r12, rdx
        adc rbx, 0
        mov r11, r9
        mov qword ptr [-264+rbp], r10
        mov r8, rbx
        mov r13, r12
        mov r12, r9
        mov rax, qword ptr [rcx]
        mul qword ptr [32+rcx]
        add r13, rax
        adc r8, rdx
        adc r12, 0
        add r13, rax
        adc r8, rdx
        adc r12, 0
        mov rax, qword ptr [8+rcx]
        mul qword ptr [24+rcx]
        add r13, rax
        adc r8, rdx
        adc r12, 0
        add r13, rax
        adc r8, rdx
        adc r12, 0
        mov rbx, r8
        mov r10, r12
        mov r8, r9
        mov rax, qword ptr [16+rcx]
        mul rax
        add r13, rax
        adc rbx, rdx
        adc r10, 0
        mov qword ptr [-256+rbp], r13
        mov rax, qword ptr [rcx]
        mul qword ptr [40+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [24+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r10, rdi
        adc r11, rsi
        add rbx, r8
        adc r10, rdi
        adc r11, rsi
        mov qword ptr [-248+rbp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [48+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rax, r12
        add r10, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov rax, qword ptr [24+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-240+rbp], r10
        mov r10, r11
        mov rax, qword ptr [rcx]
        mul qword ptr [56+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r9
        add rbx, r8
        adc r10, rdi
        adc rdx, rsi
        add rbx, r8
        adc r10, rdi
        adc rdx, rsi
        mov r11, rdx
        mov qword ptr [-232+rbp], rbx
        mov rbx, r9
        mov rax, qword ptr [rcx]
        mul qword ptr [64+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rbx, r12
        add r10, r8
        adc r11, r13
        adc rbx, r12
        mov rax, qword ptr [32+rcx]
        mul rax
        add r10, rax
        adc r11, rdx
        adc rbx, 0
        mov rdi, r13
        mov qword ptr [-224+rbp], r10
        mov rsi, r12
        mov r10, rbx
        mov r12, r9
        mov rax, qword ptr [rcx]
        mul qword ptr [72+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r11, r8
        adc r10, rdi
        adc r12, rsi
        add r11, r8
        adc r10, rdi
        adc r12, rsi
        mov qword ptr [-216+rbp], r11
        mov rbx, r12
        mov rax, qword ptr [rcx]
        mul qword ptr [80+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc rbx, r13
        adc rax, r12
        add r10, r8
        adc rbx, r13
        adc rax, r12
        mov rdx, rax
        mov r11, rbx
        mov rdi, r13
        mov rbx, rdx
        mov rsi, r12
        mov rax, qword ptr [40+rcx]
        mul rax
        add r10, rax
        adc r11, rdx
        adc rbx, 0
        mov qword ptr [-208+rbp], r10
        mov r10, rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [88+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r9
        add r11, r8
        adc r10, rdi
        adc rdx, rsi
        add r11, r8
        adc r10, rdi
        adc rdx, rsi
        mov r13, rdx
        mov qword ptr [-200+rbp], r11
        mov r12, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [96+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov rdx, rdi
        mov r11, rsi
        add r10, r8
        adc r12, rdx
        adc rax, r11
        add r10, r8
        adc r12, rdx
        adc rax, r11
        mov rbx, rdx
        mov r13, rax
        mov rsi, r11
        mov rax, qword ptr [48+rcx]
        mul rax
        add r10, rax
        adc r12, rdx
        adc r13, 0
        mov rdi, rbx
        mov qword ptr [-192+rbp], r10
        mov r10, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [104+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r9
        mov rax, qword ptr [8+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r12, r8
        adc r10, rdi
        adc r13, rsi
        add r12, r8
        adc r10, rdi
        adc r13, rsi
        mov qword ptr [-184+rbp], r12
        mov r12, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [112+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov rbx, rdi
        mov rdx, rsi
        add r10, r8
        adc r12, rbx
        adc rax, rdx
        add r10, r8
        adc r12, rbx
        adc rax, rdx
        mov r11, rdx
        mov r13, rax
        mov rdi, rbx
        mov rax, qword ptr [56+rcx]
        mul rax
        add r10, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-176+rbp], r10
        mov r10, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r9
        mov rax, qword ptr [8+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r12, r8
        adc r10, rdi
        adc r13, rsi
        add r12, r8
        adc r10, rdi
        adc r13, rsi
        mov qword ptr [-168+rbp], r12
        mov r12, r13
        mov rax, qword ptr [8+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [16+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov rbx, rdi
        mov rdx, rsi
        add r10, r8
        adc r12, rbx
        adc rax, rdx
        add r10, r8
        adc r12, rbx
        adc rax, rdx
        mov r11, rdx
        mov r13, rax
        mov rdi, rbx
        mov rax, qword ptr [64+rcx]
        mul rax
        add r10, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-160+rbp], r10
        mov r11, r9
        mov rax, qword ptr [16+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r10, r13
        mov rbx, r9
        mov rax, qword ptr [24+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r12, r8
        adc r10, rdi
        adc r11, rsi
        add r12, r8
        adc r10, rdi
        adc r11, rsi
        mov qword ptr [-152+rbp], r12
        mov rax, qword ptr [24+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [32+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rbx, r12
        add r10, r8
        adc r11, r13
        adc rbx, r12
        mov rdx, rbx
        mov rdi, r13
        mov rbx, r11
        mov rsi, r12
        mov r11, rdx
        mov r12, r9
        mov rax, qword ptr [72+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-144+rbp], r10
        mov r10, r11
        mov rax, qword ptr [32+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [40+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r10, rdi
        adc r12, rsi
        add rbx, r8
        adc r10, rdi
        adc r12, rsi
        mov qword ptr [-136+rbp], rbx
        mov r11, r12
        mov rax, qword ptr [40+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [48+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rax, r12
        add r10, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov rax, qword ptr [80+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-128+rbp], r10
        mov r10, r11
        mov rax, qword ptr [48+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [56+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r9
        add rbx, r8
        adc r10, rdi
        adc rdx, rsi
        add rbx, r8
        adc r10, rdi
        adc rdx, rsi
        mov qword ptr [-120+rbp], rbx
        mov r11, rdx
        mov rbx, r9
        mov rax, qword ptr [56+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [64+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rbx, r12
        add r10, r8
        adc r11, r13
        adc rbx, r12
        mov rdx, rbx
        mov rdi, r13
        mov rbx, r11
        mov rsi, r12
        mov r11, rdx
        mov r12, r9
        mov rax, qword ptr [88+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-112+rbp], r10
        mov r10, r11
        mov rax, qword ptr [64+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [72+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r10, rdi
        adc r12, rsi
        add rbx, r8
        adc r10, rdi
        adc r12, rsi
        mov qword ptr [-104+rbp], rbx
        mov r11, r12
        mov rax, qword ptr [72+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [80+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r9
        mov r13, rdi
        mov r12, rsi
        add r10, r8
        adc r11, r13
        adc rax, r12
        add r10, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov rax, qword ptr [96+rcx]
        mul rax
        add r10, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-96+rbp], r10
        mov r10, r9
        mov rax, qword ptr [80+rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [88+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r12, rdi
        mov rax, rsi
        mov rsi, r9
        add rbx, r8
        adc r11, r12
        adc r10, rax
        add rbx, r8
        adc r11, r12
        adc r10, rax
        mov r12, r9
        mov qword ptr [-88+rbp], rbx
        mov r13, r11
        mov r11, r10
        mov rax, qword ptr [88+rcx]
        mul qword ptr [120+rcx]
        add r13, rax
        adc r11, rdx
        adc r12, 0
        add r13, rax
        adc r11, rdx
        adc r12, 0
        mov rdi, r12
        mov rax, qword ptr [96+rcx]
        mul qword ptr [112+rcx]
        add r13, rax
        adc r11, rdx
        adc rdi, 0
        add r13, rax
        adc r11, rdx
        adc rdi, 0
        mov rbx, r11
        mov r10, rdi
        mov r11, r9
        mov rax, qword ptr [104+rcx]
        mul rax
        add r13, rax
        adc rbx, rdx
        adc r10, 0
        mov qword ptr [-80+rbp], r13
        mov r8, r10
        mov r10, rbx
        mov rax, qword ptr [96+rcx]
        mul qword ptr [120+rcx]
        add r10, rax
        adc r8, rdx
        adc rsi, 0
        add r10, rax
        adc r8, rdx
        adc rsi, 0
        mov r12, r8
        mov rbx, rsi
        mov rax, qword ptr [104+rcx]
        mul qword ptr [112+rcx]
        add r10, rax
        adc r12, rdx
        adc rbx, 0
        add r10, rax
        adc r12, rdx
        adc rbx, 0
        mov qword ptr [-72+rbp], r10
        mov r13, rbx
        mov rbx, r12
        mov rax, qword ptr [104+rcx]
        mul qword ptr [120+rcx]
        add rbx, rax
        adc r13, rdx
        adc r11, 0
        add rbx, rax
        adc r13, rdx
        adc r11, 0
        mov r12, r11
        mov r10, r13
        mov rax, qword ptr [112+rcx]
        mul rax
        add rbx, rax
        adc r10, rdx
        adc r12, 0
        mov qword ptr [-64+rbp], rbx
        mov rdi, r10
        mov rbx, r9
        mov rsi, r12
        mov rax, qword ptr [112+rcx]
        mul qword ptr [120+rcx]
        add rdi, rax
        adc rsi, rdx
        adc rbx, 0
        add rdi, rax
        adc rsi, rdx
        adc rbx, 0
        mov qword ptr [-56+rbp], rdi
        mov r8, rbx
        mov rax, qword ptr [120+rcx]
        mul rax
        add rsi, rax
        adc r8, rdx
        adc r9, 0
        mov qword ptr [-48+rbp], rsi
        mov qword ptr [-40+rbp], r8
        mov dword ptr [8+r14], 32
        mov dword ptr [r14], 0
;        mov rdi, qword ptr [16+r14]
;        lea rsi, qword ptr [-288+rbp]
;        mov edx, 256
        mov rcx, qword ptr [16+r14]
        lea rdx, qword ptr [-288+rbp]
        mov r8d, 256
        call memcpy
        mov edx, dword ptr [8+r14]
        test edx, edx
        je L232
        lea ecx, dword ptr [-1+rdx]
        mov rsi, qword ptr [16+r14]
        mov r9d, ecx
        cmp qword ptr [rsi+r9*8], 0
        jne L230
        mov edx, ecx
        ALIGN 16
L231:
        test edx, edx
        mov ecx, edx
        je L235
        dec edx
        mov eax, edx
        cmp qword ptr [rsi+rax*8], 0
        je L231
        mov dword ptr [8+r14], ecx
        mov edx, ecx
L230:
        test edx, edx
        je L232
        mov eax, dword ptr [r14]
        jmp L233

L235:
        mov dword ptr [8+r14], edx
L232:
        xor eax, eax
L233:
        mov dword ptr [r14], eax
        add rsp, 256+32			; +32 for "home" storage
        pop rbx
        pop r12
        pop r13
        pop r14
        pop rbp
        pop rsi
        pop rdi

        ret

s_mp_sqr_comba_16 ENDP


; void s_mp_sqr_comba_32(const mp_int *A, mp_int *B);

        ALIGN 16
s_mp_sqr_comba_32 PROC ; A "FRAME" function

        push rdi
        push rsi

        mov rdi, rcx
        mov rsi, rdx

        push rbp
        xor r10d, r10d
        mov r8, r10
        mov r11, r10
        mov rbp, rsp
        push r14
        mov r14, rsi
        mov rsi, r10
        push r13
        mov r13, r10
        push r12
        mov r12, r10
        push rbx
        mov rbx, r10
        sub rsp, 512+32			; +32 for "home" storage
        mov rcx, qword ptr [16+rdi]
        mov rax, qword ptr [rcx]
        mul rax
        add r8, rax
        adc rbx, rdx
        adc rsi, 0
        mov qword ptr [-544+rbp], r8
        mov rax, qword ptr [rcx]
        mul qword ptr [8+rcx]
        add rbx, rax
        adc rsi, rdx
        adc r12, 0
        add rbx, rax
        adc rsi, rdx
        adc r12, 0
        mov qword ptr [-536+rbp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [16+rcx]
        add rsi, rax
        adc r12, rdx
        adc r13, 0
        add rsi, rax
        adc r12, rdx
        adc r13, 0
        mov rbx, r12
        mov r9, r13
        mov rax, qword ptr [8+rcx]
        mul rax
        add rsi, rax
        adc rbx, rdx
        adc r9, 0
        mov qword ptr [-528+rbp], rsi
        mov rdi, r9
        mov rsi, r10
        mov r9, rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [24+rcx]
        add r9, rax
        adc rdi, rdx
        adc r11, 0
        add r9, rax
        adc rdi, rdx
        adc r11, 0
        mov r12, rdi
        mov r13, r11
        mov rdi, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [16+rcx]
        add r9, rax
        adc r12, rdx
        adc r13, 0
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov r11, r10
        mov qword ptr [-520+rbp], r9
        mov r8, r13
        mov r13, r12
        mov r12, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [32+rcx]
        add r13, rax
        adc r8, rdx
        adc r12, 0
        add r13, rax
        adc r8, rdx
        adc r12, 0
        mov rax, qword ptr [8+rcx]
        mul qword ptr [24+rcx]
        add r13, rax
        adc r8, rdx
        adc r12, 0
        add r13, rax
        adc r8, rdx
        adc r12, 0
        mov rbx, r8
        mov r9, r12
        mov r8, r10
        mov rax, qword ptr [16+rcx]
        mul rax
        add r13, rax
        adc rbx, rdx
        adc r9, 0
        mov qword ptr [-512+rbp], r13
        mov rax, qword ptr [rcx]
        mul qword ptr [40+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [24+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r9, rdi
        adc r11, rsi
        add rbx, r8
        adc r9, rdi
        adc r11, rsi
        mov qword ptr [-504+rbp], rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [48+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r10
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc r11, r13
        adc rax, r12
        add r9, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov rax, qword ptr [24+rcx]
        mul rax
        add r9, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-496+rbp], r9
        mov r9, r11
        mov rax, qword ptr [rcx]
        mul qword ptr [56+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [32+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r10
        add rbx, r8
        adc r9, rdi
        adc rdx, rsi
        add rbx, r8
        adc r9, rdi
        adc rdx, rsi
        mov r11, rdx
        mov qword ptr [-488+rbp], rbx
        mov rbx, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [64+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc r11, r13
        adc rbx, r12
        add r9, r8
        adc r11, r13
        adc rbx, r12
        mov rax, qword ptr [32+rcx]
        mul rax
        add r9, rax
        adc r11, rdx
        adc rbx, 0
        mov rdi, r13
        mov qword ptr [-480+rbp], r9
        mov rsi, r12
        mov r9, rbx
        mov r12, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [72+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [40+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r11, r8
        adc r9, rdi
        adc r12, rsi
        add r11, r8
        adc r9, rdi
        adc r12, rsi
        mov qword ptr [-472+rbp], r11
        mov rbx, r12
        mov rax, qword ptr [rcx]
        mul qword ptr [80+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r10
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc rbx, r13
        adc rax, r12
        add r9, r8
        adc rbx, r13
        adc rax, r12
        mov rdx, rax
        mov r11, rbx
        mov rdi, r13
        mov rbx, rdx
        mov rsi, r12
        mov rax, qword ptr [40+rcx]
        mul rax
        add r9, rax
        adc r11, rdx
        adc rbx, 0
        mov qword ptr [-464+rbp], r9
        mov r9, rbx
        mov rax, qword ptr [rcx]
        mul qword ptr [88+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [48+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r10
        add r11, r8
        adc r9, rdi
        adc rdx, rsi
        add r11, r8
        adc r9, rdi
        adc rdx, rsi
        mov r13, rdx
        mov qword ptr [-456+rbp], r11
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [96+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, rdi
        mov r11, rsi
        add r9, r8
        adc r12, rax
        adc r13, r11
        add r9, r8
        adc r12, rax
        adc r13, r11
        mov rbx, rax
        mov rsi, r11
        mov rax, qword ptr [48+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rdi, rbx
        mov qword ptr [-448+rbp], r9
        mov r9, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [104+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [56+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r12, r8
        adc r9, rdi
        adc r13, rsi
        add r12, r8
        adc r9, rdi
        adc r13, rsi
        mov qword ptr [-440+rbp], r12
        mov r12, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [112+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r13
        mov rbx, rdi
        mov r13, rsi
        add r9, r8
        adc rdx, rbx
        adc r12, r13
        add r9, r8
        adc rdx, rbx
        adc r12, r13
        mov rax, r12
        mov r11, r13
        mov r12, rdx
        mov r13, rax
        mov rdi, rbx
        mov rsi, r11
        mov rax, qword ptr [56+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov qword ptr [-432+rbp], r9
        mov r9, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [120+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [64+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r8
        mov rdx, rdi
        mov rbx, rsi
        add r12, rax
        adc r9, rdx
        adc r13, rbx
        add r12, rax
        adc r9, rdx
        adc r13, rbx
        mov qword ptr [-424+rbp], r12
        mov r8, rdx
        mov rsi, rax
        mov rdi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [128+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [104+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [96+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [88+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [80+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [72+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [64+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-416+rbp], r9
        mov r9, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [136+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [128+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [120+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [72+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov qword ptr [-408+rbp], r12
        mov rdi, rdx
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [144+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [104+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [96+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [88+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [80+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [72+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-400+rbp], r9
        mov r9, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [152+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [144+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [136+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [128+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [120+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [80+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov qword ptr [-392+rbp], r12
        mov rdi, rdx
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [160+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [104+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [96+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [88+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [80+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-384+rbp], r9
        mov r9, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [168+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [160+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [152+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [144+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [136+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [128+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [120+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [88+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov qword ptr [-376+rbp], r12
        mov rdi, rdx
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [176+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [104+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [96+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [88+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-368+rbp], r9
        mov r9, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [184+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [176+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [168+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [160+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [152+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [144+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [136+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [128+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [120+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [112+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [104+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [96+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov rdi, rdx
        mov qword ptr [-360+rbp], r12
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [192+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [104+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rbx, r8
        mov rax, rdi
        add r9, rsi
        adc r12, rbx
        adc r13, rax
        add r9, rsi
        adc r12, rbx
        adc r13, rax
        mov r11, rax
        mov r8, rbx
        mov rax, qword ptr [96+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rdi, r11
        mov qword ptr [-352+rbp], r9
        mov r9, r13
        mov rax, qword ptr [rcx]
        mul qword ptr [200+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [104+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        mov qword ptr [-344+rbp], r12
        mov r12, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [208+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rdx, r13
        mov rbx, r8
        mov r13, rdi
        add r9, rsi
        adc rdx, rbx
        adc r12, r13
        add r9, rsi
        adc rdx, rbx
        adc r12, r13
        mov rax, r12
        mov r11, r13
        mov r12, rdx
        mov r13, rax
        mov r8, rbx
        mov rdi, r11
        mov rax, qword ptr [104+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov qword ptr [-336+rbp], r9
        mov r9, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [216+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [112+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        mov qword ptr [-328+rbp], r12
        mov rax, qword ptr [rcx]
        mul qword ptr [224+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, r13
        mov rdx, r10
        mov rbx, r8
        mov r12, rdi
        add r9, rsi
        adc rax, rbx
        adc rdx, r12
        add r9, rsi
        adc rax, rbx
        adc rdx, r12
        mov rdi, rdx
        mov r11, r12
        mov r8, rbx
        mov r12, rax
        mov r13, rdi
        mov rdi, r11
        mov rax, qword ptr [112+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov qword ptr [-320+rbp], r9
        mov rbx, r13
        mov r9, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [232+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [120+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc rbx, r8
        adc r9, rdi
        add r12, rsi
        adc rbx, r8
        adc r9, rdi
        mov qword ptr [-312+rbp], r12
        mov r13, r9
        mov rax, qword ptr [rcx]
        mul qword ptr [240+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, r10
        mov r11, r8
        mov rdx, rdi
        add rbx, rsi
        adc r13, r11
        adc rax, rdx
        add rbx, rsi
        adc r13, r11
        adc rax, rdx
        mov r9, rdx
        mov rdx, rax
        mov r12, r13
        mov r8, r11
        mov r13, rdx
        mov rdi, r9
        mov rax, qword ptr [120+rcx]
        mul rax
        add rbx, rax
        adc r12, rdx
        adc r13, 0
        mov qword ptr [-304+rbp], rbx
        mov rbx, r13
        mov r13, r10
        mov rax, qword ptr [rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [8+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [16+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [128+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc rbx, r8
        adc r13, rdi
        add r12, rsi
        adc rbx, r8
        adc r13, rdi
        mov qword ptr [-296+rbp], r12
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [8+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [16+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [24+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov r11, r8
        mov rax, rdi
        add rbx, rsi
        adc r12, r11
        adc r13, rax
        add rbx, rsi
        adc r12, r11
        adc r13, rax
        mov r9, rax
        mov r8, r11
        mov rax, qword ptr [128+rcx]
        mul rax
        add rbx, rax
        adc r12, rdx
        adc r13, 0
        mov rdi, r9
        mov qword ptr [-288+rbp], rbx
        mov r9, r13
        mov rax, qword ptr [16+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov r13, r10
        mov rax, qword ptr [24+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [32+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [136+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        mov qword ptr [-280+rbp], r12
        mov r12, r10
        mov rax, qword ptr [24+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [32+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [40+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rdx, r13
        mov rbx, r8
        mov r13, rdi
        add r9, rsi
        adc rdx, rbx
        adc r12, r13
        add r9, rsi
        adc rdx, rbx
        adc r12, r13
        mov rax, r12
        mov r11, r13
        mov r12, rdx
        mov r13, rax
        mov r8, rbx
        mov rdi, r11
        mov rax, qword ptr [136+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov qword ptr [-272+rbp], r9
        mov r9, r13
        mov r13, r10
        mov rax, qword ptr [32+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [40+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [48+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [144+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        add r12, rsi
        adc r9, r8
        adc r13, rdi
        mov qword ptr [-264+rbp], r12
        mov rax, qword ptr [40+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [48+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [56+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, r13
        mov rdx, r10
        mov rbx, r8
        mov r12, rdi
        add r9, rsi
        adc rax, rbx
        adc rdx, r12
        add r9, rsi
        adc rax, rbx
        adc rdx, r12
        mov rdi, rdx
        mov r11, r12
        mov r8, rbx
        mov r12, rax
        mov r13, rdi
        mov rdi, r11
        mov rax, qword ptr [144+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov r11, r10
        mov qword ptr [-256+rbp], r9
        mov r9, r13
        mov rax, qword ptr [48+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [56+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [64+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [152+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        add r12, rsi
        adc r9, r8
        adc r11, rdi
        add r12, rsi
        adc r9, r8
        adc r11, rdi
        mov qword ptr [-248+rbp], r12
        mov r13, r11
        mov rax, qword ptr [56+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [64+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [72+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [160+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, r10
        mov rdx, rsi
        mov rbx, r8
        mov r12, rdi
        add r9, rdx
        adc r13, rbx
        adc rax, r12
        add r9, rdx
        adc r13, rbx
        adc rax, r12
        mov r11, r12
        mov r8, rdx
        mov rdx, rax
        mov r12, r13
        mov rdi, rbx
        mov r13, rdx
        mov rsi, r11
        mov rax, qword ptr [152+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov qword ptr [-240+rbp], r9
        mov r9, r13
        mov r13, r10
        mov rax, qword ptr [64+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [72+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [80+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [192+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [184+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [176+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [168+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [160+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r8
        mov rdx, rdi
        mov rbx, rsi
        add r12, rax
        adc r9, rdx
        adc r13, rbx
        add r12, rax
        adc r9, rdx
        adc r13, rbx
        mov qword ptr [-232+rbp], r12
        mov r8, rdx
        mov rsi, rax
        mov rdi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [72+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [80+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [88+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [168+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [160+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-224+rbp], r9
        mov r9, r13
        mov rax, qword ptr [80+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [88+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [96+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [192+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [184+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [176+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [168+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov qword ptr [-216+rbp], r12
        mov rdi, rdx
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [88+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [96+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [104+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [176+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [168+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-208+rbp], r9
        mov r9, r13
        mov rax, qword ptr [96+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [104+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [112+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [192+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [184+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [176+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov qword ptr [-200+rbp], r12
        mov rdi, rdx
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [104+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [112+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [120+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [184+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [176+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-192+rbp], r9
        mov r9, r13
        mov rax, qword ptr [112+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [120+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [128+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [192+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [184+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, r8
        mov rax, rdi
        mov rdx, rsi
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        add r12, rbx
        adc r9, rax
        adc r13, rdx
        mov qword ptr [-184+rbp], r12
        mov rdi, rdx
        mov r8, rax
        mov rsi, rbx
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [120+rcx]
        mul qword ptr [248+rcx]
        mov rsi, rax
        mov r8, rdx
        xor rdi, rdi
        mov rax, qword ptr [128+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [136+rcx]
        mul qword ptr [232+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [224+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [216+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [208+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [200+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [192+rcx]
        add rsi, rax
        adc r8, rdx
        adc rdi, 0
        mov rax, rsi
        mov rbx, r8
        mov rdx, rdi
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        add r9, rax
        adc r12, rbx
        adc r13, rdx
        mov r11, rdx
        mov r8, rax
        mov rdi, rbx
        mov rax, qword ptr [184+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-176+rbp], r9
        mov r9, r13
        mov rax, qword ptr [128+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov r13, r10
        mov rax, qword ptr [136+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [144+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [184+rcx]
        mul qword ptr [192+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r12, r8
        adc r9, rdi
        adc r13, rsi
        add r12, r8
        adc r9, rdi
        adc r13, rsi
        mov qword ptr [-168+rbp], r12
        mov r12, r13
        mov r13, r10
        mov rax, qword ptr [136+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [144+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [152+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [184+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rbx, rdi
        mov rax, rsi
        add r9, r8
        adc r12, rbx
        adc r13, rax
        add r9, r8
        adc r12, rbx
        adc r13, rax
        mov r11, rax
        mov rdi, rbx
        mov rbx, r10
        mov rax, qword ptr [192+rcx]
        mul rax
        add r9, rax
        adc r12, rdx
        adc r13, 0
        mov rsi, r11
        mov qword ptr [-160+rbp], r9
        mov r9, r13
        mov rax, qword ptr [144+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [152+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [160+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [184+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [192+rcx]
        mul qword ptr [200+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add r12, r8
        adc r9, rdi
        adc rbx, rsi
        add r12, r8
        adc r9, rdi
        adc rbx, rsi
        mov qword ptr [-152+rbp], r12
        mov rax, qword ptr [152+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [160+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [168+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [184+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [192+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r10
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc rbx, r13
        adc rdx, r12
        add r9, r8
        adc rbx, r13
        adc rdx, r12
        mov rax, rdx
        mov rdi, r13
        mov rsi, r12
        mov r11, rax
        mov r12, r10
        mov rax, qword ptr [200+rcx]
        mul rax
        add r9, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-144+rbp], r9
        mov r9, r11
        mov rax, qword ptr [160+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [168+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [176+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [184+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [192+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [200+rcx]
        mul qword ptr [208+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r9, rdi
        adc r12, rsi
        add rbx, r8
        adc r9, rdi
        adc r12, rsi
        mov qword ptr [-136+rbp], rbx
        mov r11, r12
        mov rax, qword ptr [168+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [176+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [184+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [192+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [200+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r10
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc r11, r13
        adc rax, r12
        add r9, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov rax, qword ptr [208+rcx]
        mul rax
        add r9, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-128+rbp], r9
        mov r9, r11
        mov rax, qword ptr [176+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [184+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [192+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [200+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [208+rcx]
        mul qword ptr [216+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rdx, r10
        add rbx, r8
        adc r9, rdi
        adc rdx, rsi
        add rbx, r8
        adc r9, rdi
        adc rdx, rsi
        mov qword ptr [-120+rbp], rbx
        mov r11, rdx
        mov rbx, r10
        mov rax, qword ptr [184+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [192+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [200+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [208+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc r11, r13
        adc rbx, r12
        add r9, r8
        adc r11, r13
        adc rbx, r12
        mov rdx, rbx
        mov rdi, r13
        mov rbx, r11
        mov rsi, r12
        mov r11, rdx
        mov r12, r10
        mov rax, qword ptr [216+rcx]
        mul rax
        add r9, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-112+rbp], r9
        mov r9, r11
        mov rax, qword ptr [192+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [200+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [208+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [216+rcx]
        mul qword ptr [224+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        add rbx, r8
        adc r9, rdi
        adc r12, rsi
        add rbx, r8
        adc r9, rdi
        adc r12, rsi
        mov qword ptr [-104+rbp], rbx
        mov r11, r12
        mov rax, qword ptr [200+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [208+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [216+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, r10
        mov r13, rdi
        mov r12, rsi
        add r9, r8
        adc r11, r13
        adc rax, r12
        add r9, r8
        adc r11, r13
        adc rax, r12
        mov rdx, rax
        mov rbx, r11
        mov rdi, r13
        mov r11, rdx
        mov rsi, r12
        mov r12, r10
        mov rax, qword ptr [224+rcx]
        mul rax
        add r9, rax
        adc rbx, rdx
        adc r11, 0
        mov qword ptr [-96+rbp], r9
        mov r9, r10
        mov rax, qword ptr [208+rcx]
        mul qword ptr [248+rcx]
        mov r8, rax
        mov rdi, rdx
        xor rsi, rsi
        mov rax, qword ptr [216+rcx]
        mul qword ptr [240+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov rax, qword ptr [224+rcx]
        mul qword ptr [232+rcx]
        add r8, rax
        adc rdi, rdx
        adc rsi, 0
        mov r13, rdi
        mov rax, rsi
        add rbx, r8
        adc r11, r13
        adc r9, rax
        add rbx, r8
        adc r11, r13
        adc r9, rax
        mov qword ptr [-88+rbp], rbx
        mov rsi, r11
        mov r8, r9
        mov rax, qword ptr [216+rcx]
        mul qword ptr [248+rcx]
        add rsi, rax
        adc r8, rdx
        adc r12, 0
        add rsi, rax
        adc r8, rdx
        adc r12, 0
        mov r11, r12
        mov rax, qword ptr [224+rcx]
        mul qword ptr [240+rcx]
        add rsi, rax
        adc r8, rdx
        adc r11, 0
        add rsi, rax
        adc r8, rdx
        adc r11, 0
        mov r13, r8
        mov rbx, r11
        mov rax, qword ptr [232+rcx]
        mul rax
        add rsi, rax
        adc r13, rdx
        adc rbx, 0
        mov qword ptr [-80+rbp], rsi
        mov r12, rbx
        mov rdi, r13
        mov r13, r10
        mov rax, qword ptr [224+rcx]
        mul qword ptr [248+rcx]
        add rdi, rax
        adc r12, rdx
        adc r13, 0
        add rdi, rax
        adc r12, rdx
        adc r13, 0
        mov r9, r12
        mov r12, r13
        mov rax, qword ptr [232+rcx]
        mul qword ptr [240+rcx]
        add rdi, rax
        adc r9, rdx
        adc r12, 0
        add rdi, rax
        adc r9, rdx
        adc r12, 0
        mov qword ptr [-72+rbp], rdi
        mov r11, r9
        mov rbx, r12
        mov r9, r10
        mov rax, qword ptr [232+rcx]
        mul qword ptr [248+rcx]
        add r11, rax
        adc rbx, rdx
        adc r9, 0
        add r11, rax
        adc rbx, rdx
        adc r9, 0
        mov r13, rbx
        mov rbx, r9
        mov r9, r10
        mov rax, qword ptr [240+rcx]
        mul rax
        add r11, rax
        adc r13, rdx
        adc rbx, 0
        mov qword ptr [-64+rbp], r11
        mov rdi, r13
        mov rsi, rbx
        mov rax, qword ptr [240+rcx]
        mul qword ptr [248+rcx]
        add rdi, rax
        adc rsi, rdx
        adc r9, 0
        add rdi, rax
        adc rsi, rdx
        adc r9, 0
        mov qword ptr [-56+rbp], rdi
        mov r8, r9
        mov rax, qword ptr [248+rcx]
        mul rax
        add rsi, rax
        adc r8, rdx
        adc r10, 0
        mov qword ptr [-48+rbp], rsi
        mov qword ptr [-40+rbp], r8
        mov dword ptr [8+r14], 64
        mov dword ptr [r14], 0
;        mov rdi, qword ptr [16+r14]
;        lea rsi, qword ptr [-544+rbp]
;        mov edx, 512
        mov rcx, qword ptr [16+r14]
        lea rdx, qword ptr [-544+rbp]
        mov r8d, 512
        call memcpy
        mov edx, dword ptr [8+r14]
        test edx, edx
        je L304
        lea ecx, dword ptr [-1+rdx]
        mov rsi, qword ptr [16+r14]
        mov r10d, ecx
        cmp qword ptr [rsi+r10*8], 0
        jne L302
        mov edx, ecx
        ALIGN 16
L303:
        test edx, edx
        mov ecx, edx
        je L307
        dec edx
        mov eax, edx
        cmp qword ptr [rsi+rax*8], 0
        je L303
        mov dword ptr [8+r14], ecx
        mov edx, ecx
L302:
        test edx, edx
        je L304
        mov eax, dword ptr [r14]
        jmp L305

L307:
        mov dword ptr [8+r14], edx
L304:
        xor eax, eax
L305:
        mov dword ptr [r14], eax
        add rsp, 512+32			; +32 for "home" storage
        pop rbx
        pop r12
        pop r13
        pop r14
        pop rbp

        pop rsi
        pop rdi

        ret

s_mp_sqr_comba_32 ENDP

END
