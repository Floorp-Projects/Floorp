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
; The Original Code is mozilla.org code.
;
; The Initial Developer of the Original Code is 
; Makoto Kato <m_kato@ga2.so-net.ne.jp>.
; Portions created by the Initial Developer are Copyright (C) 2004
; the Initial Developer. All Rights Reserved.
;
; Contributor(s):
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

extrn PrepareAndDispatch:PROC

.code

SharedStub PROC

   ;
   ; store 4 parameters to stack
   ;

    mov     [rsp+40], r9
    mov     [rsp+32], r8
    mov     [rsp+24], rdx
    mov     [rsp+16], rcx

    sub     rsp, 112

   ;
   ; fist 4 parameters (1st is "this" pointer) are passed in registers.
   ;

   ; for floating value

    movsd   [rsp+64], xmm1
    movsd   [rsp+72], xmm2
    movsd   [rsp+80], xmm3

   ; for integer value

    mov     [rsp+40], rdx
    mov     [rsp+48], r8
    mov     [rsp+56], r9

    ;
    ; Call PrepareAndDispatch function
    ;

    ; 5th parameter (floating parameters) of PrepareAndDispatch

    lea     r9, [rsp+64]
    mov     [rsp+32], r9

    ; 4th parameter (normal parameters) of PrepareAndDispatch

    lea     r9, [rsp+40]

    ; 3rd parameter (pointer to args on stack)

    lea     r8, [rsp+48+112]

    ; 2nd parameter (vtbl_index)

    mov     rdx, rbx

    ; 1st parameter (this) (not need re-assign)

    ;mov     rcx, [rsp+16+112]

    call    PrepareAndDispatch

    ;
    ; clean up register
    ;

    add     rsp, 112+16

    ; set return address

    mov     rdx, [rsp+8-16]

    ; restore rbx

    mov     rbx, [rsp-16]

    ; simulate __stdcall return

    jmp     rdx

SharedStub ENDP


?Stub3@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 3
    jmp     SharedStub
?Stub3@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub4@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 4
    jmp     SharedStub
?Stub4@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub5@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 5
    jmp     SharedStub
?Stub5@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub6@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 6
    jmp     SharedStub
?Stub6@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub7@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 7
    jmp     SharedStub
?Stub7@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub8@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 8
    jmp     SharedStub
?Stub8@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub9@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 9
    jmp     SharedStub
?Stub9@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub10@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 10
    jmp     SharedStub
?Stub10@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub11@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 11
    jmp     SharedStub
?Stub11@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub12@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 12
    jmp     SharedStub
?Stub12@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub13@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 13
    jmp     SharedStub
?Stub13@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub14@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 14
    jmp     SharedStub
?Stub14@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub15@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 15
    jmp     SharedStub
?Stub15@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub16@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 16
    jmp     SharedStub
?Stub16@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub17@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 17
    jmp     SharedStub
?Stub17@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub18@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 18
    jmp     SharedStub
?Stub18@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub19@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 19
    jmp     SharedStub
?Stub19@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub20@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 20
    jmp     SharedStub
?Stub20@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub21@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 21
    jmp     SharedStub
?Stub21@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub22@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 22
    jmp     SharedStub
?Stub22@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub23@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 23
    jmp     SharedStub
?Stub23@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub24@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 24
    jmp     SharedStub
?Stub24@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub25@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 25
    jmp     SharedStub
?Stub25@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub26@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 26
    jmp     SharedStub
?Stub26@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub27@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 27
    jmp     SharedStub
?Stub27@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub28@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 28
    jmp     SharedStub
?Stub28@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub29@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 29
    jmp     SharedStub
?Stub29@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub30@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 30
    jmp     SharedStub
?Stub30@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub31@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 31
    jmp     SharedStub
?Stub31@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub32@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 32
    jmp     SharedStub
?Stub32@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub33@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 33
    jmp     SharedStub
?Stub33@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub34@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 34
    jmp     SharedStub
?Stub34@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub35@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 35
    jmp     SharedStub
?Stub35@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub36@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 36
    jmp     SharedStub
?Stub36@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub37@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 37
    jmp     SharedStub
?Stub37@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub38@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 38
    jmp     SharedStub
?Stub38@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub39@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 39
    jmp     SharedStub
?Stub39@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub40@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 40
    jmp     SharedStub
?Stub40@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub41@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 41
    jmp     SharedStub
?Stub41@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub42@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 42
    jmp     SharedStub
?Stub42@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub43@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 43
    jmp     SharedStub
?Stub43@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub44@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 44
    jmp     SharedStub
?Stub44@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub45@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 45
    jmp     SharedStub
?Stub45@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub46@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 46
    jmp     SharedStub
?Stub46@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub47@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 47
    jmp     SharedStub
?Stub47@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub48@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 48
    jmp     SharedStub
?Stub48@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub49@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 49
    jmp     SharedStub
?Stub49@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub50@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 50
    jmp     SharedStub
?Stub50@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub51@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 51
    jmp     SharedStub
?Stub51@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub52@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 52
    jmp     SharedStub
?Stub52@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub53@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 53
    jmp     SharedStub
?Stub53@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub54@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 54
    jmp     SharedStub
?Stub54@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub55@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 55
    jmp     SharedStub
?Stub55@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub56@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 56
    jmp     SharedStub
?Stub56@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub57@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 57
    jmp     SharedStub
?Stub57@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub58@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 58
    jmp     SharedStub
?Stub58@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub59@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 59
    jmp     SharedStub
?Stub59@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub60@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 60
    jmp     SharedStub
?Stub60@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub61@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 61
    jmp     SharedStub
?Stub61@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub62@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 62
    jmp     SharedStub
?Stub62@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub63@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 63
    jmp     SharedStub
?Stub63@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub64@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 64
    jmp     SharedStub
?Stub64@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub65@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 65
    jmp     SharedStub
?Stub65@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub66@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 66
    jmp     SharedStub
?Stub66@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub67@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 67
    jmp     SharedStub
?Stub67@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub68@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 68
    jmp     SharedStub
?Stub68@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub69@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 69
    jmp     SharedStub
?Stub69@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub70@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 70
    jmp     SharedStub
?Stub70@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub71@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 71
    jmp     SharedStub
?Stub71@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub72@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 72
    jmp     SharedStub
?Stub72@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub73@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 73
    jmp     SharedStub
?Stub73@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub74@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 74
    jmp     SharedStub
?Stub74@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub75@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 75
    jmp     SharedStub
?Stub75@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub76@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 76
    jmp     SharedStub
?Stub76@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub77@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 77
    jmp     SharedStub
?Stub77@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub78@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 78
    jmp     SharedStub
?Stub78@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub79@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 79
    jmp     SharedStub
?Stub79@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub80@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 80
    jmp     SharedStub
?Stub80@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub81@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 81
    jmp     SharedStub
?Stub81@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub82@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 82
    jmp     SharedStub
?Stub82@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub83@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 83
    jmp     SharedStub
?Stub83@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub84@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 84
    jmp     SharedStub
?Stub84@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub85@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 85
    jmp     SharedStub
?Stub85@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub86@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 86
    jmp     SharedStub
?Stub86@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub87@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 87
    jmp     SharedStub
?Stub87@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub88@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 88
    jmp     SharedStub
?Stub88@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub89@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 89
    jmp     SharedStub
?Stub89@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub90@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 90
    jmp     SharedStub
?Stub90@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub91@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 91
    jmp     SharedStub
?Stub91@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub92@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 92
    jmp     SharedStub
?Stub92@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub93@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 93
    jmp     SharedStub
?Stub93@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub94@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 94
    jmp     SharedStub
?Stub94@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub95@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 95
    jmp     SharedStub
?Stub95@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub96@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 96
    jmp     SharedStub
?Stub96@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub97@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 97
    jmp     SharedStub
?Stub97@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub98@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 98
    jmp     SharedStub
?Stub98@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub99@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 99
    jmp     SharedStub
?Stub99@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub100@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 100
    jmp     SharedStub
?Stub100@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub101@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 101
    jmp     SharedStub
?Stub101@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub102@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 102
    jmp     SharedStub
?Stub102@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub103@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 103
    jmp     SharedStub
?Stub103@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub104@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 104
    jmp     SharedStub
?Stub104@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub105@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 105
    jmp     SharedStub
?Stub105@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub106@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 106
    jmp     SharedStub
?Stub106@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub107@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 107
    jmp     SharedStub
?Stub107@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub108@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 108
    jmp     SharedStub
?Stub108@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub109@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 109
    jmp     SharedStub
?Stub109@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub110@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 110
    jmp     SharedStub
?Stub110@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub111@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 111
    jmp     SharedStub
?Stub111@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub112@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 112
    jmp     SharedStub
?Stub112@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub113@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 113
    jmp     SharedStub
?Stub113@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub114@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 114
    jmp     SharedStub
?Stub114@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub115@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 115
    jmp     SharedStub
?Stub115@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub116@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 116
    jmp     SharedStub
?Stub116@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub117@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 117
    jmp     SharedStub
?Stub117@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub118@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 118
    jmp     SharedStub
?Stub118@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub119@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 119
    jmp     SharedStub
?Stub119@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub120@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 120
    jmp     SharedStub
?Stub120@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub121@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 121
    jmp     SharedStub
?Stub121@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub122@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 122
    jmp     SharedStub
?Stub122@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub123@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 123
    jmp     SharedStub
?Stub123@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub124@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 124
    jmp     SharedStub
?Stub124@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub125@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 125
    jmp     SharedStub
?Stub125@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub126@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 126
    jmp     SharedStub
?Stub126@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub127@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 127
    jmp     SharedStub
?Stub127@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub128@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 128
    jmp     SharedStub
?Stub128@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub129@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 129
    jmp     SharedStub
?Stub129@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub130@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 130
    jmp     SharedStub
?Stub130@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub131@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 131
    jmp     SharedStub
?Stub131@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub132@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 132
    jmp     SharedStub
?Stub132@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub133@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 133
    jmp     SharedStub
?Stub133@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub134@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 134
    jmp     SharedStub
?Stub134@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub135@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 135
    jmp     SharedStub
?Stub135@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub136@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 136
    jmp     SharedStub
?Stub136@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub137@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 137
    jmp     SharedStub
?Stub137@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub138@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 138
    jmp     SharedStub
?Stub138@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub139@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 139
    jmp     SharedStub
?Stub139@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub140@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 140
    jmp     SharedStub
?Stub140@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub141@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 141
    jmp     SharedStub
?Stub141@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub142@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 142
    jmp     SharedStub
?Stub142@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub143@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 143
    jmp     SharedStub
?Stub143@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub144@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 144
    jmp     SharedStub
?Stub144@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub145@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 145
    jmp     SharedStub
?Stub145@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub146@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 146
    jmp     SharedStub
?Stub146@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub147@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 147
    jmp     SharedStub
?Stub147@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub148@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 148
    jmp     SharedStub
?Stub148@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub149@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 149
    jmp     SharedStub
?Stub149@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub150@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 150
    jmp     SharedStub
?Stub150@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub151@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 151
    jmp     SharedStub
?Stub151@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub152@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 152
    jmp     SharedStub
?Stub152@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub153@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 153
    jmp     SharedStub
?Stub153@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub154@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 154
    jmp     SharedStub
?Stub154@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub155@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 155
    jmp     SharedStub
?Stub155@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub156@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 156
    jmp     SharedStub
?Stub156@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub157@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 157
    jmp     SharedStub
?Stub157@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub158@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 158
    jmp     SharedStub
?Stub158@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub159@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 159
    jmp     SharedStub
?Stub159@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub160@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 160
    jmp     SharedStub
?Stub160@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub161@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 161
    jmp     SharedStub
?Stub161@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub162@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 162
    jmp     SharedStub
?Stub162@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub163@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 163
    jmp     SharedStub
?Stub163@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub164@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 164
    jmp     SharedStub
?Stub164@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub165@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 165
    jmp     SharedStub
?Stub165@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub166@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 166
    jmp     SharedStub
?Stub166@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub167@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 167
    jmp     SharedStub
?Stub167@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub168@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 168
    jmp     SharedStub
?Stub168@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub169@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 169
    jmp     SharedStub
?Stub169@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub170@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 170
    jmp     SharedStub
?Stub170@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub171@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 171
    jmp     SharedStub
?Stub171@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub172@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 172
    jmp     SharedStub
?Stub172@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub173@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 173
    jmp     SharedStub
?Stub173@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub174@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 174
    jmp     SharedStub
?Stub174@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub175@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 175
    jmp     SharedStub
?Stub175@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub176@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 176
    jmp     SharedStub
?Stub176@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub177@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 177
    jmp     SharedStub
?Stub177@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub178@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 178
    jmp     SharedStub
?Stub178@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub179@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 179
    jmp     SharedStub
?Stub179@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub180@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 180
    jmp     SharedStub
?Stub180@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub181@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 181
    jmp     SharedStub
?Stub181@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub182@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 182
    jmp     SharedStub
?Stub182@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub183@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 183
    jmp     SharedStub
?Stub183@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub184@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 184
    jmp     SharedStub
?Stub184@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub185@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 185
    jmp     SharedStub
?Stub185@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub186@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 186
    jmp     SharedStub
?Stub186@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub187@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 187
    jmp     SharedStub
?Stub187@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub188@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 188
    jmp     SharedStub
?Stub188@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub189@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 189
    jmp     SharedStub
?Stub189@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub190@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 190
    jmp     SharedStub
?Stub190@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub191@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 191
    jmp     SharedStub
?Stub191@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub192@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 192
    jmp     SharedStub
?Stub192@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub193@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 193
    jmp     SharedStub
?Stub193@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub194@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 194
    jmp     SharedStub
?Stub194@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub195@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 195
    jmp     SharedStub
?Stub195@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub196@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 196
    jmp     SharedStub
?Stub196@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub197@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 197
    jmp     SharedStub
?Stub197@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub198@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 198
    jmp     SharedStub
?Stub198@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub199@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 199
    jmp     SharedStub
?Stub199@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub200@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 200
    jmp     SharedStub
?Stub200@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub201@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 201
    jmp     SharedStub
?Stub201@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub202@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 202
    jmp     SharedStub
?Stub202@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub203@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 203
    jmp     SharedStub
?Stub203@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub204@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 204
    jmp     SharedStub
?Stub204@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub205@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 205
    jmp     SharedStub
?Stub205@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub206@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 206
    jmp     SharedStub
?Stub206@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub207@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 207
    jmp     SharedStub
?Stub207@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub208@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 208
    jmp     SharedStub
?Stub208@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub209@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 209
    jmp     SharedStub
?Stub209@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub210@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 210
    jmp     SharedStub
?Stub210@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub211@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 211
    jmp     SharedStub
?Stub211@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub212@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 212
    jmp     SharedStub
?Stub212@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub213@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 213
    jmp     SharedStub
?Stub213@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub214@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 214
    jmp     SharedStub
?Stub214@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub215@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 215
    jmp     SharedStub
?Stub215@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub216@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 216
    jmp     SharedStub
?Stub216@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub217@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 217
    jmp     SharedStub
?Stub217@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub218@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 218
    jmp     SharedStub
?Stub218@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub219@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 219
    jmp     SharedStub
?Stub219@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub220@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 220
    jmp     SharedStub
?Stub220@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub221@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 221
    jmp     SharedStub
?Stub221@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub222@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 222
    jmp     SharedStub
?Stub222@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub223@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 223
    jmp     SharedStub
?Stub223@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub224@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 224
    jmp     SharedStub
?Stub224@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub225@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 225
    jmp     SharedStub
?Stub225@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub226@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 226
    jmp     SharedStub
?Stub226@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub227@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 227
    jmp     SharedStub
?Stub227@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub228@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 228
    jmp     SharedStub
?Stub228@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub229@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 229
    jmp     SharedStub
?Stub229@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub230@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 230
    jmp     SharedStub
?Stub230@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub231@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 231
    jmp     SharedStub
?Stub231@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub232@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 232
    jmp     SharedStub
?Stub232@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub233@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 233
    jmp     SharedStub
?Stub233@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub234@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 234
    jmp     SharedStub
?Stub234@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub235@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 235
    jmp     SharedStub
?Stub235@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub236@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 236
    jmp     SharedStub
?Stub236@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub237@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 237
    jmp     SharedStub
?Stub237@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub238@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 238
    jmp     SharedStub
?Stub238@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub239@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 239
    jmp     SharedStub
?Stub239@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub240@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 240
    jmp     SharedStub
?Stub240@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub241@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 241
    jmp     SharedStub
?Stub241@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub242@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 242
    jmp     SharedStub
?Stub242@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub243@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 243
    jmp     SharedStub
?Stub243@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub244@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 244
    jmp     SharedStub
?Stub244@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub245@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 245
    jmp     SharedStub
?Stub245@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub246@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 246
    jmp     SharedStub
?Stub246@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub247@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 247
    jmp     SharedStub
?Stub247@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub248@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 248
    jmp     SharedStub
?Stub248@nsXPTCStubBase@@UEAAIXZ ENDP


?Stub249@nsXPTCStubBase@@UEAAIXZ PROC EXPORT
    push    rbx
    mov     rbx, 249
    jmp     SharedStub
?Stub249@nsXPTCStubBase@@UEAAIXZ ENDP


END
