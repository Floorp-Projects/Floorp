COMMENT | -*- Mode: asm; tab-width: 8; c-basic-offset: 4 -*-

 ***** BEGIN LICENSE BLOCK *****
 Version: MPL 1.1/GPL 2.0/LGPL 2.1

 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS IS" basis,
 WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 for the specific language governing rights and limitations under the
 License.

 The Original Code is mozilla.org Code.

 The Initial Developer of the Original Code is
 Netscape Communications Corporation.
 Portions created by the Initial Developer are Copyright (C) 2001
 the Initial Developer. All Rights Reserved.

 Contributor(s):
   Henry Sobotka <sobotka@axess.com>

 Alternatively, the contents of this file may be used under the terms of
 either of the GNU General Public License Version 2 or later (the "GPL"),
 or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.

 ***** END LICENSE BLOCK *****
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

t_Int4Bytes   equ     001004h
t_Int8Bytes   equ     001008h
t_Float4Bytes equ     000104h
t_Float8Bytes equ     000108h

TypesArray  dd        t_Int4Bytes                       ; nsXPTType::T_I8
            dd        t_Int4Bytes                       ; nsXPTType::T_I16
            dd        t_Int4Bytes                       ; nsXPTType::T_I32
            dd        t_Int8Bytes                       ; nsXPTType::T_I64 (***)
            dd        t_Int4Bytes                       ; nsXPTType::T_U8
            dd        t_Int4Bytes                       ; nsXPTType::T_U16
            dd        t_Int4Bytes                       ; nsXPTType::T_U32
            dd        t_Int8Bytes                       ; nsXPTType::T_U64 (***)
            dd        t_Float4Bytes                     ; nsXPTType::T_FLOAT  (***)
            dd        t_Float8Bytes                     ; nsXPTType::T_DOUBLE (***)
            dd        t_Int4Bytes                       ; nsXPTType::T_BOOL
            dd        t_Int4Bytes                       ; nsXPTType::T_CHAR
            dd        t_Int4Bytes                       ; nsXPTType::T_WCHAR
            dd        t_Int4Bytes                       ; TD_VOID
            dd        t_Int4Bytes                       ; TD_PNSIID
            dd        t_Int4Bytes                       ; TD_DOMSTRING
            dd        t_Int4Bytes                       ; TD_PSTRING
            dd        t_Int4Bytes                       ; TD_PWSTRING
            dd        t_Int4Bytes                       ; TD_INTERFACE_TYPE
            dd        t_Int4Bytes                       ; TD_INTERFACE_IS_TYPE
            dd        t_Int4Bytes                       ; TD_ARRAY
            dd        t_Int4Bytes                       ; TD_PSTRING_SIZE_IS
            dd        t_Int4Bytes                       ; TD_PWSTRING_SIZE_IS
            dd        t_Int4Bytes                       ; TD_UTF8STRING
            dd        t_Int4Bytes                       ; TD_CSTRING
            dd        t_Int4Bytes                       ; TD_ASTRING
            ;  All other values default to 4 byte int/ptr



            ; Optlink puts 'that' in eax, 'index' in edx, 'paramcount' in ecx
            ; 'params' is on the stack...
XPTC_InvokeByIndex PROC OPTLINK EXPORT USES ebx edi esi, that, index, paramcount, params
            
            LOCAL cparams:dword, fparams:dword, reg_edx:dword, reg_ecx:dword, count:dword

            mov   dword ptr [that],  eax              ; that
            mov   dword ptr [index], edx              ; index
            mov   dword ptr [paramcount], ecx         ; paramcount
            mov   dword ptr [count], ecx              ; save a copy of count

;     #define FOURBYTES           4
;     #define EIGHTBYTES          8
;     #define FLOAT      0x00000100
;     #define INT        0x00001000
;     #define LENGTH     0x000000ff
;
;types[ ] = {
;    FOURBYES   | INT,    // int/uint/ptr/etc
;    EIGHTBYTES | INT,    // long long
;    FOURBYES   | FLOAT,  // float
;    EIGHTBYTES | FLOAT   // double
;};    
; params+00h = val  // double
; params+08h = ptr
; params+0ch = type
; params+0dh = flags
; PTR_IS_DATA = 0x01
; ecx = params
; edx = src
; edi = dst
; ebx = type (bh = int/float, bl = byte count)

            xor   eax, eax
            mov   dword ptr [cparams],eax               ; cparams = 0;
            mov   dword ptr [fparams],eax               ; fparams = 0;

            shl   ecx, 03h
            sub   esp, ecx                              ;    dst/esp = add esp, paramCount * 8
            mov   edi, esp

            push  eax                                   ;    push 0 // "end" of floating  point register stack...

            mov   ecx, dword ptr [params]               ;    // params is a "register" variable
@While1:
            mov   eax, dword ptr [count]                ;    while( count-- )
            or    eax, eax                              ;    // (test for zero)
            je    @EndWhile1
            dec   eax
            mov   dword ptr [count], eax
                                                        ;    {

            test byte ptr [ecx+0dh],01h                 ;        if ( params->flags & PTR_IS_DATA ) {
            je    @IfElse1
            lea    edx, dword ptr [ecx+08h]             ;            src = &params->ptr;
            mov    ebx, 01004h                          ;            type = INT | FOURBYTES;
            jmp    @EndIf1
@IfElse1:                                               ;        } else {
            mov    edx, ecx                             ;            src = &params->val
            movzx  eax, byte ptr [ecx+0ch]              ;            type = types[params->type];
            cmp    eax, 22                              ;            // range check params->type... (0 to 22)
            jle    @TypeOK
            mov    eax,2                                ;            // all others default to 4 byte int
@TypeOK:
            mov    ebx, dword ptr [TypesArray+eax*4]
@EndIf1:                                                ;        }

             test   bh, 001h                            ;        if ( type & FLOAT ) {
             je     @EndIf2
             cmp    dword ptr [fparams], 4              ;            if ( fparams < 4 ) {
             jge    @EndIf3
             push   edx                                 ;               push src;
             push   ebx                                 ;               push type
             inc dword ptr [fparams]                    ;               fparams++;
;;;             movzx  eax, bl                             ;               // dst += (type & LENGTH);
;;;             add    edi, eax                            ;
;;;             xor    ebx, ebx                            ;               // type = 0;  // "handled"
@EndIf3:                                                ;            }
@EndIf2:                                                ;        }

                                                        ;        // copy bytes...
@While2:    or     bl, bl                               ;        while( type & LENGTH ) {
            je     @EndWhile2
            test   bh, 010h                             ;           if( type & INT ) {
            je     @EndIf4
            cmp    dword ptr [cparams], 8               ;               if( cparams < 8 ) {
            jge    @EndIf5
            lea    eax, dword ptr [reg_edx]             ;                   (&reg_edx)[cparams] = *src;
            sub    eax, dword ptr [cparams]
            mov    esi, dword ptr [edx]
            mov    dword ptr [eax], esi
            add    dword ptr [cparams], 4               ;                   cparams += 4;
;;;            jmp    @NoCopy                              ;                   // goto nocopy;
@EndIf5:                                                ;               }
@EndIf4:                                                ;           }
            mov    eax, dword ptr [edx]                 ;           *dst = *src;
            mov    dword ptr [edi], eax
@NoCopy:                                                ;nocopy:
            add    edi, 4                               ;           dst++;
            add    edx, 4                               ;           src++;
            sub    bl, 4                                ;           type -= 4;
            jmp    @While2
@EndWhile2:                                             ;        }
            add   ecx, 010h                             ;        params++;
            jmp   @While1
@EndWhile1:                                             ;    }

                                                        ;    // Set up fpu and regs can make the call...
@While3:    pop   ebx                                   ;    while ( pop type ) {
            or    ebx, ebx
            je    @EndWhile3
            pop   edx                                   ;        pop src
            cmp   bl, 08h                               ;        if( type & EIGHTBYTES ) {
            jne   @IfElse6
            fld   qword ptr [edx]                       ;             fld     qword ptr [src]
            jmp   @EndIf6
@IfElse6:                                               ;        } else {
            fld   dword ptr [edx]                       ;             fld     dword ptr [src]
@EndIf6:                                                ;        }
            jmp   @While3
@EndWhile3:                                             ;    }

            ; make the call
            mov     eax, dword ptr [that]      ; get 'that' ("this" ptr)
            mov     ebx, dword ptr [index]     ; get index
            shl     ebx, 03h                   ; index *= 8 bytes per method
            add     ebx, 08h                   ; += 8 at head of vtable
            add     ebx, [eax]                 ; calculate address

            mov     ecx, dword ptr [ebx+04h]
            add     eax, ecx
            sub     esp, 04h                   ; make room for 'this' ptr on stack

            mov     edx, dword ptr [reg_edx]
            mov     ecx, dword ptr [reg_ecx]

            call    dword ptr [ebx]         ; call method

            mov     ecx, dword ptr [paramcount]
            shl     ecx, 03h
            add     esp, ecx                   ; remove space on stack for params
            add     esp, 04h                   ; remove space on stack for 'this' ptr
            ret

            ENDP

            END


