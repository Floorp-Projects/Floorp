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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef gcint_h___
#define gcint_h___

#include "prmon.h"
#include "prgc.h"

extern PRLogModuleInfo *_pr_msgc_lm;
extern GCInfo _pr_gcData;

#if defined(_WIN32) && !defined(DEBUG)
#undef INLINE_LOCK
#endif

#ifdef INLINE_LOCK
#define LOCK_GC()       EnterCriticalSection(&_pr_gcData.lock->mutexHandle)
#define UNLOCK_GC()     LeaveCriticalSection(&_pr_gcData.lock->mutexHandle)
#else
#define LOCK_GC()       PR_EnterMonitor(_pr_gcData.lock)
#define UNLOCK_GC()     PR_ExitMonitor (_pr_gcData.lock)
#define GC_IS_LOCKED()  (PR_GetMonitorEntryCount(_pr_gcData.lock)!=0)
#endif

#ifdef DEBUG
#define _GCTRACE(x, y) if (_pr_gcData.flags & x) GCTrace y
#else
#define _GCTRACE(x, y)
#endif

extern GCBeginGCHook *_pr_beginGCHook;
extern void *_pr_beginGCHookArg;
extern GCBeginGCHook *_pr_endGCHook;
extern void *_pr_endGCHookArg;

extern GCBeginFinalizeHook *_pr_beginFinalizeHook;
extern void *_pr_beginFinalizeHookArg;
extern GCBeginFinalizeHook *_pr_endFinalizeHook;
extern void *_pr_endFinalizeHookArg;

extern int _pr_do_a_dump;
extern FILE *_pr_dump_file;

extern PRLogModuleInfo *_pr_gc_lm;

/*
** Root finders. Root finders are used by the GC to find pointers into
** the GC heap that are not contained in the GC heap.
*/
typedef struct RootFinderStr RootFinder;

struct RootFinderStr {
    RootFinder *next;
    GCRootFinder *func;
    char *name;
    void *arg;
};
extern RootFinder *_pr_rootFinders;

typedef struct CollectorTypeStr {
    GCType gctype;
    PRUint32 flags;
} CollectorType;

#define GC_MAX_TYPES	256
extern CollectorType *_pr_collectorTypes;

#define _GC_TYPE_BUSY   0x1
#define _GC_TYPE_FINAL  0x2
#define _GC_TYPE_WEAK   0x4

/* Slot in _pr_gcTypes used for free memory */
#define FREE_MEMORY_TYPEIX 255

extern void _PR_InitGC(PRWord flags);
extern void _MD_InitGC(void);
extern void PR_CALLBACK _PR_ScanFinalQueue(void *notused);

/*
** Grow the GC Heap.
*/
extern void *_MD_GrowGCHeap(PRUint32 *sizep);

/*
** Extend the GC Heap.
*/
extern PRBool _MD_ExtendGCHeap(char *base, PRInt32 oldSize, PRInt32 newSize);

/*
** Free a GC segment.
*/
extern void _MD_FreeGCSegment(void *base, PRInt32 len);

#endif /* gcint_h___ */
