/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef DEBUG

#include "plhash.h"
#include "prprf.h"
#include "prlock.h"
#include "prthread.h"
#include "nsDebug.h"
#include "nsVoidArray.h"

#include "mozilla/BlockingResourceBase.h"
#include "mozilla/CondVar.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"

#ifdef NS_TRACE_MALLOC
# include <stdio.h>
# include "nsTraceMalloc.h"
#endif

static PRUintn      ResourceAcqnChainFrontTPI = (PRUintn)-1;
static PLHashTable* OrderTable = 0;
static PRLock*      OrderTableLock = 0;
static PRLock*      ResourceMutex = 0;

// Needs to be kept in sync with BlockingResourceType. */
static char const *const kResourceTypeNames[] = {
    "Mutex", "Monitor", "CondVar"
};

struct nsNamedVector : public nsVoidArray {
    const char* mName;

#ifdef NS_TRACE_MALLOC
    // Callsites for the inner locks/monitors stored in our base nsVoidArray.
    // This array parallels our base nsVoidArray.
    nsVoidArray mInnerSites;
#endif

    nsNamedVector(const char* name = 0, PRUint32 initialSize = 0)
        : nsVoidArray(initialSize),
          mName(name)
    {
    }
};

static void *
_hash_alloc_table(void *pool, PRSize size)
{
    return operator new(size);
}

static void 
_hash_free_table(void *pool, void *item)
{
    operator delete(item);
}

static PLHashEntry *
_hash_alloc_entry(void *pool, const void *key)
{
    return new PLHashEntry;
}

/*
 * Because monitors and locks may be associated with an mozilla::BlockingResourceBase,
 * without having had their associated nsNamedVector created explicitly in
 * nsAutoMonitor::NewMonitor/DeleteMonitor, we need to provide a freeEntry
 * PLHashTable hook, to avoid leaking nsNamedVectors which are replaced by
 * nsAutoMonitor::NewMonitor.
 *
 * There is still a problem with the OrderTable containing orphaned
 * nsNamedVector entries, for manually created locks wrapped by nsAutoLocks.
 * (there should be no manually created monitors wrapped by nsAutoMonitors:
 * you should use nsAutoMonitor::NewMonitor and nsAutoMonitor::DestroyMonitor
 * instead of PR_NewMonitor and PR_DestroyMonitor).  These lock vectors don't
 * strictly leak, as they are killed on shutdown, but there are unnecessary
 * named vectors in the hash table that outlive their associated locks.
 *
 * XXX so we should have nsLock, nsMonitor, etc. and strongly type their
 * XXX nsAutoXXX counterparts to take only the non-auto types as inputs
 */
static void 
_hash_free_entry(void *pool, PLHashEntry *entry, PRUintn flag)
{
    nsNamedVector* vec = (nsNamedVector*) entry->value;
    if (vec) {
        entry->value = 0;
        delete vec;
    }
    if (flag == HT_FREE_ENTRY)
        delete entry;
}

static const PLHashAllocOps _hash_alloc_ops = {
    _hash_alloc_table, _hash_free_table,
    _hash_alloc_entry, _hash_free_entry
};

static PRIntn
_purge_one(PLHashEntry* he, PRIntn cnt, void* arg)
{
    nsNamedVector* vec = (nsNamedVector*) he->value;

    if (he->key == arg)
        return HT_ENUMERATE_REMOVE;
    vec->RemoveElement(arg);
    return HT_ENUMERATE_NEXT;
}

static void
OnResourceRecycle(void* aResource)
{
    NS_PRECONDITION(OrderTable, "should be inited!");

    PR_Lock(OrderTableLock);
    PL_HashTableEnumerateEntries(OrderTable, _purge_one, aResource);
    PR_Unlock(OrderTableLock);
}

static PLHashNumber
_hash_pointer(const void* key)
{
    return PLHashNumber(NS_PTR_TO_INT32(key)) >> 2;
}

// TODO just included for function below
#include "prcmon.h"

// Must be single-threaded here, early in primordial thread.
static void InitAutoLockStatics()
{
    (void) PR_NewThreadPrivateIndex(&ResourceAcqnChainFrontTPI, 0);
    OrderTable = PL_NewHashTable(64, _hash_pointer,
                                 PL_CompareValues, PL_CompareValues,
                                 &_hash_alloc_ops, 0);
    if (OrderTable && !(OrderTableLock = PR_NewLock())) {
        PL_HashTableDestroy(OrderTable);
        OrderTable = 0;
    }

    if (OrderTable && !(ResourceMutex = PR_NewLock())) {
        PL_HashTableDestroy(OrderTable);
        OrderTable = 0;
    }

    // TODO unnecessary after API changes
    PR_CSetOnMonitorRecycle(OnResourceRecycle);
}

/* TODO re-enable this after API change.  with backwards compatibility
 enabled, it conflicts with the impl in nsAutoLock.cpp.  Not freeing
 this stuff will "leak" memory that is cleanup up when the process
 exits. */
#if 0
void _FreeAutoLockStatics()
{
    PLHashTable* table = OrderTable;
    if (!table) return;

    // Called at shutdown, so we don't need to lock.
    // TODO unnecessary after API changes
    PR_CSetOnMonitorRecycle(0);
    PR_DestroyLock(OrderTableLock);
    OrderTableLock = 0;
    PL_HashTableDestroy(table);
    OrderTable = 0;
}
#endif


static nsNamedVector* GetVector(PLHashTable* table, const void* key)
{
    PLHashNumber hash = _hash_pointer(key);
    PLHashEntry** hep = PL_HashTableRawLookup(table, hash, key);
    PLHashEntry* he = *hep;
    if (he)
        return (nsNamedVector*) he->value;
    nsNamedVector* vec = new nsNamedVector();
    if (vec)
        PL_HashTableRawAdd(table, hep, hash, key, vec);
    return vec;
}

static void OnResourceCreated(const void* key, const char* name )
{
    NS_PRECONDITION(key && OrderTable, "should be inited!");

    nsNamedVector* value = new nsNamedVector(name);
    if (value) {
        PR_Lock(OrderTableLock);
        PL_HashTableAdd(OrderTable, key, value);
        PR_Unlock(OrderTableLock);
    }
}

// We maintain an acyclic graph in OrderTable, so recursion can't diverge.
static PRBool Reachable(PLHashTable* table, const void* goal, const void* start)
{
    PR_ASSERT(goal);
    PR_ASSERT(start);
    nsNamedVector* vec = GetVector(table, start);
    for (PRUint32 i = 0, n = vec->Count(); i < n; i++) {
        void* aResource = vec->ElementAt(i);
        if (aResource == goal || Reachable(table, goal, aResource))
            return PR_TRUE;
    }
    return PR_FALSE;
}

static PRBool WellOrdered(const void* aResource1, const void* aResource2,
                          const void *aCallsite2, PRUint32* aIndex2p,
                          nsNamedVector** aVec1p, nsNamedVector** aVec2p)
{
    NS_ASSERTION(OrderTable && OrderTableLock, "supposed to be initialized!");

    PRBool rv = PR_TRUE;
    PLHashTable* table = OrderTable;

    PR_Lock(OrderTableLock);

    // Check whether we've already asserted (addr1 < addr2).
    nsNamedVector* vec1 = GetVector(table, aResource1);
    if (vec1) {
        PRUint32 i, n;

        for (i = 0, n = vec1->Count(); i < n; i++)
            if (vec1->ElementAt(i) == aResource2)
                break;

        if (i == n) {
            // Now check for (addr2 < addr1) and return false if so.
            nsNamedVector* vec2 = GetVector(table, aResource2);
            if (vec2) {
                for (i = 0, n = vec2->Count(); i < n; i++) {
                    void* aResourcei = vec2->ElementAt(i);
                    PR_ASSERT(aResourcei);
                    if (aResourcei == aResource1 
                        || Reachable(table, aResource1, aResourcei)) {
                        *aIndex2p = i;
                        *aVec1p = vec1;
                        *aVec2p = vec2;
                        rv = PR_FALSE;
                        break;
                    }
                }

                if (rv) {
                    // Insert (addr1 < addr2) into the order table.
                    // XXX fix plvector/nsVector to use const void*
                    vec1->AppendElement((void*) aResource2);
#ifdef NS_TRACE_MALLOC
                    vec1->mInnerSites.AppendElement((void*) aCallsite2);
#endif
                }
            }
        }
    }

    // all control flow must pass through this point
//unlock:
    PR_Unlock(OrderTableLock);
    return rv;
}

//
// BlockingResourceBase implementation
//
// Note that in static/member functions, user code abiding by the 
// BlockingResourceBase API's contract gives us the following guarantees:
//
//  - mResource is not NULL
//  - mResource points to a valid underlying resource
//  - mResource is NOT shared with any other mozilla::BlockingResourceBase
//
// If user code violates the API contract, the behavior of the following
// functions is undefined.  So no error checking is strictly necessary.
//
// That said, assertions of the the above facts are sprinkled throughout the
// following code, to check for errors in user code.
//
namespace mozilla {


void
BlockingResourceBase::Init(void* aResource, 
                           const char* aName, 
                           BlockingResourceBase::BlockingResourceType aType)
{
    if (NS_UNLIKELY(ResourceAcqnChainFrontTPI == PRUintn(-1)))
        InitAutoLockStatics();

    NS_PRECONDITION(aResource, "null resource");

    OnResourceCreated(aResource, aName);
    mResource = aResource;
    mChainPrev = 0;
    mType = aType;
}

BlockingResourceBase::~BlockingResourceBase()
{
    NS_PRECONDITION(mResource, "bad subclass impl, or double free");

    // we don't expect to find this resouce in the acquisition chain.
    // it should have been Release()'n as many times as it has been 
    // Acquire()ed, before this destructor was called.
    // additionally, WE are the only ones who can create underlying 
    // resources, so there should be one underlying instance per 
    // BlockingResourceBase.  thus we don't need to do any cleanup unless 
    // client code has done something seriously and obviously wrong.
    // that, plus the potential performance impact of full cleanup, mean
    // that it has been removed for now.

    OnResourceRecycle(mResource);
    mResource = 0;
    mChainPrev = 0;
}

void BlockingResourceBase::Acquire()
{
    NS_PRECONDITION(mResource, "bad base class impl or double free");
    PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(ResourceMutex);

    if (mType == eCondVar) {
        NS_NOTYETIMPLEMENTED("TODO: figure out how to Acquire() condvars");
        return;
    }

    BlockingResourceBase* chainFront = 
        (BlockingResourceBase*) 
            PR_GetThreadPrivate(ResourceAcqnChainFrontTPI);

    if (eMonitor == mType) {
        // reentering a monitor: the old implementation ensured that this
        // was only done immediately after a previous entry.  little
        // tricky here since we can't rely on the monitor already having
        // been entered, as AutoMonitor used to let us do (sort of)

        if (this == chainFront) {
            // acceptable reentry, and nothing to update.
            return;
        } 
        else if (chainFront) {
            BlockingResourceBase* br = chainFront->mChainPrev;
            while (br) {
                if (br == this) {
                    NS_ASSERTION(br != this, 
                                 "reentered monitor after acquiring "
                                 "other resources");
                    // have to ignore this allocation and return.  even if
                    // we cleaned up the old acquisition of the monitor and
                    // put the new one at the front of the chain, we'd
                    // almost certainly set off the deadlock detector.
                    // this error is interesting enough.
                    return;
                }
                br = br->mChainPrev;
            }
        }
    }

    const void* callContext =
#ifdef NS_TRACE_MALLOC
        (const void*)NS_TraceMallocGetStackTrace();
#else
        nsnull
#endif
        ;

    if (eMutex == mType
        && chainFront == this
        && !(chainFront->mChainPrev)) {
        // corner case: acquire only a single lock, then try to reacquire 
        // the lock.  there's no entry in the order table for the first
        // acquisition, so we might not detect this (immediate and real!) 
        // deadlock.
        //
        // XXX need to remove this corner case, and get a stack trace for 
        // first acquisition, and get the lock's name
        char buf[128];
        PR_snprintf(buf, sizeof buf,
                    "Imminent deadlock between Mutex@%p and itself!",
                    chainFront->mResource);

#ifdef NS_TRACE_MALLOC
        fputs("\nDeadlock will occur here:\n", stderr);
        NS_TraceMallocPrintStackTrace(
            stderr, (nsTMStackTraceIDStruct*)callContext);
        putc('\n', stderr);
#endif

        NS_ERROR(buf);

        return;                 // what else to do? we're screwed
    }

    nsNamedVector* vec1;
    nsNamedVector* vec2;
    PRUint32 i2;

    if (!chainFront 
        || WellOrdered(chainFront->mResource, mResource, 
                       callContext, 
                       &i2, 
                       &vec1, &vec2)) {
        mChainPrev = chainFront;
        PR_SetThreadPrivate(ResourceAcqnChainFrontTPI, this);
    }
    else {
        char buf[128];
        PR_snprintf(buf, sizeof buf,
                    "Potential deadlock between %s%s@%p and %s%s@%p",
                    vec1->mName ? vec1->mName : "",
                    kResourceTypeNames[chainFront->mType],
                    chainFront->mResource,
                    vec2->mName ? vec2->mName : "",
                    kResourceTypeNames[mType],
                    mResource);

#ifdef NS_TRACE_MALLOC
        fprintf(stderr, "\n*** %s\n\nStack of current acquisition:\n", buf);
        NS_TraceMallocPrintStackTrace(
            stderr, NS_TraceMallocGetStackTrace());

        fputs("\nStack of conflicting acquisition:\n", stderr);
        NS_TraceMallocPrintStackTrace(
            stderr, (nsTMStackTraceIDStruct *)vec2->mInnerSites.ElementAt(i2));
        putc('\n', stderr);
#endif

        NS_ERROR(buf);

        // because we know the latest acquisition set off the deadlock,
        // detector, it's debatable whether we should append it to the 
        // acquisition chain; it might just trigger later errors that 
        // have already been reported here.  I choose not too add it.
    }
}

void BlockingResourceBase::Release()
{
    NS_PRECONDITION(mResource, "bad base class impl or double free");
    PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(ResourceMutex);

    if (mType == eCondVar) {
        NS_NOTYETIMPLEMENTED("TODO: figure out how to Release() condvars");
        return;
    }

    BlockingResourceBase* chainFront = 
        (BlockingResourceBase*) 
            PR_GetThreadPrivate(ResourceAcqnChainFrontTPI);
    NS_ASSERTION(chainFront, 
                 "Release()ing something that hasn't been Acquire()ed");

    if (NS_LIKELY(chainFront == this)) {
        PR_SetThreadPrivate(ResourceAcqnChainFrontTPI, mChainPrev);
    }
    else {
        // not an error, but makes code hard to reason about.
        NS_WARNING("you're releasing a resource in non-LIFO order; why?");

        // we walk backwards in order of acquisition:
        //  (1)  ...node<-prev<-curr...
        //              /     /
        //  (2)  ...prev<-curr...
        BlockingResourceBase* curr = chainFront;
        BlockingResourceBase* prev = nsnull;
        while (curr && (prev = curr->mChainPrev) && (prev != this))
            curr = prev;
        if (prev == this)
            curr->mChainPrev = prev->mChainPrev;
    }
}

//
// Debug implementation of Mutex
void
Mutex::Lock()
{
    PR_Lock(ResourceMutex);
    Acquire();
    PR_Unlock(ResourceMutex);

    PR_Lock(mLock);
}

void
Mutex::Unlock()
{
    PRStatus status = PR_Unlock(mLock);
    NS_ASSERTION(PR_SUCCESS == status, "problem Unlock()ing");

    PR_Lock(ResourceMutex);
    Release();
    PR_Unlock(ResourceMutex);
}

//
// Debug implementation of Monitor
void 
Monitor::Enter() 
{
    PR_Lock(ResourceMutex);
    ++mEntryCount;
    Acquire();
    PR_Unlock(ResourceMutex);

    PR_EnterMonitor(mMonitor);
}

void
Monitor::Exit()
{
    PRStatus status = PR_ExitMonitor(mMonitor);
    NS_ASSERTION(PR_SUCCESS == status, "bad ExitMonitor()");

    PR_Lock(ResourceMutex);
    if (--mEntryCount == 0)
        Release();
    PR_Unlock(ResourceMutex);
}


} // namespace mozilla


#endif // ifdef DEBUG
