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

 Alternatively, the contents of this file may be used under the terms of
 either the GNU General Public License Version 2 or later (the "GPL"), or
 the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.

 ***** END LICENSE BLOCK *****
        License Version 1.1 (the "License"); you may not use this file
        except in compliance with the License. You may obtain a copy of
        the License at http://www.mozilla.org/MPL/

        Software distributed under the License is distributed on an "AS
        IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
        implied. See the License for the specific language governing
        rights and limitations under the License.

        The Original Code is the Netscape Portable Runtime (NSPR).

        The Initial Developer of the Original Code is IBM Corporation.
        Portions created by IBM are Copyright (C) 2001 IBM Corporation.
        All Rights Reserved.

        Contributor(s):

        Alternatively, the contents of this file may be used under the
        terms of the GNU General Public License Version 2 or later (the
        "GPL"), in which case the provisions of the GPL are applicable 
        instead of those above.  If you wish to allow use of your 
        version of this file only under the terms of the GPL and not to
        allow others to use your version of this file under the MPL,
        indicate your decision by deleting the provisions above and
        replace them with the notice and other provisions required by
        the GPL.  If you do not delete the provisions above, a recipient
        may use your version of this file under either the MPL or the
        GPL.

        Windows uses inline assembly for their atomic functions, so we have
        created an assembly file for VACPP on OS/2.

        This assembly file also contains an implementation of RAM semaphores.

        Notes:
          The ulTIDPID element of the RAMSEM structure is overloaded in the 386
          implementation to hold the TID:PID in the lower 31 bits and the lock
          bit in the high bit
        |
        page ,132

        .486P
        ASSUME  CS:FLAT,  DS:FLAT,  SS:FLAT,  ES:FLAT,  FS:FLAT

        EXTRN   Dos32PostEventSem:PROC
        EXTRN   Dos32WaitEventSem:PROC
        EXTRN   Dos32ResetEventSem:PROC

ramsem  STRUC
        ramsem_ulTIDPID         DD      ?
        ramsem_hevSem           DD      ?
        ramsem_cLocks           DD      ?
        ramsem_cWaiting         DW      ?
        ramsem_cPosts           DW      ?
ramsem  ENDS

ERROR_SEM_TIMEOUT               equ     121
ERROR_NOT_OWNER                 equ     288
SEM_RELEASE_UNOWNED             equ     1
SEM_RELEASE_ALL                 equ     2
TS_LOCKBIT                      equ     31


DATA    SEGMENT DWORD USE32 PUBLIC 'DATA'

        EXTRN   plisCurrent:DWORD

DATA    ENDS

CODE32  SEGMENT USE32 PUBLIC 'CODE'

        PUBLIC  SemRequest486
        PUBLIC  SemReleasex86

        PUBLIC  _PR_MD_ATOMIC_SET
        PUBLIC  _PR_MD_ATOMIC_ADD
        PUBLIC  _PR_MD_ATOMIC_INCREMENT
        PUBLIC  _PR_MD_ATOMIC_DECREMENT

;;;---------------------------------------------------------------------------
;;; APIRET _Optlink SemRequest(PRAMSEM pramsem, ULONG ulTimeout);
;;;
;;; Registers:
;;;   EAX - packed TID:PID word
;;;   ECX - address of RAMSEM structure
;;;   EDX - length of timeout in milli-seconds
;;;---------------------------------------------------------------------------

        ALIGN   10H
SemRequest486     PROC
        push    ebx                                  ; Save ebx (volatile)
        mov     ecx, eax                             ; PRAMSEM must be in ecx,
                                                     ; not eax, for cmpxchg

        mov     ebx, dword ptr [plisCurrent]
        mov     eax, dword ptr [ebx+4]               ; Place thread id in high
                                                     ; word, process id in low
        mov     ax,  word ptr [ebx]                  ; word
        mov     ebx,eax

req486_test:
        xor     eax,eax
        cmp     (ramsem PTR [ecx]).ramsem_ulTIDPID, ebx ; If we own the sem, just
        jz short req486_inc_exit                      ; increment the use count

        lock inc     (ramsem PTR [ecx]).ramsem_cWaiting ; inc waiting flag

;       lock                                         ; Uncomment for SMP
        DB      0F0h
;       cmpxchg (ramsem PTR [ecx]).ramsem_ulTIDPID, ebx
;         (byte 3 is the offset of ulProcessThread into the RAMSEM structure)
        DB      00Fh
        DB      0B1h
        DB      019h
        jnz short req486_sleep

req486_inc_exit:
   lock inc     (ramsem PTR [ecx]).ramsem_cLocks

req486_exit:
        pop     ebx                                  ; Restore ebx
        ret

req486_sleep:
        push    ecx                                  ; Save ecx (volatile)
        push    edx                                  ; Save edx (volatile)
        push    edx                                  ; timeout
        push    (ramsem PTR [ecx]).ramsem_hevSem
        call    Dos32WaitEventSem
        add     esp, 8
        pop     edx                                  ; restore edx
        pop     ecx                                  ; restore ecx
        or      eax, eax
        jne     req486_exit                          ; Exit, if error

        push    ecx                                  ; Save ecx (volatile)
        push    edx                                  ; Save edx (volatile)
        sub     esp, 4                               ; Use stack space for
        push    esp                                  ;  dummy pulPostCt
        push    (ramsem PTR [ecx]).ramsem_hevSem
        call    Dos32ResetEventSem
        add     esp, 12
        pop     edx                                  ; restore edx
        pop     ecx                                  ; restore ecx
        jmp     req486_test                          ; Retry the semaphore

SemRequest486     ENDP

;;;---------------------------------------------------------------------
;;; APIRET _Optlink SemReleasex86(PRAMSEM pramsem, ULONG flFlags);
;;;
;;; Registers:
;;;   EAX - address of RAMSEM structure
;;;   ECX - temporary variable
;;;   EDX - flags
;;;---------------------------------------------------------------------

        ALIGN   10H
SemReleasex86     PROC
        test    edx, SEM_RELEASE_UNOWNED             ; If set, don't bother
        jnz short rel_ownerok                        ; getting/checking PID/TID

        push    ebx                                  ; Save ebx (volatile)
        mov     ebx, dword ptr [plisCurrent]
        mov     ecx, dword ptr [ebx+4]               ; Place thread id in high
                                                     ; word, process id in low
        mov     cx,  word ptr [ebx]                  ; word
        pop     ebx                                  ; Restore ebx

        sub     ecx, (ramsem PTR [eax]).ramsem_ulTIDPID ; This thread the owner?
        shl     ecx,1                                ; Don't compare top bit
        jnz short rel_notowner

rel_ownerok:
        test    edx, SEM_RELEASE_ALL
        jnz short rel_clear

   lock dec     (ramsem PTR [eax]).ramsem_cLocks
        jnz short rel_exit

rel_disown:
        mov     (ramsem PTR [eax]).ramsem_ulTIDPID, 0

   lock inc     (ramsem PTR [eax]).ramsem_cPosts
        mov     cx, (ramsem PTR [eax]).ramsem_cWaiting
        cmp     (ramsem PTR [eax]).ramsem_cPosts, cx
        jne short rel_post

rel_exit:
        xor     eax, eax
        ret

rel_clear:
   lock mov     (ramsem PTR [eax]).ramsem_cLocks,0
        jmp     rel_disown

rel_notowner:
        mov     eax, ERROR_NOT_OWNER
        ret

rel_post:
        mov     (ramsem PTR [eax]).ramsem_cPosts, cx
        push    (ramsem PTR [eax]).ramsem_hevSem
        call    Dos32PostEventSem
        add     esp,4
        xor     eax,eax
        ret
SemReleasex86     ENDP

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_SET(PRInt32* val, PRInt32 newval)
;;;---------------------------------------------------------------------
        ALIGN   10H
_PR_MD_ATOMIC_SET     proc
   lock xchg    dword ptr [eax],edx
        mov eax, edx;
        ret
_PR_MD_ATOMIC_SET     endp

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_ADD(PRInt32* ptr, PRInt32 val)
;;;---------------------------------------------------------------------
        ALIGN   10H
_PR_MD_ATOMIC_ADD     proc
        mov ecx, edx
        lock xadd dword ptr [eax], edx
        mov eax, edx
        add eax, ecx
        ret
_PR_MD_ATOMIC_ADD     endp

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_INCREMENT(PRInt32* val)
;;;---------------------------------------------------------------------
        ALIGN   10H
_PR_MD_ATOMIC_INCREMENT     proc
        mov edx, 1
        lock xadd dword ptr [eax], edx
        mov eax, edx
        inc eax
        ret
_PR_MD_ATOMIC_INCREMENT     endp

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_DECREMENT(PRInt32* val)
;;;---------------------------------------------------------------------
        ALIGN   10H
_PR_MD_ATOMIC_DECREMENT     proc
        mov edx, 0ffffffffh
        lock xadd dword ptr [eax], edx
        mov eax, edx
        dec eax
        ret
_PR_MD_ATOMIC_DECREMENT     endp

CODE32  ENDS
END
