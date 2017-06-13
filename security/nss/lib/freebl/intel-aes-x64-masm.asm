; LICENSE:
; This submission to NSS is to be made available under the terms of the
; Mozilla Public License, v. 2.0. You can obtain one at http:
; //mozilla.org/MPL/2.0/.
;###############################################################################
; Copyright(c) 2014, Intel Corp.
; Developers and authors:
; Shay Gueron and Vlad Krasnov
; Intel Corporation, Israel Development Centre, Haifa, Israel
; Please send feedback directly to crypto.feedback.alias@intel.com


.DATA
ALIGN 16
Lmask dd 0c0f0e0dh,0c0f0e0dh,0c0f0e0dh,0c0f0e0dh
Lmask192 dd 004070605h, 004070605h, 004070605h, 004070605h
Lmask256 dd 00c0f0e0dh, 00c0f0e0dh, 00c0f0e0dh, 00c0f0e0dh
Lcon1 dd 1,1,1,1
Lcon2 dd 1bh,1bh,1bh,1bh

.CODE

ctx     textequ <rcx>
output  textequ <rdx>
input   textequ <r8>
inputLen textequ <r9d>


aes_rnd MACRO i
    movdqu  xmm8, [i*16 + ctx]
    aesenc  xmm0, xmm8
    aesenc  xmm1, xmm8
    aesenc  xmm2, xmm8
    aesenc  xmm3, xmm8
    aesenc  xmm4, xmm8
    aesenc  xmm5, xmm8
    aesenc  xmm6, xmm8
    aesenc  xmm7, xmm8
    ENDM

aes_last_rnd MACRO i
    movdqu  xmm8, [i*16 + ctx]
    aesenclast  xmm0, xmm8
    aesenclast  xmm1, xmm8
    aesenclast  xmm2, xmm8
    aesenclast  xmm3, xmm8
    aesenclast  xmm4, xmm8
    aesenclast  xmm5, xmm8
    aesenclast  xmm6, xmm8
    aesenclast  xmm7, xmm8
    ENDM

aes_dec_rnd MACRO i
    movdqu  xmm8, [i*16 + ctx]
    aesdec  xmm0, xmm8
    aesdec  xmm1, xmm8
    aesdec  xmm2, xmm8
    aesdec  xmm3, xmm8
    aesdec  xmm4, xmm8
    aesdec  xmm5, xmm8
    aesdec  xmm6, xmm8
    aesdec  xmm7, xmm8
    ENDM

aes_dec_last_rnd MACRO i
    movdqu  xmm8, [i*16 + ctx]
    aesdeclast  xmm0, xmm8
    aesdeclast  xmm1, xmm8
    aesdeclast  xmm2, xmm8
    aesdeclast  xmm3, xmm8
    aesdeclast  xmm4, xmm8
    aesdeclast  xmm5, xmm8
    aesdeclast  xmm6, xmm8
    aesdeclast  xmm7, xmm8
    ENDM


gen_aes_ecb_func MACRO enc, rnds

LOCAL   loop8
LOCAL   loop1
LOCAL   bail

        xor     inputLen, inputLen
        mov     input,      [rsp + 1*8 + 8*4]
        mov     inputLen,   [rsp + 1*8 + 8*5]

        sub     rsp, 3*16

        movdqu  [rsp + 0*16], xmm6
        movdqu  [rsp + 1*16], xmm7
        movdqu  [rsp + 2*16], xmm8

loop8:
        cmp     inputLen, 8*16
        jb      loop1

        movdqu  xmm0, [0*16 + input]
        movdqu  xmm1, [1*16 + input]
        movdqu  xmm2, [2*16 + input]
        movdqu  xmm3, [3*16 + input]
        movdqu  xmm4, [4*16 + input]
        movdqu  xmm5, [5*16 + input]
        movdqu  xmm6, [6*16 + input]
        movdqu  xmm7, [7*16 + input]

        movdqu  xmm8, [0*16 + ctx]
        pxor    xmm0, xmm8
        pxor    xmm1, xmm8
        pxor    xmm2, xmm8
        pxor    xmm3, xmm8
        pxor    xmm4, xmm8
        pxor    xmm5, xmm8
        pxor    xmm6, xmm8
        pxor    xmm7, xmm8

IF enc eq 1
        rnd textequ <aes_rnd>
        lastrnd textequ <aes_last_rnd>
        aesinst textequ <aesenc>
        aeslastinst textequ <aesenclast>
ELSE
        rnd textequ <aes_dec_rnd>
        lastrnd textequ <aes_dec_last_rnd>
        aesinst textequ <aesdec>
        aeslastinst textequ <aesdeclast>
ENDIF

        i = 1
        WHILE i LT rnds
            rnd i
            i = i+1
            ENDM
        lastrnd rnds

        movdqu  [0*16 + output], xmm0
        movdqu  [1*16 + output], xmm1
        movdqu  [2*16 + output], xmm2
        movdqu  [3*16 + output], xmm3
        movdqu  [4*16 + output], xmm4
        movdqu  [5*16 + output], xmm5
        movdqu  [6*16 + output], xmm6
        movdqu  [7*16 + output], xmm7

        lea input, [8*16 + input]
        lea output, [8*16 + output]
        sub inputLen, 8*16
        jmp loop8

loop1:
        cmp     inputLen, 1*16
        jb      bail

        movdqu  xmm0, [input]
        movdqu  xmm7, [0*16 + ctx]
        pxor    xmm0, xmm7

        i = 1
    WHILE i LT rnds
            movdqu  xmm7, [i*16 + ctx]
            aesinst  xmm0, xmm7
            i = i+1
        ENDM
        movdqu  xmm7, [rnds*16 + ctx]
        aeslastinst xmm0, xmm7

        movdqu  [output], xmm0

        lea input, [1*16 + input]
        lea output, [1*16 + output]
        sub inputLen, 1*16
        jmp loop1

bail:
        xor rax, rax

        movdqu  xmm6, [rsp + 0*16]
        movdqu  xmm7, [rsp + 1*16]
        movdqu  xmm8, [rsp + 2*16]
        add     rsp, 3*16
        ret
ENDM

intel_aes_encrypt_ecb_128 PROC
gen_aes_ecb_func 1, 10
intel_aes_encrypt_ecb_128 ENDP

intel_aes_encrypt_ecb_192 PROC
gen_aes_ecb_func 1, 12
intel_aes_encrypt_ecb_192 ENDP

intel_aes_encrypt_ecb_256 PROC
gen_aes_ecb_func 1, 14
intel_aes_encrypt_ecb_256 ENDP

intel_aes_decrypt_ecb_128 PROC
gen_aes_ecb_func 0, 10
intel_aes_decrypt_ecb_128 ENDP

intel_aes_decrypt_ecb_192 PROC
gen_aes_ecb_func 0, 12
intel_aes_decrypt_ecb_192 ENDP

intel_aes_decrypt_ecb_256 PROC
gen_aes_ecb_func 0, 14
intel_aes_decrypt_ecb_256 ENDP


KEY textequ <rcx>
KS  textequ <rdx>
ITR textequ <r8>

intel_aes_encrypt_init_128  PROC

    movdqu  xmm1, [KEY]
    movdqu  [KS], xmm1
    movdqa  xmm2, xmm1

    lea ITR, Lcon1
    movdqa  xmm0, [ITR]
    lea ITR, Lmask
    movdqa  xmm4, [ITR]

    mov ITR, 8

Lenc_128_ks_loop:
        lea KS, [16 + KS]
        dec ITR

        pshufb  xmm2, xmm4
        aesenclast  xmm2, xmm0
        pslld   xmm0, 1
        movdqa  xmm3, xmm1
        pslldq  xmm3, 4
        pxor    xmm1, xmm3
        pslldq  xmm3, 4
        pxor    xmm1, xmm3
        pslldq  xmm3, 4
        pxor    xmm1, xmm3
        pxor    xmm1, xmm2
        movdqu  [KS], xmm1
        movdqa  xmm2, xmm1

        jne Lenc_128_ks_loop

    lea ITR, Lcon2
    movdqa  xmm0, [ITR]

    pshufb  xmm2, xmm4
    aesenclast  xmm2, xmm0
    pslld   xmm0, 1
    movdqa  xmm3, xmm1
    pslldq  xmm3, 4
    pxor    xmm1, xmm3
    pslldq  xmm3, 4
    pxor    xmm1, xmm3
    pslldq  xmm3, 4
    pxor    xmm1, xmm3
    pxor    xmm1, xmm2
    movdqu  [16 + KS], xmm1
    movdqa  xmm2, xmm1

    pshufb  xmm2, xmm4
    aesenclast  xmm2, xmm0
    movdqa  xmm3, xmm1
    pslldq  xmm3, 4
    pxor    xmm1, xmm3
    pslldq  xmm3, 4
    pxor    xmm1, xmm3
    pslldq  xmm3, 4
    pxor    xmm1, xmm3
    pxor    xmm1, xmm2
    movdqu  [32 + KS], xmm1
    movdqa  xmm2, xmm1

    ret
intel_aes_encrypt_init_128  ENDP


intel_aes_decrypt_init_128  PROC

    push    KS
    push    KEY

    call    intel_aes_encrypt_init_128

    pop     KEY
    pop     KS

    movdqu  xmm0, [0*16 + KS]
    movdqu  xmm1, [10*16 + KS]
    movdqu  [10*16 + KS], xmm0
    movdqu  [0*16 + KS], xmm1

    i = 1
    WHILE i LT 5
        movdqu  xmm0, [i*16 + KS]
        movdqu  xmm1, [(10-i)*16 + KS]

        aesimc  xmm0, xmm0
        aesimc  xmm1, xmm1

        movdqu  [(10-i)*16 + KS], xmm0
        movdqu  [i*16 + KS], xmm1

        i = i+1
    ENDM

    movdqu  xmm0, [5*16 + KS]
    aesimc  xmm0, xmm0
    movdqu  [5*16 + KS], xmm0
    ret
intel_aes_decrypt_init_128  ENDP


intel_aes_encrypt_init_192  PROC

    sub     rsp, 16*2
    movdqu  [16*0 + rsp], xmm6
    movdqu  [16*1 + rsp], xmm7

    movdqu  xmm1, [KEY]
    mov     ITR, [16 + KEY]
    movd    xmm3, ITR

    movdqu  [KS], xmm1
    movdqa  xmm5, xmm3

    lea ITR, Lcon1
    movdqu  xmm0, [ITR]
    lea ITR, Lmask192
    movdqu  xmm4, [ITR]

    mov ITR, 4

Lenc_192_ks_loop:
        movdqa  xmm2, xmm3
        pshufb  xmm2, xmm4
        aesenclast xmm2, xmm0
        pslld   xmm0, 1

        movdqa  xmm6, xmm1
        movdqa  xmm7, xmm3
        pslldq  xmm6, 4
        pslldq  xmm7, 4
        pxor    xmm1, xmm6
        pxor    xmm3, xmm7
        pslldq  xmm6, 4
        pxor    xmm1, xmm6
        pslldq  xmm6, 4
        pxor    xmm1, xmm6
        pxor    xmm1, xmm2
        pshufd  xmm2, xmm1, 0ffh
        pxor    xmm3, xmm2

        movdqa  xmm6, xmm1
        shufpd  xmm5, xmm1, 00h
        shufpd  xmm6, xmm3, 01h

        movdqu  [16 + KS], xmm5
        movdqu  [32 + KS], xmm6

        movdqa  xmm2, xmm3
        pshufb  xmm2, xmm4
        aesenclast  xmm2, xmm0
        pslld   xmm0, 1

        movdqa  xmm6, xmm1
        movdqa  xmm7, xmm3
        pslldq  xmm6, 4
        pslldq  xmm7, 4
        pxor    xmm1, xmm6
        pxor    xmm3, xmm7
        pslldq  xmm6, 4
        pxor    xmm1, xmm6
        pslldq  xmm6, 4
        pxor    xmm1, xmm6
        pxor    xmm1, xmm2
        pshufd  xmm2, xmm1, 0ffh
        pxor    xmm3, xmm2

        movdqu  [48 + KS], xmm1
        movdqa  xmm5, xmm3

        lea KS, [48 + KS]

        dec ITR
        jnz Lenc_192_ks_loop

    movdqu  [16 + KS], xmm5

    movdqu  xmm7, [16*1 + rsp]
    movdqu  xmm6, [16*0 + rsp]
    add rsp, 16*2
    ret
intel_aes_encrypt_init_192  ENDP

intel_aes_decrypt_init_192  PROC
    push    KS
    push    KEY

    call    intel_aes_encrypt_init_192

    pop     KEY
    pop     KS

    movdqu  xmm0, [0*16 + KS]
    movdqu  xmm1, [12*16 + KS]
    movdqu  [12*16 + KS], xmm0
    movdqu  [0*16 + KS], xmm1

    i = 1
    WHILE i LT 6
        movdqu  xmm0, [i*16 + KS]
        movdqu  xmm1, [(12-i)*16 + KS]

        aesimc  xmm0, xmm0
        aesimc  xmm1, xmm1

        movdqu  [(12-i)*16 + KS], xmm0
        movdqu  [i*16 + KS], xmm1

        i = i+1
    ENDM

    movdqu  xmm0, [6*16 + KS]
    aesimc  xmm0, xmm0
    movdqu  [6*16 + KS], xmm0
    ret
intel_aes_decrypt_init_192  ENDP


intel_aes_encrypt_init_256  PROC
    sub     rsp, 16*2
    movdqu  [16*0 + rsp], xmm6
    movdqu  [16*1 + rsp], xmm7

    movdqu  xmm1, [16*0 + KEY]
    movdqu  xmm3, [16*1 + KEY]

    movdqu  [16*0 + KS], xmm1
    movdqu  [16*1 + KS], xmm3

    lea ITR, Lcon1
    movdqu  xmm0, [ITR]
    lea ITR, Lmask256
    movdqu  xmm5, [ITR]

    pxor    xmm6, xmm6

    mov ITR, 6

Lenc_256_ks_loop:

        movdqa  xmm2, xmm3
        pshufb  xmm2, xmm5
        aesenclast  xmm2, xmm0
        pslld   xmm0, 1
        movdqa  xmm4, xmm1
        pslldq  xmm4, 4
        pxor    xmm1, xmm4
        pslldq  xmm4, 4
        pxor    xmm1, xmm4
        pslldq  xmm4, 4
        pxor    xmm1, xmm4
        pxor    xmm1, xmm2
        movdqu  [16*2 + KS], xmm1

        pshufd  xmm2, xmm1, 0ffh
        aesenclast  xmm2, xmm6
        movdqa  xmm4, xmm3
        pslldq  xmm4, 4
        pxor    xmm3, xmm4
        pslldq  xmm4, 4
        pxor    xmm3, xmm4
        pslldq  xmm4, 4
        pxor    xmm3, xmm4
        pxor    xmm3, xmm2
        movdqu  [16*3 + KS], xmm3

        lea KS, [32 + KS]
        dec ITR
        jnz Lenc_256_ks_loop

    movdqa  xmm2, xmm3
    pshufb  xmm2, xmm5
    aesenclast  xmm2, xmm0
    movdqa  xmm4, xmm1
    pslldq  xmm4, 4
    pxor    xmm1, xmm4
    pslldq  xmm4, 4
    pxor    xmm1, xmm4
    pslldq  xmm4, 4
    pxor    xmm1, xmm4
    pxor    xmm1, xmm2
    movdqu  [16*2 + KS], xmm1

    movdqu  xmm7, [16*1 + rsp]
    movdqu  xmm6, [16*0 + rsp]
    add rsp, 16*2
    ret

intel_aes_encrypt_init_256  ENDP


intel_aes_decrypt_init_256  PROC
    push    KS
    push    KEY

    call    intel_aes_encrypt_init_256

    pop     KEY
    pop     KS

    movdqu  xmm0, [0*16 + KS]
    movdqu  xmm1, [14*16 + KS]
    movdqu  [14*16 + KS], xmm0
    movdqu  [0*16 + KS], xmm1

    i = 1
    WHILE i LT 7
        movdqu  xmm0, [i*16 + KS]
        movdqu  xmm1, [(14-i)*16 + KS]

        aesimc  xmm0, xmm0
        aesimc  xmm1, xmm1

        movdqu  [(14-i)*16 + KS], xmm0
        movdqu  [i*16 + KS], xmm1

        i = i+1
    ENDM

    movdqu  xmm0, [7*16 + KS]
    aesimc  xmm0, xmm0
    movdqu  [7*16 + KS], xmm0
    ret
intel_aes_decrypt_init_256  ENDP



gen_aes_cbc_enc_func MACRO rnds

LOCAL   loop1
LOCAL   bail

        mov     input,      [rsp + 1*8 + 8*4]
        mov     inputLen,   [rsp + 1*8 + 8*5]

        sub     rsp, 3*16

        movdqu  [rsp + 0*16], xmm6
        movdqu  [rsp + 1*16], xmm7
        movdqu  [rsp + 2*16], xmm8

        movdqu  xmm0, [256+ctx]

        movdqu  xmm2, [0*16 + ctx]
        movdqu  xmm3, [1*16 + ctx]
        movdqu  xmm4, [2*16 + ctx]
        movdqu  xmm5, [3*16 + ctx]
        movdqu  xmm6, [4*16 + ctx]
        movdqu  xmm7, [5*16 + ctx]

loop1:
        cmp     inputLen, 1*16
        jb      bail

        movdqu  xmm1, [input]
        pxor    xmm1, xmm2
        pxor    xmm0, xmm1

        aesenc  xmm0, xmm3
        aesenc  xmm0, xmm4
        aesenc  xmm0, xmm5
        aesenc  xmm0, xmm6
        aesenc  xmm0, xmm7

        i = 6
    WHILE i LT rnds
            movdqu  xmm8, [i*16 + ctx]
            aesenc  xmm0, xmm8
            i = i+1
        ENDM
        movdqu  xmm8, [rnds*16 + ctx]
        aesenclast xmm0, xmm8

        movdqu  [output], xmm0

        lea input, [1*16 + input]
        lea output, [1*16 + output]
        sub inputLen, 1*16
        jmp loop1

bail:
        movdqu  [256+ctx], xmm0

        xor rax, rax

        movdqu  xmm6, [rsp + 0*16]
        movdqu  xmm7, [rsp + 1*16]
        movdqu  xmm8, [rsp + 2*16]
        add     rsp, 3*16
        ret

ENDM

gen_aes_cbc_dec_func MACRO rnds

LOCAL   loop8
LOCAL   loop1
LOCAL   dec1
LOCAL   bail

        mov     input,      [rsp + 1*8 + 8*4]
        mov     inputLen,   [rsp + 1*8 + 8*5]

        sub     rsp, 3*16

        movdqu  [rsp + 0*16], xmm6
        movdqu  [rsp + 1*16], xmm7
        movdqu  [rsp + 2*16], xmm8

loop8:
        cmp     inputLen, 8*16
        jb      dec1

        movdqu  xmm0, [0*16 + input]
        movdqu  xmm1, [1*16 + input]
        movdqu  xmm2, [2*16 + input]
        movdqu  xmm3, [3*16 + input]
        movdqu  xmm4, [4*16 + input]
        movdqu  xmm5, [5*16 + input]
        movdqu  xmm6, [6*16 + input]
        movdqu  xmm7, [7*16 + input]

        movdqu  xmm8, [0*16 + ctx]
        pxor    xmm0, xmm8
        pxor    xmm1, xmm8
        pxor    xmm2, xmm8
        pxor    xmm3, xmm8
        pxor    xmm4, xmm8
        pxor    xmm5, xmm8
        pxor    xmm6, xmm8
        pxor    xmm7, xmm8

        i = 1
        WHILE i LT rnds
            aes_dec_rnd i
            i = i+1
            ENDM
        aes_dec_last_rnd rnds

        movdqu  xmm8, [256 + ctx]
        pxor    xmm0, xmm8
        movdqu  xmm8, [0*16 + input]
        pxor    xmm1, xmm8
        movdqu  xmm8, [1*16 + input]
        pxor    xmm2, xmm8
        movdqu  xmm8, [2*16 + input]
        pxor    xmm3, xmm8
        movdqu  xmm8, [3*16 + input]
        pxor    xmm4, xmm8
        movdqu  xmm8, [4*16 + input]
        pxor    xmm5, xmm8
        movdqu  xmm8, [5*16 + input]
        pxor    xmm6, xmm8
        movdqu  xmm8, [6*16 + input]
        pxor    xmm7, xmm8
        movdqu  xmm8, [7*16 + input]

        movdqu  [0*16 + output], xmm0
        movdqu  [1*16 + output], xmm1
        movdqu  [2*16 + output], xmm2
        movdqu  [3*16 + output], xmm3
        movdqu  [4*16 + output], xmm4
        movdqu  [5*16 + output], xmm5
        movdqu  [6*16 + output], xmm6
        movdqu  [7*16 + output], xmm7
        movdqu  [256 + ctx], xmm8

        lea input, [8*16 + input]
        lea output, [8*16 + output]
        sub inputLen, 8*16
        jmp loop8
dec1:

        movdqu  xmm3, [256 + ctx]

loop1:
        cmp     inputLen, 1*16
        jb      bail

        movdqu  xmm0, [input]
        movdqa  xmm4, xmm0
        movdqu  xmm7, [0*16 + ctx]
        pxor    xmm0, xmm7

        i = 1
    WHILE i LT rnds
            movdqu  xmm7, [i*16 + ctx]
            aesdec  xmm0, xmm7
            i = i+1
        ENDM
        movdqu  xmm7, [rnds*16 + ctx]
        aesdeclast xmm0, xmm7
        pxor    xmm3, xmm0

        movdqu  [output], xmm3
        movdqa  xmm3, xmm4

        lea input, [1*16 + input]
        lea output, [1*16 + output]
        sub inputLen, 1*16
        jmp loop1

bail:
        movdqu  [256 + ctx], xmm3
        xor rax, rax

        movdqu  xmm6, [rsp + 0*16]
        movdqu  xmm7, [rsp + 1*16]
        movdqu  xmm8, [rsp + 2*16]
        add     rsp, 3*16
        ret
ENDM

intel_aes_encrypt_cbc_128 PROC
gen_aes_cbc_enc_func  10
intel_aes_encrypt_cbc_128 ENDP

intel_aes_encrypt_cbc_192 PROC
gen_aes_cbc_enc_func  12
intel_aes_encrypt_cbc_192 ENDP

intel_aes_encrypt_cbc_256 PROC
gen_aes_cbc_enc_func  14
intel_aes_encrypt_cbc_256 ENDP

intel_aes_decrypt_cbc_128 PROC
gen_aes_cbc_dec_func  10
intel_aes_decrypt_cbc_128 ENDP

intel_aes_decrypt_cbc_192 PROC
gen_aes_cbc_dec_func  12
intel_aes_decrypt_cbc_192 ENDP

intel_aes_decrypt_cbc_256 PROC
gen_aes_cbc_dec_func  14
intel_aes_decrypt_cbc_256 ENDP



ctrCtx textequ <r10>
CTR textequ <r11d>
CTRSave textequ <eax>

gen_aes_ctr_func MACRO rnds

LOCAL   loop8
LOCAL   loop1
LOCAL   enc1
LOCAL   bail

        mov     input,      [rsp + 8*1 + 4*8]
        mov     inputLen,   [rsp + 8*1 + 5*8]

        mov     ctrCtx, ctx
        mov     ctx, [8+ctrCtx]

        sub     rsp, 3*16
        movdqu  [rsp + 0*16], xmm6
        movdqu  [rsp + 1*16], xmm7
        movdqu  [rsp + 2*16], xmm8


        push    rbp
        mov     rbp, rsp
        sub     rsp, 8*16
        and     rsp, -16


        movdqu  xmm0, [16+ctrCtx]
        mov     CTRSave, DWORD PTR [ctrCtx + 16 + 3*4]
        bswap   CTRSave
        movdqu  xmm1, [ctx + 0*16]

        pxor    xmm0, xmm1

        movdqa  [rsp + 0*16], xmm0
        movdqa  [rsp + 1*16], xmm0
        movdqa  [rsp + 2*16], xmm0
        movdqa  [rsp + 3*16], xmm0
        movdqa  [rsp + 4*16], xmm0
        movdqa  [rsp + 5*16], xmm0
        movdqa  [rsp + 6*16], xmm0
        movdqa  [rsp + 7*16], xmm0

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 1*16 + 3*4], CTR

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 2*16 + 3*4], CTR

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 3*16 + 3*4], CTR

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 4*16 + 3*4], CTR

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 5*16 + 3*4], CTR

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 6*16 + 3*4], CTR

        inc     CTRSave
        mov     CTR, CTRSave
        bswap   CTR
        xor     CTR, DWORD PTR [ctx + 3*4]
        mov     DWORD PTR [rsp + 7*16 + 3*4], CTR


loop8:
        cmp     inputLen, 8*16
        jb      loop1

        movdqu  xmm0, [0*16 + rsp]
        movdqu  xmm1, [1*16 + rsp]
        movdqu  xmm2, [2*16 + rsp]
        movdqu  xmm3, [3*16 + rsp]
        movdqu  xmm4, [4*16 + rsp]
        movdqu  xmm5, [5*16 + rsp]
        movdqu  xmm6, [6*16 + rsp]
        movdqu  xmm7, [7*16 + rsp]

        i = 1
        WHILE i LE 8
            aes_rnd i

            inc     CTRSave
            mov     CTR, CTRSave
            bswap   CTR
            xor     CTR, DWORD PTR [ctx + 3*4]
            mov     DWORD PTR [rsp + (i-1)*16 + 3*4], CTR

            i = i+1
        ENDM
        WHILE i LT rnds
            aes_rnd i
            i = i+1
            ENDM
        aes_last_rnd rnds

        movdqu  xmm8, [0*16 + input]
        pxor    xmm0, xmm8
        movdqu  xmm8, [1*16 + input]
        pxor    xmm1, xmm8
        movdqu  xmm8, [2*16 + input]
        pxor    xmm2, xmm8
        movdqu  xmm8, [3*16 + input]
        pxor    xmm3, xmm8
        movdqu  xmm8, [4*16 + input]
        pxor    xmm4, xmm8
        movdqu  xmm8, [5*16 + input]
        pxor    xmm5, xmm8
        movdqu  xmm8, [6*16 + input]
        pxor    xmm6, xmm8
        movdqu  xmm8, [7*16 + input]
        pxor    xmm7, xmm8

        movdqu  [0*16 + output], xmm0
        movdqu  [1*16 + output], xmm1
        movdqu  [2*16 + output], xmm2
        movdqu  [3*16 + output], xmm3
        movdqu  [4*16 + output], xmm4
        movdqu  [5*16 + output], xmm5
        movdqu  [6*16 + output], xmm6
        movdqu  [7*16 + output], xmm7

        lea input, [8*16 + input]
        lea output, [8*16 + output]
        sub inputLen, 8*16
        jmp loop8


loop1:
        cmp     inputLen, 1*16
        jb      bail

        movdqu  xmm0, [rsp]
        add     rsp, 16

        i = 1
    WHILE i LT rnds
            movdqu  xmm7, [i*16 + ctx]
            aesenc  xmm0, xmm7
            i = i+1
        ENDM
        movdqu  xmm7, [rnds*16 + ctx]
        aesenclast xmm0, xmm7

        movdqu  xmm7, [input]
        pxor    xmm0, xmm7
        movdqu  [output], xmm0

        lea input, [1*16 + input]
        lea output, [1*16 + output]
        sub inputLen, 1*16
        jmp loop1

bail:

        movdqu  xmm0, [rsp]
        movdqu  xmm1, [ctx + 0*16]
        pxor    xmm0, xmm1
        movdqu  [16+ctrCtx], xmm0


        xor     rax, rax
        mov     rsp, rbp
        pop     rbp

        movdqu  xmm6, [rsp + 0*16]
        movdqu  xmm7, [rsp + 1*16]
        movdqu  xmm8, [rsp + 2*16]
        add     rsp, 3*16

        ret
ENDM


intel_aes_encrypt_ctr_128 PROC
gen_aes_ctr_func  10
intel_aes_encrypt_ctr_128 ENDP

intel_aes_encrypt_ctr_192 PROC
gen_aes_ctr_func  12
intel_aes_encrypt_ctr_192 ENDP

intel_aes_encrypt_ctr_256 PROC
gen_aes_ctr_func  14
intel_aes_encrypt_ctr_256 ENDP


END
