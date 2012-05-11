/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nspr_symbian_defs_h___
#define nspr_symbian_defs_h___

#include "prthread.h"

/*
 * Internal configuration macros
 */

#define _PR_SI_SYSNAME  "SYMBIAN"
#if defined(__WINS__)
#define _PR_SI_ARCHITECTURE "i386"
#elif defined(__arm__)
#define _PR_SI_ARCHITECTURE "arm"
#else
#error "Unknown CPU architecture"
#endif
#define PR_DLL_SUFFIX		".dll"

#undef	HAVE_STACK_GROWING_UP

#ifdef DYNAMIC_LIBRARY
#define HAVE_DLL
#define USE_DLFCN
#endif

#define _PR_STAT_HAS_ONLY_ST_ATIME
#define _PR_NO_LARGE_FILES
#define _PR_HAVE_SYSV_SEMAPHORES
#define PR_HAVE_SYSV_NAMED_SHARED_MEMORY

#ifndef _PR_PTHREADS
#error "Classic NSPR is not implemented"
#endif

extern void _MD_EarlyInit(void);
extern PRIntervalTime _PR_UNIX_GetInterval(void);
extern PRIntervalTime _PR_UNIX_TicksPerSecond(void);

#define _MD_EARLY_INIT                  _MD_EarlyInit
#define _MD_FINAL_INIT                  _PR_UnixInit
#define _MD_GET_INTERVAL                _PR_UNIX_GetInterval
#define _MD_INTERVAL_PER_SEC            _PR_UNIX_TicksPerSecond

/* For writev() */
#include <sys/uio.h>

#endif /* nspr_symbian_defs_h___ */
