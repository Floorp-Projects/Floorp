/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s):
 *   Patrick Beard <beard@netscape.com>
 */

/*
 * prgcleak.c
 */

#ifdef GC_LEAK_DETECTOR

/* for FILE */
#include <stdio.h>

/* NSPR stuff */
#include "generic_threads.h"
#include "primpl.h"

extern FILE *GC_stdout, *GC_stderr;

extern void GC_gcollect(void);
extern void GC_clear_roots(void);

static PRStatus PR_CALLBACK scanner(PRThread* t, void** baseAddr,
                                    PRUword count, void* closure)
{
    char* begin = (char*)baseAddr;
    char* end = (char*)(baseAddr + count);
    GC_mark_range_proc marker = (GC_mark_range_proc) closure;
    marker(begin, end);
    return PR_SUCCESS;
}

static void mark_all_stacks(GC_mark_range_proc marker)
{
    PR_ScanStackPointers(&scanner, (void *)marker);
}

static void locker(void* mutex)
{
    if (_PR_MD_CURRENT_CPU())
        PR_Lock(mutex);
}

static void unlocker(void* mutex)
{
    if (_PR_MD_CURRENT_CPU())
        PR_Unlock(mutex);
}

static void stopper(void* unused)
{
    if (_PR_MD_CURRENT_CPU())
        PR_SuspendAll();
}

static void starter(void* unused)
{
    if (_PR_MD_CURRENT_CPU())
        PR_ResumeAll();
}

void _PR_InitGarbageCollector()
{
    void* mutex;

    /* redirect GC's stderr to catch startup leaks. */
    GC_stderr = fopen("StartupLeaks", "w");

    mutex = PR_NewLock();
    PR_ASSERT(mutex != NULL);

    GC_generic_init_threads(&mark_all_stacks, mutex, 
            &locker, &unlocker,
            &stopper, &starter);
}

void _PR_ShutdownGarbageCollector()
{
    /* do anything you need to shut down the collector. */
}

#endif /* GC_LEAK_DETECTOR */
