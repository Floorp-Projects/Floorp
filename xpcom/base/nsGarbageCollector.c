/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
	nsGarbageCollector.c
 */

#ifdef GC_LEAK_DETECTOR

/* for FILE */
#include <stdio.h>

/* NSPR stuff */
#include "generic_threads.h"
#include "prthread.h"
#include "prlock.h"

/* Linux/Win32 export private NSPR files to include/private */
#ifdef XP_MAC
#include "pprthred.h"
#else
#include "private/pprthred.h"
#endif

#include "nsLeakDetector.h"

extern FILE *GC_stdout, *GC_stderr;

extern void GC_gcollect(void);
extern void GC_clear_roots(void);

static PRStatus PR_CALLBACK scanner(PRThread* t, void** baseAddr, PRUword count, void* closure)
{
	char* begin = (char*)baseAddr;
	char* end = (char*)(baseAddr + count);
	GC_mark_range_proc marker = (GC_mark_range_proc) closure;
	marker(begin, end);
	return PR_SUCCESS;
}

static void mark_all_stacks(GC_mark_range_proc marker)
{
	/* PR_ThreadScanStackPointers(PR_GetCurrentThread(), &scanner, marker); */
	PR_ScanStackPointers(&scanner, (void *)marker);
}

static void locker(void* mutex)
{
	PR_Lock(mutex);
}

static void unlocker(void* mutex)
{
	PR_Unlock(mutex);
}

static void stopper(void* unused)
{
	PR_SuspendAll();
}

static void starter(void* unused)
{
	PR_ResumeAll();
}

nsresult NS_InitGarbageCollector()
{
	PRLock* mutex;
	
	/* redirect GC's stderr to catch startup leaks. */
	GC_stderr = fopen("StartupLeaks", "w");

	mutex = PR_NewLock();
	if (mutex == NULL)
		return NS_ERROR_FAILURE;
		
	GC_generic_init_threads(&mark_all_stacks, mutex,
						 	&locker, &unlocker,
							&stopper, &starter);
	
	return NS_OK;
}

nsresult NS_ShutdownGarbageCollector()
{
	/* do anything you need to shut down the collector. */
	return NS_OK;
}

#endif /* GC_LEAK_DETECTOR */
