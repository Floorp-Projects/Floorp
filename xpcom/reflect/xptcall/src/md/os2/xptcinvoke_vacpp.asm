COMMENT | -*- Mode: asm; tab-width: 8; c-basic-offset: 4 -*-

        The contents of this file are subject to the Netscape Public License
        Version 1.0 (the "NPL"); you may not use this file except in
        compliance with the NPL.  You may obtain a copy of the NPL at
        http://www.mozilla.org/NPL/

        Software distributed under the NPL is distributed on an "AS IS" basis,
        WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
        for the specific language governing rights and limitations under the
        NPL.

        The Initial Developer of this code under the NPL is Netscape
        Communications Corporation.  Portions created by Netscape are
        Copyright (C) 1999 Netscape Communications Corporation.  All Rights
        Reserved.

        Contributor: Henry Sobotka <sobotka@axess.com>

        This Original Code has been modified by IBM Corporation.
        Modifications made by IBM described herein are
        Copyright (c) International Business Machines
        Corporation, 2000

        Modifications to Mozilla code or documentation
        identified per MPL Section 3.3

        Date             Modified by     Description of modification
        03/23/2000       IBM Corp.      Various fixes for parameter passing and adjusting the 'this' 
                                                pointer for multiply inherited objects.

        xptcall_vacpp.asm: ALP assembler procedures for VAC++ build of xptcall

        We use essentially the same algorithm as the other platforms, except 
        Optlink as calling convention. This means loading the three leftmost
        conforming (<= 4 bytes, enum or pointer) parameters into eax, edx and ecx,
        and the four leftmost float types atop the FP stack. As "this" goes into
        eax, we only have to load edx and ecx (if there are two parameters).
        Nonconforming parameters go on the stack. As the right-size space has
        to be allocated at the proper place (order of parameters) in the stack
        for each conforming parameter, we simply copy them all. |
        

            .486P
            .MODEL FLAT, OPTLINK
            .STACK

            .CODE

            EXTERN OPTLINK invoke_copy_to_stack__FPUiUlP13nsXPTCVariant:PROC
       
            ; Optlink puts pStack in eax, count in edx, params in ecx
CallMethodFromVTable__FPvUiPUiT1T2iT6T3 PROC OPTLINK EXPORT    pStack, count, params,
                    that, index, ibytes, cpc, cpp, fpc, fpp
                
            mov     eax, esp                    ; set up value of pStack for copy call
            sub     eax, dword ptr [ibytes]     ; make room for copied params
            mov     esp, eax                    ; adjust stack pointer accordingly
            sub     esp, 0Ch                    ; make room for three invoke_copy params
            call    invoke_copy_to_stack__FPUiUlP13nsXPTCVariant

            add     esp, 0Ch                   ; reset esp
            mov     eax, dword ptr [that]      ; get 'that' ("this" ptr)
            mov     edx, dword ptr [eax]
            mov     ebx, dword ptr [index]     ; get index
            shl     ebx, 03h                   ; index *= 8 bytes per method
            add     ebx, 08h                   ; += 8 at head of vtable
            add     ebx, [eax]                 ; calculate address

            mov     ecx, dword ptr [ebx+04h]
            add     eax, ecx
            push    eax


                    ;**************************************
                    ;     Load fpparams into registers    
                    ;**************************************
            mov     ecx, dword ptr [fpc]    ; get fpcount
            cmp     ecx, 0h                 ; check fp count
            je      loadcp                  ; if no fpparams, goto 'loadcp'

  loadfps:  dec cl                          ; ecx--
            mov     edx, ecx                ; get fpparams array offset
            shl     edx, 03h                ; * 8 bytes per fp param
            add     edx, dword ptr [fpp]    ; add base array addr
            fld     qword ptr [edx]         ; add fp param at location 'edx'
            cmp     cl, 0h                  ;
            jne     loadfps                 ; if more params, loop around


                    ;*************************************
                    ;     Load cparams into registers
                    ;*************************************
  loadcp:   cmp     dword ptr [cpc], 0h     ; check param count 
            je      done                    ; if no params, goto 'done' and call function
    
            mov     edx, dword ptr [cpp]    ; load params
            cmp     dword ptr [cpc], 01h    ; check param count
            je      loadone                 ; only one param to load, goto loadone
    
            mov     ecx, [edx + 04h]        ; load 2nd param if present
    
  loadone:  mov     edx, [edx]              ; load 1st param if present

                    ;***********************
                    ;     Call Function
                    ;***********************
  done:     call    dword ptr [ebx]         ; call method
            add     esp, dword ptr [ibytes] ; deflate stack by ibytes
            add     esp, 04h                ; plus 4
            ret                             ; method rc in eax

CallMethodFromVTable__FPvUiPUiT1T2iT6T3    ENDP

            END
