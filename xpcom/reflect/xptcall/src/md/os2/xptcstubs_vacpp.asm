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

        xptcallstub_vacpp.asm: ALP assembler procedure for VAC++ build of xptcall
	|

        .486P
        .MODEL FLAT, OPTLINK
        .STACK

        .CODE

        EXTERN OPTLINK PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi:PROC

            ; Optlink puts stubidx in eax
SetEntryFromIndex__Fi       PROC OPTLINK EXPORT stubidx
        mov     dword ptr [esp+20h], ecx    ; move arg into stack slot
        mov	    dword ptr [esp+1ch], edx    ; ditto
        lea     ecx, dword ptr [esp+1ch]    ; load args address
        mov	    edx, eax                    ; index
        mov     eax, dword ptr [ebp+18h]    ; this
        sub	    esp, 0ch                    ; room for params
        ;call    dword ptr [ebp+10h]         ; PrepareAndDispatch
        call    PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi
        add	    esp, 10h                    ; clean up
        ret
SetEntryFromIndex__Fi       ENDP
        
        END
