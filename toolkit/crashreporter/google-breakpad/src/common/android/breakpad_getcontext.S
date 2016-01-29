// Copyright (c) 2012, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A minimalistic implementation of getcontext() to be used by
// Google Breakpad on Android.

#include "common/android/ucontext_constants.h"

/* int getcontext (ucontext_t *ucp) */

#ifdef __arm__

  .text
  .global breakpad_getcontext
  .hidden breakpad_getcontext
  .type breakpad_getcontext, #function
  .align 0
  .fnstart
breakpad_getcontext:

  /* First, save r4-r11 */
  add   r1, r0, #(MCONTEXT_GREGS_OFFSET + 4*4)
  stm   r1, {r4-r11}

  /* r12 is a scratch register, don't save it */

  /* Save sp and lr explicitely. */
  /* - sp can't be stored with stmia in Thumb-2 */
  /* - STM instructions that store sp and pc are deprecated in ARM */
  str   sp, [r0, #(MCONTEXT_GREGS_OFFSET + 13*4)]
  str   lr, [r0, #(MCONTEXT_GREGS_OFFSET + 14*4)]

  /* Save the caller's address in 'pc' */
  str   lr, [r0, #(MCONTEXT_GREGS_OFFSET + 15*4)]

  /* Save ucontext_t* pointer accross next call */
  mov   r4, r0

  /* Call sigprocmask(SIG_BLOCK, NULL, &(ucontext->uc_sigmask)) */
  mov   r0, #0  /* SIG_BLOCK */
  mov   r1, #0  /* NULL */
  add   r2, r4, #UCONTEXT_SIGMASK_OFFSET
  bl    sigprocmask(PLT)

  /* Intentionally do not save the FPU state here. This is because on
   * Linux/ARM, one should instead use ptrace(PTRACE_GETFPREGS) or
   * ptrace(PTRACE_GETVFPREGS) to get it.
   *
   * Note that a real implementation of getcontext() would need to save
   * this here to allow setcontext()/swapcontext() to work correctly.
   */

  /* Restore the values of r4 and lr */
  mov   r0, r4
  ldr   lr, [r0, #(MCONTEXT_GREGS_OFFSET + 14*4)]
  ldr   r4, [r0, #(MCONTEXT_GREGS_OFFSET +  4*4)]

  /* Return 0 */
  mov   r0, #0
  bx    lr

  .fnend
  .size breakpad_getcontext, . - breakpad_getcontext

#elif defined(__i386__)

  .text
  .global breakpad_getcontext
  .hidden breakpad_getcontext
  .align 4
  .type breakpad_getcontext, @function

breakpad_getcontext:

  movl 4(%esp), %eax   /* eax = uc */

  /* Save register values */
  movl %ecx, MCONTEXT_ECX_OFFSET(%eax)
  movl %edx, MCONTEXT_EDX_OFFSET(%eax)
  movl %ebx, MCONTEXT_EBX_OFFSET(%eax)
  movl %edi, MCONTEXT_EDI_OFFSET(%eax)
  movl %esi, MCONTEXT_ESI_OFFSET(%eax)
  movl %ebp, MCONTEXT_EBP_OFFSET(%eax)

  movl (%esp), %edx   /* return address */
  lea  4(%esp), %ecx  /* exclude return address from stack */
  mov  %edx, MCONTEXT_EIP_OFFSET(%eax)
  mov  %ecx, MCONTEXT_ESP_OFFSET(%eax)

  xorl %ecx, %ecx
  movw %fs, %cx
  mov  %ecx, MCONTEXT_FS_OFFSET(%eax)

  movl $0, MCONTEXT_EAX_OFFSET(%eax)

  /* Save floating point state to fpregstate, then update
   * the fpregs pointer to point to it */
  leal UCONTEXT_FPREGS_MEM_OFFSET(%eax), %ecx
  fnstenv (%ecx)
  fldenv  (%ecx)
  mov %ecx, UCONTEXT_FPREGS_OFFSET(%eax)

  /* Save signal mask: sigprocmask(SIGBLOCK, NULL, &uc->uc_sigmask) */
  leal UCONTEXT_SIGMASK_OFFSET(%eax), %edx
  xorl %ecx, %ecx
  push %edx   /* &uc->uc_sigmask */
  push %ecx   /* NULL */
  push %ecx   /* SIGBLOCK == 0 on i386 */
  call sigprocmask@PLT
  addl $12, %esp

  movl $0, %eax
  ret

  .size breakpad_getcontext, . - breakpad_getcontext

#else
#error "This file has not been ported for your CPU!"
#endif
