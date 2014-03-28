/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_MACOSX
#include <mach/task.h>
#include <mach/thread_act.h>
#include <pthread.h>
#elif XP_WIN
#include <windows.h>
#endif

#include "StackTop.h"

void *GetStackTop(void *guess) {
#if defined(XP_MACOSX)
  pthread_t thread = pthread_self();
  return pthread_get_stackaddr_np(thread);
#elif defined(XP_WIN)
#if defined(_MSC_VER) && defined(_M_IX86)
  // offset 0x18 from the FS segment register gives a pointer to
  // the thread information block for the current thread
  NT_TIB* pTib;
  __asm {
    MOV EAX, FS:[18h]
      MOV pTib, EAX
  }
  return static_cast<void*>(pTib->StackBase);
#elif defined(__GNUC__) && defined(i386)
  // offset 0x18 from the FS segment register gives a pointer to
  // the thread information block for the current thread
  NT_TIB* pTib;
  asm ( "movl %%fs:0x18, %0\n"
       : "=r" (pTib)
      );
  return static_cast<void*>(pTib->StackBase);
#elif defined(_M_X64) || defined(__x86_64)
  PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb());
  return static_cast<void*>(pTib->StackBase);
#else
#error Need a way to get the stack bounds on this platform (Windows)
#endif
#else
  return guess;
#endif
}
