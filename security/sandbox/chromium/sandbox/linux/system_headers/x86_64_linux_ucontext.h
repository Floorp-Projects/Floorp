// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_X86_64_LINUX_UCONTEXT_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_X86_64_LINUX_UCONTEXT_H_

#include <stdint.h>

// We do something compatible with glibc. Hopefully, at some point Android will
// provide that for us, and __BIONIC_HAVE_UCONTEXT_T should be defined.
// Spec:
// http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-AMD64/LSB-Core-AMD64/libc-ddefs.html#AEN5668

#if !defined(__BIONIC_HAVE_UCONTEXT_T)
#include <asm/sigcontext.h>

struct _libc_fpxreg {
  unsigned short significand[4];
  unsigned short exponent;
  unsigned short padding[3];
};

struct _libc_xmmreg {
  uint32_t element[4];
};

struct _libc_fpstate {
  uint16_t cwd;
  uint16_t swd;
  uint16_t twd;
  uint16_t fop;
  uint64_t rip;
  uint64_t rdp;
  uint32_t mxcsr;
  uint32_t mxcsr_mask;
  struct _libc_fpxreg _st[8];
  struct _libc_xmmreg _xmm[16];
  uint32_t padding[24];
};

typedef uint64_t greg_t;

typedef struct {
  greg_t gregs[23];
  struct _libc_fpstate* fpregs;
  unsigned long __reserved1[8];
} mcontext_t;

enum {
  REG_R8 = 0,
  REG_R9,
  REG_R10,
  REG_R11,
  REG_R12,
  REG_R13,
  REG_R14,
  REG_R15,
  REG_RDI,
  REG_RSI,
  REG_RBP,
  REG_RBX,
  REG_RDX,
  REG_RAX,
  REG_RCX,
  REG_RSP,
  REG_RIP,
  REG_EFL,
  REG_CSGSFS,
  REG_ERR,
  REG_TRAPNO,
  REG_OLDMASK,
  REG_CR2,
  NGREG,
};

typedef struct ucontext {
  unsigned long uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  sigset_t uc_sigmask;
  struct _libc_fpstate __fpregs_mem;
} ucontext_t;

#else
#include <sys/ucontext.h>
#endif  // __BIONIC_HAVE_UCONTEXT_T

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_X86_64_LINUX_UCONTEXT_H_
