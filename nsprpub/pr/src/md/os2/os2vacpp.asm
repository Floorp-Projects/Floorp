COMMENT | -*- Mode: asm; tab-width: 8; c-basic-offset: 4 -*-
        The contents of this file are subject to the Mozilla Public
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
        created an assembly file for VACPP on OS/2
        |

        .486P
        .MODEL FLAT, OPTLINK
        .STACK

        .CODE

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_SET(PRInt32* val, PRInt32 newval)
;;;---------------------------------------------------------------------
_PR_MD_ATOMIC_SET     PROC OPTLINK EXPORT
        lock xchg    dword ptr [eax],edx
        mov eax, edx;

        ret
_PR_MD_ATOMIC_SET     endp

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_ADD(PRInt32* ptr, PRInt32 val)
;;;---------------------------------------------------------------------
_PR_MD_ATOMIC_ADD     PROC OPTLINK EXPORT
        mov ecx, edx
        lock xadd dword ptr [eax], edx
        mov eax, edx
        add eax, ecx

        ret
_PR_MD_ATOMIC_ADD     endp

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_INCREMENT(PRInt32* val)
;;;---------------------------------------------------------------------
_PR_MD_ATOMIC_INCREMENT     PROC OPTLINK EXPORT
        mov edx, 1
        lock xadd dword ptr [eax], edx
        mov eax, edx
        inc eax

        ret
_PR_MD_ATOMIC_INCREMENT     endp

;;;---------------------------------------------------------------------
;;; PRInt32 _Optlink _PR_MD_ATOMIC_DECREMENT(PRInt32* val)
;;;---------------------------------------------------------------------
_PR_MD_ATOMIC_DECREMENT     PROC OPTLINK EXPORT
        mov edx, 0ffffffffh
        lock xadd dword ptr [eax], edx
        mov eax, edx
        dec eax

        ret
_PR_MD_ATOMIC_DECREMENT     endp

        END
