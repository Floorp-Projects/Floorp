/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#include "nsAutoLock.h"

#ifdef DEBUG

#include "plhash.h"
#include "prprf.h"
#include "prlock.h"
#include "prthread.h"
#include "nsDebug.h"
#include "nsVector.h"

static PRUintn      LockStackTPI = (PRUintn)-1;
static PLHashTable* OrderTable = 0;
static PRLock*      OrderTableLock = 0;

static const char* LockTypeNames[] = {"Lock", "Monitor", "CMonitor"};

struct nsNamedVector : public nsVector {
    const char* mName;

    nsNamedVector(const char* name = 0, PRUint32 initialSize = 0)
        : nsVector(initialSize),
          mName(name)
    {
    }
};

PR_STATIC_CALLBACK(PRIntn)
_purge_one(PLHashEntry* he, PRIntn cnt, void* arg)
{
    nsNamedVector* vec = (nsNamedVector*) he->value;

    if (he->key == arg) {
        he->value = 0;
        delete vec;
        return HT_ENUMERATE_REMOVE;
    }
    for (PRUint32 i = 0, n = vec->GetSize(); i < n; i++) {
        if (vec->Get(i) == arg) {
            vec->Remove(i);
            break;
        }
    }
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
    return PLHashNumber(key) >> 2;
}

// Must be single-threaded here, early  in primordial thread.
static void InitAutoLockStatics()
{
    (void) PR_NewThreadPrivateIndex(&LockStackTPI, 0);
    OrderTable = PL_NewHashTable(64, _hash_pointer,
                                 PL_CompareValues, PL_CompareValues,
                                 0, 0);
    if (OrderTable && !(OrderTableLock = PR_NewLock())) {
        PL_HashTableDestroy(OrderTable);
        OrderTable = 0;
    }
    PR_CSetOnMonitorRecycle(OnMonitorRecycle);
}

PR_STATIC_CALLBACK(PRIntn)
_purge_all(PLHashEntry* he, PRIntn cnt, void* arg)
{
    nsNamedVector* vec = (nsNamedVector*) he->value;
    he->value = 0;
    delete vec;
    return HT_ENUMERATE_REMOVE;
}

void _FreeAutoLockStatics()
{
    PLHashTable* table = OrderTable;
    if (!table) return;

    // Called at shutdown, so we don't need to lock.
    PR_CSetOnMonitorRecycle(0);
    PR_DestroyLock(OrderTableLock);
    OrderTableLock = 0;
    PL_HashTableEnumerateEntries(table, _purge_all, 0);
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
    for (PRUint32 i = 0, n = vec->GetSize(); i < n; i++) {
        void* addr = vec->Get(i);
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
        for (i = 0, n = vec1->GetSize(); i < n; i++)
            if (vec1->Get(i) == addr2)
                break;

        if (i == n) {
            // Now check for (addr2 < addr1) and return false if so.
            nsNamedVector* vec2 = GetVector(table, addr2);
            if (vec2) {
                for (i = 0, n = vec2->GetSize(); i < n; i++) {
                    void* addri = vec2->Get(i);
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
                    vec1->Add((void*) addr2);
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
}

void nsAutoMonitor::Exit()
{
#ifdef DEBUG
    (void) PR_SetThreadPrivate(LockStackTPI, mDown);
#endif
    PRStatus status = PR_ExitMonitor(mMonitor);
    NS_ASSERTION(status == PR_SUCCESS, "PR_ExitMonitor failed");
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
}

void nsAutoCMonitor::Exit()
{
#ifdef DEBUG
    (void) PR_SetThreadPrivate(LockStackTPI, mDown);
#endif
    PRStatus status = PR_CExitMonitor(mLockObject);
    NS_ASSERTION(status == PR_SUCCESS, "PR_CExitMonitor failed");
}
