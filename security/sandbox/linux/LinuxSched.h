/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LinuxSched_h
#define mozilla_LinuxSched_h

#include <linux/sched.h>

// Some build environments, in particular the Android NDK, don't
// define some of the newer clone/unshare flags ("newer" relatively
// speaking; CLONE_NEWUTS is present since kernel 2.6.19 in 2006).

#ifndef CLONE_NEWUTS
#define CLONE_NEWUTS  0x04000000
#endif
#ifndef CLONE_NEWIPC
#define CLONE_NEWIPC  0x08000000
#endif
#ifndef CLONE_NEWUSER
#define CLONE_NEWUSER 0x10000000
#endif
#ifndef CLONE_NEWPID
#define CLONE_NEWPID  0x20000000
#endif
#ifndef CLONE_NEWNET
#define CLONE_NEWNET  0x40000000
#endif
#ifndef CLONE_IO
#define CLONE_IO      0x80000000
#endif

#endif
