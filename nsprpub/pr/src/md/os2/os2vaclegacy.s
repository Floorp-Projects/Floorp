/ -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/ 
/ This Source Code Form is subject to the terms of the Mozilla Public
/ License, v. 2.0. If a copy of the MPL was not distributed with this
/ file, You can obtain one at http://mozilla.org/MPL/2.0/.

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


