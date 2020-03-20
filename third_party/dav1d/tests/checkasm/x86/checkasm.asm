; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
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

%define private_prefix checkasm
%include "config.asm"
%include "ext/x86/x86inc.asm"

SECTION_RODATA

error_message: db "failed to preserve register", 0

%if ARCH_X86_64
; just random numbers to reduce the chance of incidental match
ALIGN 16
x6:  dq 0x1a1b2550a612b48c,0x79445c159ce79064
x7:  dq 0x2eed899d5a28ddcd,0x86b2536fcd8cf636
x8:  dq 0xb0856806085e7943,0x3f2bf84fc0fcca4e
x9:  dq 0xacbd382dcf5b8de2,0xd229e1f5b281303f
x10: dq 0x71aeaff20b095fd9,0xab63e2e11fa38ed9
x11: dq 0x89b0c0765892729a,0x77d410d5c42c882d
x12: dq 0xc45ea11a955d8dd5,0x24b3c1d2a024048b
x13: dq 0x2e8ec680de14b47c,0xdd7b8919edd42786
x14: dq 0x135ce6888fa02cbf,0x11e53e2b2ac655ef
x15: dq 0x011ff554472a7a10,0x6de8f4c914c334d5
n7:  dq 0x21f86d66c8ca00ce
n8:  dq 0x75b6ba21077c48ad
n9:  dq 0xed56bb2dcb3c7736
n10: dq 0x8bda43d3fd1a7e06
n11: dq 0xb64a9c9e5d318408
n12: dq 0xdf9a54b303f1d3a3
n13: dq 0x4a75479abd64e097
n14: dq 0x249214109d5d1c88
%endif

SECTION .text

cextern fail_func

; max number of args used by any asm function.
; (max_args % 4) must equal 3 for stack alignment
%define max_args 15

%if ARCH_X86_64

;-----------------------------------------------------------------------------
; int checkasm_stack_clobber(uint64_t clobber, ...)
;-----------------------------------------------------------------------------
cglobal stack_clobber, 1,2
    ; Clobber the stack with junk below the stack pointer
    %define argsize (max_args+6)*8
    SUB  rsp, argsize
    mov   r1, argsize-8
.loop:
    mov [rsp+r1], r0
    sub   r1, 8
    jge .loop
    ADD  rsp, argsize
    RET

%if WIN64
    %assign free_regs 7
    DECLARE_REG_TMP 4
%else
    %assign free_regs 9
    DECLARE_REG_TMP 7
%endif

;-----------------------------------------------------------------------------
; void checkasm_checked_call(void *func, ...)
;-----------------------------------------------------------------------------
INIT_XMM
cglobal checked_call, 2,15,16,max_args*8+8
    mov  t0, r0

    ; All arguments have been pushed on the stack instead of registers in
    ; order to test for incorrect assumptions that 32-bit ints are
    ; zero-extended to 64-bit.
    mov  r0, r6mp
    mov  r1, r7mp
    mov  r2, r8mp
    mov  r3, r9mp
%if UNIX64
    mov  r4, r10mp
    mov  r5, r11mp
    %assign i 6
    %rep max_args-6
        mov  r9, [rsp+stack_offset+(i+1)*8]
        mov  [rsp+(i-6)*8], r9
        %assign i i+1
    %endrep
%else ; WIN64
    %assign i 4
    %rep max_args-4
        mov  r9, [rsp+stack_offset+(i+7)*8]
        mov  [rsp+i*8], r9
        %assign i i+1
    %endrep

    ; Move possible floating-point arguments to the correct registers
    movq m0, r0
    movq m1, r1
    movq m2, r2
    movq m3, r3

    %assign i 6
    %rep 16-6
        mova m %+ i, [x %+ i]
        %assign i i+1
    %endrep
%endif

%assign i 14
%rep 15-free_regs
    mov r %+ i, [n %+ i]
    %assign i i-1
%endrep
    call t0
%assign i 14
%rep 15-free_regs
    xor r %+ i, [n %+ i]
    or  r14, r %+ i
    %assign i i-1
%endrep

%if WIN64
    %assign i 6
    %rep 16-6
        pxor m %+ i, [x %+ i]
        por  m6, m %+ i
        %assign i i+1
    %endrep
    packsswb m6, m6
    movq r5, m6
    or  r14, r5
%endif

    ; Call fail_func() with a descriptive message to mark it as a failure
    ; if the called function didn't preserve all callee-saved registers.
    ; Save the return value located in rdx:rax first to prevent clobbering.
    jz .ok
    mov  r9, rax
    mov r10, rdx
    lea  r0, [error_message]
    xor eax, eax
    call fail_func
    mov rdx, r10
    mov rax, r9
.ok:
    RET

; trigger a warmup of vector units
%macro WARMUP 0
cglobal warmup, 0, 0
    xorps   m0, m0
    mulps   m0, m0
    RET
%endmacro

INIT_YMM avx2
WARMUP
INIT_ZMM avx512
WARMUP

%else

; just random numbers to reduce the chance of incidental match
%define n3 dword 0x6549315c
%define n4 dword 0xe02f3e23
%define n5 dword 0xb78d0d1d
%define n6 dword 0x33627ba7

;-----------------------------------------------------------------------------
; void checkasm_checked_call(void *func, ...)
;-----------------------------------------------------------------------------
cglobal checked_call, 1,7
    mov  r3, n3
    mov  r4, n4
    mov  r5, n5
    mov  r6, n6
%rep max_args
    PUSH dword [esp+20+max_args*4]
%endrep
    call r0
    xor  r3, n3
    xor  r4, n4
    xor  r5, n5
    xor  r6, n6
    or   r3, r4
    or   r5, r6
    or   r3, r5
    jz .ok
    mov  r3, eax
    mov  r4, edx
    LEA  r0, error_message
    mov [esp], r0
    call fail_func
    mov  edx, r4
    mov  eax, r3
.ok:
    add  esp, max_args*4
    RET

%endif ; ARCH_X86_64
