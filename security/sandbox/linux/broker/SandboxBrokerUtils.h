/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_SandboxBrokerUtils_h
#define mozilla_SandboxBrokerUtils_h

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sandbox/linux/system_headers/linux_syscalls.h"

// On 32-bit Linux, stat calls are translated by libc into stat64
// calls. We'll intercept those and handle them in the stat functions
// but must be sure to use the right structure layout.

#if defined(__NR_stat64)
typedef struct stat64 statstruct;
#define statsyscall stat64
#define lstatsyscall lstat64
#elif defined(__NR_stat)
typedef struct stat statstruct;
#define statsyscall stat
#define lstatsyscall lstat
#else
#error Missing stat syscall include.
#endif

#endif // mozilla_SandboxBrokerUtils_h
