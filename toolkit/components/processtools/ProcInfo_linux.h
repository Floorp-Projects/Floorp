/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_ProcInfo_linux_h
#define __mozilla_ProcInfo_linux_h

// The following is directly inspired from kernel:
// https://github.com/torvalds/linux/blob/v5.16/include/linux/posix-timers.h#L29-L48
#ifndef CPUCLOCK_SCHED
#  define CPUCLOCK_SCHED 2
#endif
#ifndef CPUCLOCK_PERTHREAD_MASK
#  define CPUCLOCK_PERTHREAD_MASK 4
#endif
#ifndef MAKE_PROCESS_CPUCLOCK
#  define MAKE_PROCESS_CPUCLOCK(pid, clock) \
    ((int)(~(unsigned)(pid) << 3) | (int)(clock))
#endif
#ifndef MAKE_THREAD_CPUCLOCK
#  define MAKE_THREAD_CPUCLOCK(tid, clock) \
    MAKE_PROCESS_CPUCLOCK(tid, (clock) | CPUCLOCK_PERTHREAD_MASK)
#endif

#endif  // ProcInfo_linux_h
