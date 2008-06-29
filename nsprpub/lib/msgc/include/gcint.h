/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
