// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* This file has been imported from
 * http://git.chromium.org/gitweb/?p=chromium.git;a=blob;f=sandbox/linux/services/android_i386_ucontext.h;hb=99b3e83972e478a42fa72da1ffefee58413e87d4
 */

#ifndef SANDBOX_LINUX_SERVICES_ANDROID_I386_UCONTEXT_H_
#define SANDBOX_LINUX_SERVICES_ANDROID_I386_UCONTEXT_H_

// We do something compatible with glibc. Hopefully, at some point Android will
// provide that for us, and __BIONIC_HAVE_UCONTEXT_T should be defined.
// This is mostly copied from breakpad (common/android/include/sys/ucontext.h),
// except we do use sigset_t for uc_sigmask instead of a custom type.

#if !defined(__BIONIC_HAVE_UCONTEXT_T)
#include <asm/sigcontext.h>

/* 80-bit floating-point register */
struct _libc_fpreg {
  unsigned short significand[4];
  unsigned short exponent;
};

/* Simple floating-point state, see FNSTENV instruction */
struct _libc_fpstate {
  unsigned long cw;
  unsigned long sw;
  unsigned long tag;
  unsigned long ipoff;
  unsigned long cssel;
  unsigned long dataoff;
  unsigned long datasel;
  struct _libc_fpreg _st[8];
  unsigned long status;
};

typedef uint32_t greg_t;

typedef struct {
  uint32_t gregs[19];
  struct _libc_fpstate* fpregs;
  uint32_t oldmask;
  uint32_t cr2;
} mcontext_t;

enum {
  REG_GS = 0,
  REG_FS,
  REG_ES,
  REG_DS,
  REG_EDI,
  REG_ESI,
  REG_EBP,
  REG_ESP,
  REG_EBX,
  REG_EDX,
  REG_ECX,
  REG_EAX,
  REG_TRAPNO,
  REG_ERR,
  REG_EIP,
  REG_CS,
  REG_EFL,
  REG_UESP,
  REG_SS,
};

typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  sigset_t uc_sigmask;
  struct _libc_fpstate __fpregs_mem;
} ucontext_t;

#else
#include <sys/ucontext.h>
#endif  // __BIONIC_HAVE_UCONTEXT_T

#endif  // SANDBOX_LINUX_SERVICES_ANDROID_I386_UCONTEXT_H_
