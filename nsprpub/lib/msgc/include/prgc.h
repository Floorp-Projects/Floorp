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

#ifndef prgc_h___
#define prgc_h___

/*
** API to NSPR gc memory system.
*/
#include "prtypes.h"
#include "prmon.h"
#include "prthread.h"
#include <stdio.h>

#if defined(WIN16)
#define GCPTR __far
#else
#define GCPTR
#endif


PR_BEGIN_EXTERN_C

/*
** Initialize the garbage collector.
**     "flags" is the trace flags (see below).
**     "initialHeapSize" is the initial size of the heap and may be zero
**        if the default is desired.
**    "segmentSize" is the size of each segment of memory added to the
**       heap when the heap is grown.
*/
PR_EXTERN(void) PR_InitGC(
    PRWord flags, PRInt32 initialHeapSize, PRInt32 segmentSize, PRThreadScope scope);

/*
** Shuts down gc and frees up all memory associated with it. 
*/
PR_EXTERN(void) PR_ShutdownGC(PRBool finalizeOnExit);

/*
** This walk function will be called for every gc object in the
** heap as it is walked. If it returns non-zero, the walk is terminated.
*/
typedef PRInt32 (*PRWalkFun)(void GCPTR* obj, void* data);

/*
** GC Type record. This defines all of the GC operations used on a
** particular object type. These structures are passed to
** PR_RegisterType.
*/
typedef struct GCType {
    /*
    ** Scan an object that is in the GC heap and call GCInfo.livePointer
    ** on all of the pointers in it. If this slot is null then the object
    ** won't be scanned (i.e. it has no embedded pointers).
    */
    void (PR_CALLBACK *scan)(void GCPTR *obj);

    /*
    ** Finalize an object that has no references. This is called by the
    ** GC after it has determined where the object debris is but before
    ** it has moved the debris to the logical "free list". The object is
    ** marked alive for this call and removed from the list of objects
    ** that need finalization (finalization only happens once for an
    ** object). If this slot is null then the object doesn't need
    ** finalization.
    */
    void (PR_CALLBACK *finalize)(void GCPTR *obj);

    /*
    ** Dump out an object during a PR_DumpGCHeap(). This is used as a
    ** debugging tool.
    */
    void (PR_CALLBACK *dump)(FILE *out, void GCPTR *obj, PRBool detailed, PRIntn indentLevel);

    /*
    ** Add object to summary table.
    */
    void (PR_CALLBACK *summarize)(void GCPTR *obj, PRUint32 bytes);

    /*
    ** Free hook called by GC when the object is being freed.
    */
    void (PR_CALLBACK *free)(void *obj);

    /* Weak pointer support: If the object has a weak pointer (Note:
       at most one), this function is used to get the weak link's
       offset from the start of the body of a gc object */
    PRUint32 (PR_CALLBACK *getWeakLinkOffset)(void *obj);

    /* Descriptive character for dumping this GCType */
    char kindChar;

    /*
    ** Walker routine. This routine should apply fun(obj->ptr, data)
    ** for every gc pointer within the object.
    */
    PRInt32 (PR_CALLBACK *walk)(void GCPTR *obj, PRWalkFun fun, void* data);
} GCType;

/*
** This data structure must be added as the hash table passed to 
** the summarize method of GCType.
*/ 
typedef struct PRSummaryEntry {
    void* clazz;
    PRInt32 instancesCount;
    PRInt32 totalSize;
} PRSummaryEntry;

/*
** This function pointer must be registered by users of nspr
** to produce the finally summary after all object in the
** heap have been visited.
*/
typedef void (PR_CALLBACK *PRSummaryPrinter)(FILE *out, void* closure);

PR_EXTERN(void) PR_CALLBACK PR_RegisterSummaryPrinter(PRSummaryPrinter fun, void* closure);

typedef void PR_CALLBACK GCRootFinder(void *arg);
typedef void PR_CALLBACK GCBeginFinalizeHook(void *arg);
typedef void PR_CALLBACK GCEndFinalizeHook(void *arg);
typedef void PR_CALLBACK GCBeginGCHook(void *arg);
typedef void PR_CALLBACK GCEndGCHook(void *arg);

typedef enum { PR_GCBEGIN, PR_GCEND } GCLockHookArg;

typedef void PR_CALLBACK GCLockHookFunc(GCLockHookArg arg1, void *arg2);

typedef struct GCLockHook GCLockHook;

struct GCLockHook {
  GCLockHookFunc* func;
  void* arg;
  GCLockHook* next;
  GCLockHook* prev;
};


/*
** Hooks which are called at the beginning and end of the GC process.
** The begin hooks are called before the root finding step. The hooks are
** called with threading disabled, so it is now allowed to re-enter the
** kernel. The end hooks are called after the gc has finished but before
** the finalizer has run.
*/
PR_EXTERN(void) PR_CALLBACK PR_SetBeginGCHook(GCBeginGCHook *hook, void *arg);
PR_EXTERN(void) PR_CALLBACK PR_GetBeginGCHook(GCBeginGCHook **hook, void **arg);
PR_EXTERN(void) PR_CALLBACK PR_SetEndGCHook(GCBeginGCHook *hook, void *arg);
PR_EXTERN(void) PR_CALLBACK PR_GetEndGCHook(GCEndGCHook **hook, void **arg);

/*
** Called before SuspendAll is called by dogc, so that GC thread can hold
** all the locks before hand to avoid any deadlocks
*/

/*
PR_EXTERN(void) PR_SetGCLockHook(GCLockHook *hook, void *arg);
PR_EXTERN(void) PR_GetGCLockHook(GCLockHook **hook, void **arg);
*/

PR_EXTERN(int) PR_RegisterGCLockHook(GCLockHookFunc *hook, void *arg);

/*
** Hooks which are called at the beginning and end of the GC finalization
** process. After the GC has identified all of the dead objects in the
** heap, it looks for objects that need finalization. Before it calls the
** first finalization proc (see the GCType structure above) it calls the
** begin hook. When it has finalized the last object it calls the end
** hook.
*/
PR_EXTERN(void) PR_SetBeginFinalizeHook(GCBeginFinalizeHook *hook, void *arg);
PR_EXTERN(void) PR_GetBeginFinalizeHook(GCBeginFinalizeHook **hook, void **arg);
PR_EXTERN(void) PR_SetEndFinalizeHook(GCBeginFinalizeHook *hook, void *arg);
PR_EXTERN(void) PR_GetEndFinalizeHook(GCEndFinalizeHook **hook, void **arg);

/*
** Register a GC type. Return's the index into the GC internal type
** table. The returned value is passed to PR_AllocMemory. After the call,
** the "type" memory belongs to the GC (the caller must not free it or
** change it).
*/
PR_EXTERN(PRInt32) PR_RegisterType(GCType *type);

/*
** Register a root finder with the collector. The collector will call
** these functions to identify all of the roots before collection
** proceeds. "arg" is passed to the function when it is called.
*/
PR_EXTERN(PRStatus) PR_RegisterRootFinder(GCRootFinder func, char *name, void *arg);

/*
** Allocate some GC'able memory. The object must be at least bytes in
** size. The type index function for the object is specified. "flags"
** specifies some control flags. If PR_ALLOC_CLEAN is set then the memory
** is zero'd before being returned. If PR_ALLOC_DOUBLE is set then the
** allocated memory is double aligned.
**
** Any memory cell that you store a pointer to something allocated by
** this call must be findable by the GC. Use the PR_RegisterRootFinder to
** register new places where the GC will look for pointers into the heap.
** The GC already knows how to scan any NSPR threads or monitors.
*/
PR_EXTERN(PRWord GCPTR *)PR_AllocMemory(
    PRWord bytes, PRInt32 typeIndex, PRWord flags);
PR_EXTERN(PRWord GCPTR *)PR_AllocSimpleMemory(
    PRWord bytes, PRInt32 typeIndex);

/*
** This function can be used to cause PR_AllocMemory to always return
** NULL. This may be useful in low memory situations when we're trying to
** shutdown applets.
*/
PR_EXTERN(void) PR_EnableAllocation(PRBool yesOrNo);

/* flags bits */
#define PR_ALLOC_CLEAN 0x1
#define PR_ALLOC_DOUBLE 0x2
#define PR_ALLOC_ZERO_HANDLE 0x4              /* XXX yes, it's a hack */

/*
** Force a garbage collection right now. Return when it completes.
*/
PR_EXTERN(void) PR_GC(void);

/*
** Force a finalization right now. Return when finalization has
** completed. Finalization completes when there are no more objects
** pending finalization. This does not mean there are no objects in the
** gc heap that will need finalization should a collection be done after
** this call.
*/
PR_EXTERN(void) PR_ForceFinalize(void);

/*
** Dump the GC heap out to the given file. This will stop the system dead
** in its tracks while it is occuring.
*/
PR_EXTERN(void) PR_DumpGCHeap(FILE *out, PRBool detailed);

/*
** Wrapper for PR_DumpGCHeap
*/
PR_EXTERN(void) PR_DumpMemory(PRBool detailed);

/*
** Dump summary of objects allocated.
*/
PR_EXTERN(void) PR_DumpMemorySummary(void);

/*
** Dump the application heaps.
*/
PR_EXTERN(void) PR_DumpApplicationHeaps(void);

/*
** Helper function used by dump routines to do the indentation in a
** consistent fashion.
*/
PR_EXTERN(void) PR_DumpIndent(FILE *out, PRIntn indent);

/*
** The GCInfo structure contains all of the GC state...
**
** busyMemory:
**    The amount of GC heap memory that is busy at this instant. Busy
**    doesn't mean alive, it just means that it has been
**    allocated. Immediately after a collection busy means how much is
**    alive.
**
** freeMemory:
**    The amount of GC heap memory that is as yet unallocated.
**
** allocMemory:
**    The sum of free and busy memory in the GC heap.
**
** maxMemory:
**    The maximum size that the GC heap is allowed to grow.
**
** lowSeg:
**    The lowest segment currently used in the GC heap.
**
** highSeg:
**    The highest segment currently used in the GC heap.  
**    The lowSeg and highSeg members are used for a "quick test" of whether 
**    a pointer falls within the GC heap. [ see GC_IN_HEAP(...) ]
**
** lock:
**    Monitor used for syncronization within the GC.
**
** finalizer:
**    Thread in which the GC finalizer is running.
**
** liveBlock:
**    Object scanning functions call through this function pointer to
**    register a potential block of pointers with the collector. (This is
**    currently not at all different than processRoot.)
**
** livePointer:
**    Object scanning functions call through this function pointer to
**    register a single pointer with the collector.
**
** processRootBlock:
**    When a root finder identifies a root it should call through this
**    function pointer so that the GC can process the root. The call takes
**    a base address and count which the gc will examine for valid heap
**    pointers.
**
** processRootPointer:
**    When a root finder identifies a root it should call through this
**    function pointer so that the GC can process the root. The call takes
**    a single pointer value.
*/
typedef struct GCInfoStr {
    PRWord  flags;         /* trace flags (see below)               */
    PRWord  busyMemory;    /* memory in use right now               */
    PRWord  freeMemory;    /* memory free right now                 */
    PRWord  allocMemory;   /* sum of busy & free memory             */
    PRWord  maxMemory;     /* max memory we are allowed to allocate */
    PRWord *lowSeg;        /* lowest segment in the GC heap         */
    PRWord *highSeg;       /* higest segment in the GC heap         */

    PRMonitor *lock;
    PRThread  *finalizer;

    void (PR_CALLBACK *liveBlock)(void **base, PRInt32 count);
    void (PR_CALLBACK *livePointer)(void *ptr);
    void (PR_CALLBACK *processRootBlock)(void **base, PRInt32 count);
    void (PR_CALLBACK *processRootPointer)(void *ptr);
    FILE* dumpOutput;
#ifdef GCTIMINGHOOK
    void (*gcTimingHook)(int32 gcTime);
#endif
} GCInfo;

PR_EXTERN(GCInfo *) PR_GetGCInfo(void);
PR_EXTERN(PRBool) PR_GC_In_Heap(void GCPTR *object);

/*
** Simple bounds check to see if a pointer is anywhere near the GC heap.
** Used to avoid calls to PR_ProcessRoot and GCInfo.livePointer by object
** scanning code.
*/
#if !defined(XP_PC) || defined(_WIN32)
#define GC_IN_HEAP(_info, _p) (((PRWord*)(_p) >= (_info)->lowSeg) && \
                               ((PRWord*)(_p) <  (_info)->highSeg))
#else
/*
** The simple bounds check, above, doesn't work in Win16, because we don't
** maintain: lowSeg == MIN(all segments) and highSeg == MAX(all segments).
** So we have to do a little better.
*/
#define GC_IN_HEAP(_info, _p) PR_GC_In_Heap(_p)
#endif

PR_EXTERN(PRWord) PR_GetObjectHeader(void *ptr);

PR_EXTERN(PRWord) PR_SetObjectHeader(void *ptr, PRWord newUserBits);

/************************************************************************/

/* Trace flags (passed to PR_InitGC or in environment GCLOG) */
#define GC_TRACE    0x0001
#define GC_ROOTS    0x0002
#define GC_LIVE     0x0004
#define GC_ALLOC    0x0008
#define GC_MARK     0x0010
#define GC_SWEEP    0x0020
#define GC_DEBUG    0x0040
#define GC_FINAL    0x0080

#if defined(DEBUG_kipp) || defined(DEBUG_warren)
#define GC_CHECK    0x0100
#endif

#ifdef DEBUG
#define GCTRACE(x, y) if (PR_GetGCInfo()->flags & x) GCTrace y
PR_EXTERN(void) GCTrace(char *fmt, ...);
#else
#define GCTRACE(x, y)
#endif

PR_END_EXTERN_C

#endif /* prgc_h___ */
