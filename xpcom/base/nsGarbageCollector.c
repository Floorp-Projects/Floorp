/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Patrick Beard <beard@netscape.com>
 */

/*
	nsGarbageCollector.c
 */

#include "generic_threads.h"
#include "pprthred.h"

#include <Processes.h>

extern FILE *GC_stdout, *GC_stderr;

extern void NSInitGarbageCollector();
extern void NSShutdownGarbageCollector();
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
	// PR_ThreadScanStackPointers(PR_GetCurrentThread(), &scanner, marker);
	PR_ScanStackPointers(&scanner, marker);
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

void NSInitGarbageCollector()
{
	PRLock* mutex;
	
	// redirect GC's stderr.
	GC_stderr = fopen("RuntimeLeaks", "w");

	mutex = PR_NewLock();
	if (mutex != NULL) {
		GC_generic_init_threads(&mark_all_stacks, mutex,
							 	&locker, &unlocker,
								&stopper, &starter);
	}
}

void NSShutdownGarbageCollector()
{
	// Run a collection to get unreferenced leaks.
	GC_gcollect();

#if 0
	fclose(GC_stderr);
	GC_stderr = fopen("ShutdownLeaks", "w");

	// Try to show leaks in current roots:
	fprintf(GC_stderr, "Shutdown Leaks:\n");
	GC_clear_roots();
	GC_gcollect();
#endif
}
