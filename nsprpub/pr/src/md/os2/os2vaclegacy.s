/ -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/ 
/ The contents of this file are subject to the Mozilla Public
/ License Version 1.1 (the "License"); you may not use this file
/ except in compliance with the License. You may obtain a copy of
/ the License at http://www.mozilla.org/MPL/
/ 
/ Software distributed under the License is distributed on an "AS
/ IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
/ implied. See the License for the specific language governing
/ rights and limitations under the License.
/ 
/ The Original Code is the Netscape Portable Runtime (NSPR).
/ 
/ The Initial Developer of the Original Code is InnoTek
/ Systemberatung GmbH.
/ Portions created by the Initial Developer are Copyright (C) 2003
/ the Initial Developer. All Rights Reserved.
/ 
/ Contributor(s):
/    InnoTek Systemberatung GmbH / Knut St. Osmundsen
/ 
/ Alternatively, the contents of this file may be used under the
/ terms of the GNU General Public License Version 2 or later (the
/ "GPL"), in which case the provisions of the GPL are applicable 
/ instead of those above.  If you wish to allow use of your 
/ version of this file only under the terms of the GPL and not to
/ allow others to use your version of this file under the MPL,
/ indicate your decision by deleting the provisions above and
/ replace them with the notice and other provisions required by
/ the GPL.  If you do not delete the provisions above, a recipient
/ may use your version of this file under either the MPL or the
/ GPL.
/ 

    .text
    .align 4
    .globl PR_NewMonitor
PR_NewMonitor:
    jmp _PR_NewMonitor

    .align 4
    .globl PR_EnterMonitor
PR_EnterMonitor:
    mov     %eax,  4(%esp) 
    jmp _PR_EnterMonitor

    .align 4
    .globl PR_ExitMonitor 
PR_ExitMonitor:
    mov     %eax,  4(%esp) 
    jmp _PR_ExitMonitor 


    
    .align 4
    .globl PR_AttachThread
PR_AttachThread:
    mov     %eax,  4(%esp) 
    mov     %edx,  8(%esp) 
    mov     %ecx, 12(%esp)
    jmp _PR_AttachThread
    
    .align 4
    .globl PR_DetachThread
PR_DetachThread:
    jmp _PR_DetachThread
    
    .align 4
    .globl PR_GetCurrentThread
PR_GetCurrentThread:
    jmp _PR_GetCurrentThread


