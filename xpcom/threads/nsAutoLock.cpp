/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAutoLock.h"

#ifdef DEBUG

#include "plhash.h"
#include "prprf.h"
#include "prlock.h"
#include "prthread.h"
#include "nsDebug.h"
#include "nsVoidArray.h"

static PRUintn      LockStackTPI = (PRUintn)-1;
static PLHashTable* OrderTable = 0;
static PRLock*      OrderTableLock = 0;

static const char* LockTypeNames[] = {"Lock", "Monitor", "CMonitor"};

struct nsNamedVector : public nsVoidArray {
    const char* mName;

    nsNamedVector(const char* name = 0, PRUint32 initialSize = 0)
        : nsVoidArray(initialSize),
          mName(name)
    {
    }
};

static void * PR_CALLBACK
_hash_alloc_table(void *pool, PRSize size)
{
    return operator new(size);
}

static void  PR_CALLBACK
_hash_free_table(void *pool, void *item)
{
    operator delete(item);
}

static PLHashEntry * PR_CALLBACK
_hash_alloc_entry(void *pool, const void *key)
{
    return new PLHashEntry;
}

/*
 * Because monitors and locks may be associated with an nsAutoLockBase,
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
static void  PR_CALLBACK
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

static PLHashAllocOps _hash_alloc_ops = {
    _hash_alloc_table, _hash_free_table,
    _hash_alloc_entry, _hash_free_entry
};

PR_STATIC_CALLBACK(PRIntn)
_purge_one(PLHashEntry* he, PRIntn cnt, void* arg)
{
    nsNamedVector* vec = (nsNamedVector*) he->value;

    if (he->key == arg)
        return HT_ENUMERATE_REMOVE;
    vec->RemoveElement(arg);
    return HT_ENUMERATE_NEXT;
}

PR_STATIC_CALLBACK(void)
OnMonitorRecycle(void* addr)
{
    PR_Lock(OrderTableLock);
    PL_HashTableEnumerateEntries(OrderTable, _purge_one, addr);
    PR_Unlock(OrderTableLock);
}

PR_STATIC_CALLBACK(PLHashNumber)
_hash_pointer(const void* key)
{
    return PLHashNumber(NS_PTR_TO_INT32(key)) >> 2;
}

// Must be single-threaded here, early in primordial thread.
static void InitAutoLockStatics()
{
    (void) PR_NewThreadPrivateIndex(&LockStackTPI, 0);
    OrderTable = PL_NewHashTable(64, _hash_pointer,
                                 PL_CompareValues, PL_CompareValues,
                                 &_hash_alloc_ops, 0);
    if (OrderTable && !(OrderTableLock = PR_NewLock())) {
        PL_HashTableDestroy(OrderTable);
        OrderTable = 0;
    }
    PR_CSetOnMonitorRecycle(OnMonitorRecycle);
}

void _FreeAutoLockStatics()
{
    PLHashTable* table = OrderTable;
    if (!table) return;

    // Called at shutdown, so we don't need to lock.
    PR_CSetOnMonitorRecycle(0);
    PR_DestroyLock(OrderTableLock);
    OrderTableLock = 0;
    PL_HashTableDestroy(table);
    OrderTable = 0;
}

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

// We maintain an acyclic graph in order table, so recursion can't diverge
static PRBool Reachable(PLHashTable* table, const void* goal, const void* start)
{
    PR_ASSERT(goal);
    PR_ASSERT(start);
    nsNamedVector* vec = GetVector(table, start);
    for (PRUint32 i = 0, n = vec->Count(); i < n; i++) {
        void* addr = vec->ElementAt(i);
        if (addr == goal || Reachable(table, goal, addr))
            return PR_TRUE;
    }
    return PR_FALSE;
}

static PRBool WellOrdered(const void* addr1, const void* addr2,
                          nsNamedVector** vec1p, nsNamedVector** vec2p)
{
    PRBool rv = PR_TRUE;
    PLHashTable* table = OrderTable;
    if (!table) return rv;
    PR_Lock(OrderTableLock);

    PRUint32 i, n;

    // Check whether we've already asserted (addr1 < addr2).
    nsNamedVector* vec1 = GetVector(table, addr1);
    if (vec1) {
        for (i = 0, n = vec1->Count(); i < n; i++)
            if (vec1->ElementAt(i) == addr2)
                break;

        if (i == n) {
            // Now check for (addr2 < addr1) and return false if so.
            nsNamedVector* vec2 = GetVector(table, addr2);
            if (vec2) {
                for (i = 0, n = vec2->Count(); i < n; i++) {
                    void* addri = vec2->ElementAt(i);
                    PR_ASSERT(addri);
                    if (addri == addr1 || Reachable(table, addr1, addri)) {
                        *vec1p = vec1;
                        *vec2p = vec2;
                        rv = PR_FALSE;
                        break;
                    }
                }

                if (rv) {
                    // Assert (addr1 < addr2) into the order table.
                    // XXX fix plvector/nsVector to use const void*
                    vec1->AppendElement((void*) addr2);
                }
            }
        }
    }

    PR_Unlock(OrderTableLock);
    return rv;
}

nsAutoLockBase::nsAutoLockBase(void* addr, nsAutoLockType type)
{
    if (LockStackTPI == PRUintn(-1))
        InitAutoLockStatics();
    nsAutoLockBase* stackTop =
        (nsAutoLockBase*) PR_GetThreadPrivate(LockStackTPI);
    if (stackTop) {
        if (stackTop->mAddr == addr) {
            // Ignore reentry: it's legal for monitors, and NSPR will assert
            // if you reenter a PRLock.
        } else {
            nsNamedVector* vec1;
            nsNamedVector* vec2;
            if (!WellOrdered(stackTop->mAddr, addr, &vec1, &vec2)) {
                char buf[128];
                PR_snprintf(buf, sizeof buf,
                            "potential deadlock between %s%s@%p and %s%s@%p",
                            vec1->mName ? vec1->mName : "",
                            LockTypeNames[stackTop->mType],
                            stackTop->mAddr,
                            vec2->mName ? vec2->mName : "",
                            LockTypeNames[type],
                            addr);
                NS_ERROR(buf);
            }
        }
    }
    mAddr = addr;
    mDown = stackTop;
    mType = type;
    (void) PR_SetThreadPrivate(LockStackTPI, this);
}

nsAutoLockBase::~nsAutoLockBase()
{
    (void) PR_SetThreadPrivate(LockStackTPI, mDown);
}

#endif /* DEBUG */

PRMonitor* nsAutoMonitor::NewMonitor(const char* name)
{
    PRMonitor* mon = PR_NewMonitor();
#ifdef DEBUG
    if (mon && OrderTable) {
        nsNamedVector* value = new nsNamedVector(name);
        if (value) {
            PR_Lock(OrderTableLock);
            PL_HashTableAdd(OrderTable, mon, value);
            PR_Unlock(OrderTableLock);
        }
    }
#endif
    return mon;
}

void nsAutoMonitor::DestroyMonitor(PRMonitor* mon)
{
#ifdef DEBUG
    if (OrderTable)
        OnMonitorRecycle(mon);
#endif
    PR_DestroyMonitor(mon);
}

void nsAutoMonitor::Enter()
{
#ifdef DEBUG
    nsAutoLockBase* stackTop =
        (nsAutoLockBase*) PR_GetThreadPrivate(LockStackTPI);
    NS_ASSERTION(stackTop == mDown, "non-LIFO nsAutoMonitor::Enter");
    mDown = stackTop;
    (void) PR_SetThreadPrivate(LockStackTPI, this);
#endif
    PR_EnterMonitor(mMonitor);
    mLockCount += 1;
}

void nsAutoMonitor::Exit()
{
#ifdef DEBUG
    (void) PR_SetThreadPrivate(LockStackTPI, mDown);
#endif
    PRStatus status = PR_ExitMonitor(mMonitor);
    NS_ASSERTION(status == PR_SUCCESS, "PR_ExitMonitor failed");
    mLockCount -= 1;
}

// XXX we don't worry about cached monitors being destroyed behind our back.
// XXX current NSPR (mozilla/nsprpub/pr/src/threads/prcmon.c) never destroys
// XXX a cached monitor! potential resource pig in conjunction with necko...

void nsAutoCMonitor::Enter()
{
#ifdef DEBUG
    nsAutoLockBase* stackTop =
        (nsAutoLockBase*) PR_GetThreadPrivate(LockStackTPI);
    NS_ASSERTION(stackTop == mDown, "non-LIFO nsAutoCMonitor::Enter");
    mDown = stackTop;
    (void) PR_SetThreadPrivate(LockStackTPI, this);
#endif
    PR_CEnterMonitor(mLockObject);
    mLockCount += 1;
}

void nsAutoCMonitor::Exit()
{
#ifdef DEBUG
    (void) PR_SetThreadPrivate(LockStackTPI, mDown);
#endif
    PRStatus status = PR_CExitMonitor(mLockObject);
    NS_ASSERTION(status == PR_SUCCESS, "PR_CExitMonitor failed");
    mLockCount -= 1;
}
