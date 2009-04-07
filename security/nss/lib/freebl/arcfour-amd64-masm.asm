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
; The Original Code is "Marc Bevand's fast AMD64 ARCFOUR source"
;
; The Initial Developer of the Original Code is
; Marc Bevand <bevand_m@epita.fr> .
; Portions created by the Initial Developer are 
; Copyright (C) 2004 the Initial Developer. All Rights Reserved.
;
; Contributor(s): Makoto Kato (m_kato@ga2.so-net.ne.jp)
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
; ***** END LICENSE BLOCK *****

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
