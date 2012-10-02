; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

; ** ARCFOUR implementation optimized for AMD64.
; **
; ** The throughput achieved by this code is about 320 MBytes/sec, on
; ** a 1.8 GHz AMD Opteron (rev C0) processor.

.CODE

; extern void ARCFOUR(RC4Context *cx, unsigned long long inputLen, 
;                     const unsigned char *input, unsigned char *output);


ARCFOUR PROC

        push    rbp
        push    rbx
        push    rsi
        push    rdi

        mov     rbp, rcx                        ; key = ARG(key)
        mov     rbx, rdx                        ; rbx = ARG(len)
        mov     rsi, r8                         ; in = ARG(in)
        mov     rdi, r9                         ; out = ARG(out)
        mov     rcx, [rbp]                      ; x = key->x
        mov     rdx, [rbp+8]                    ; y = key->y
        add     rbp, 16                         ; d = key->data
        inc     rcx                             ; x++
        and     rcx, 0ffh                       ; x &= 0xff
        lea     rbx, [rbx+rsi-8]                ; rbx = in+len-8
        mov     r9, rbx                         ; tmp = in+len-8
        mov     rax, [rbp+rcx*8]                ; tx = d[x]
        cmp     rbx, rsi                        ; cmp in with in+len-8
        jl      Lend                            ; jump if (in+len-8 < in)

Lstart:
        add     rsi, 8                          ; increment in
        add     rdi, 8                          ; increment out

        ;
        ; generate the next 8 bytes of the rc4 stream into r8
        ;

        mov     r11, 8                          ; byte counter

@@:
        add     dl, al                          ; y += tx
        mov     ebx, [rbp+rdx*8]                ; ty = d[y]
        mov     [rbp+rcx*8], ebx                ; d[x] = ty
        add     bl, al                          ; val = ty + tx
        mov     [rbp+rdx*8], eax                ; d[y] = tx
        inc     cl                              ; x++ (NEXT ROUND)
        mov     eax, [rbp+rcx*8]                ; tx = d[x] (NEXT ROUND)
        mov     r8b, [rbp+rbx*8]                ; val = d[val]
        dec     r11b
        ror     r8, 8                           ; (ror does not change ZF)
        jnz     @b

        ;
        ; xor 8 bytes
        ;

        xor     r8, [rsi-8]
        cmp     rsi, r9                         ; cmp in+len-8 with in
        mov     [rdi-8], r8
        jle     Lstart

Lend:
        add     r9, 8                           ; tmp = in+len

        ;
        ; handle the last bytes, one by one
        ;

@@:
        cmp     r9, rsi                         ; cmp in with in+len
        jle     Lfinished                       ; jump if (in+len <= in)
        add     dl, al                          ; y += tx
        mov     ebx, [rbp+rdx*8]                ; ty = d[y]
        mov     [rbp+rcx*8], ebx                ; d[x] = ty
        add     bl, al                          ; val = ty + tx
        mov     [rbp+rdx*8], eax                ; d[y] = tx
        inc     cl                              ; x++ (NEXT ROUND)
        mov     eax, [rbp+rcx*8]                ; tx = d[x] (NEXT ROUND)
        mov     r8b, [rbp+rbx*8]                ; val = d[val]
        xor     r8b, [rsi]                      ; xor 1 byte
        mov     [rdi], r8b
        inc     rsi                             ; in++
        inc     rdi
        jmp     @b

Lfinished:
        dec     rcx                             ; x--
        mov     [rbp-8], dl                     ; key->y = y
        mov     [rbp-16], cl                    ; key->x = x

        pop     rdi
        pop     rsi
        pop     rbx
        pop     rbp
        ret

ARCFOUR ENDP

END
