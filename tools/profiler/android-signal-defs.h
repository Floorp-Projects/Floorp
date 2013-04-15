/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Android runs a fairly new Linux kernel, so signal info is there,
// but the C library doesn't have the structs defined.

#include <android/api-level.h>

// All NDK platform versions have asm/sigcontext.h for ARM
// Only NDK >= 6, platform >= 9 have asm/sigcontext.h for x86
// Only NDK >= 8, platform >= 9 have asm/sigcontext.h for MIPS
#if defined(__arm__) || defined(__thumb__) || __ANDROID_API__ >= 9
#include <asm/sigcontext.h>
#else
#error use newer NDK or newer platform version (e.g. --with-android-version=9)
#endif

#ifndef __BIONIC_HAVE_UCONTEXT_T
typedef uint32_t __sigset_t;
typedef struct sigcontext mcontext_t;
typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  __sigset_t uc_sigmask;
} ucontext_t;
#endif

