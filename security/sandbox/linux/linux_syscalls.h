// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* This file has been imported from
 * http://git.chromium.org/gitweb/?p=chromium.git;a=blob_plain;f=sandbox/linux/services/linux_syscalls.h;h=77c1be8b82a0fe4aca308ca40547ebf7a008d24a;hb=2362c9abea79cae475921bdeee58f9e3910d211c
 */

// This header will be kept up to date so that we can compile system-call
// policies even when system headers are old.
// System call numbers are accessible through __NR_syscall_name.

#ifndef SANDBOX_LINUX_SERVICES_LINUX_SYSCALLS_H_
#define SANDBOX_LINUX_SERVICES_LINUX_SYSCALLS_H_

#if defined(__x86_64__)
#include "x86_64_linux_syscalls.h"
#endif

#if defined(__i386__)
#include "x86_32_linux_syscalls.h"
#endif

#if defined(__arm__) && defined(__ARM_EABI__)
#include "arm_linux_syscalls.h"
#endif

#endif  // SANDBOX_LINUX_SERVICES_LINUX_SYSCALLS_H_

