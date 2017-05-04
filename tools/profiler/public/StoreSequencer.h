/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StoreSequencer_h
#define StoreSequencer_h

// STORE_SEQUENCER is a memory barrier used to ensure profiles are updated
// safely.

#if defined(__arm__)
// TODO Is there something cheaper that will prevent memory stores from being
// reordered?

typedef void (*LinuxKernelMemoryBarrierFunc)(void);
LinuxKernelMemoryBarrierFunc pLinuxKernelMemoryBarrier __attribute__((weak)) =
    (LinuxKernelMemoryBarrierFunc) 0xffff0fa0;

# define STORE_SEQUENCER() pLinuxKernelMemoryBarrier()

#elif defined(__aarch64__)
# define STORE_SEQUENCER() asm volatile("dmb ish" ::: "memory")

#elif defined(__i386__) || defined(__x86_64__) || \
      defined(_M_IX86) || defined(_M_X64)

# if defined(_MSC_VER)
#  include <intrin.h>
#  define STORE_SEQUENCER() _ReadWriteBarrier()
# elif defined(__INTEL_COMPILER)
#  define STORE_SEQUENCER() __memory_barrier()
# elif __GNUC__
#  define STORE_SEQUENCER() asm volatile("" ::: "memory")
# else
#  error "STORE_SEQUENCER not supported for your compiler."
# endif

#else
# error "STORE_SEQUENCER not supported for your platform."
#endif

#endif  // StoreSequencer_h
