/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
