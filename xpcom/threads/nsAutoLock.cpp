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
#include "prthread.h"
#include "nsDebug.h"
#include "nsVector.h"

static PRUintn      LockStackTPI = (PRUintn)-1;
static PLHashTable* OrderTable = 0;

static const char* LockTypeNames[] = {"Lock", "Monitor", "CMonitor"};

struct nsNamedVector : public nsVector {
    const char* mName;

    nsNamedVector(const char* name, PRUint32 initialSize = 0)
        : nsVector(initialSize),
          mName(name)
    {
    }
};

PR_STATIC_CALLBACK(PLHashNumber)
_hash_pointer(const void *key)
{
    return PLHashNumber(key) >> 2;
}

static nsNamedVector* GetVector(PLHashTable* table, const void* key)
{
    PLHashNumber hash = _hash_pointer(key);
    PLHashEntry** hep = PL_HashTableRawLookup(table, hash, key);
    PLHashEntry* he = *hep;
    if (he)
        return (nsNamedVector*) he->value;
    nsNamedVector* vec = new nsNamedVector(0, 1);
    if (vec)
        PL_HashTableRawAdd(table, hep, hash, key, vec);
    return vec;
}

static PRBool WellOrdered(const void* addr1, const void* addr2,
                          nsNamedVector** vec1p, nsNamedVector** vec2p)
{
    PLHashTable* table = OrderTable;
    if (!table) return PR_TRUE;

    // Check whether we've already asserted (addr1 < addr2).
    nsNamedVector* vec1 = GetVector(table, addr1);
    if (!vec1) return PR_TRUE;
    for (PRUint32 i = 0, n = vec1->GetSize(); i < n; i++)
        if (vec1->Get(i) == addr2)
            return PR_TRUE;

    // Now check for (addr2 < addr1) and return false if so.
    nsNamedVector* vec2 = GetVector(table, addr2);
    if (!vec2) return PR_TRUE;
    for (PRUint32 i = 0, n = vec2->GetSize(); i < n; i++)
        if (vec2->Get(i) == addr1) {
            *vec1p = vec1;
            *vec2p = vec2;
            return PR_FALSE;
        }

    // Assert (addr1 < addr2) into the order table.
    // XXX fix plvector/nsVector to use const void*
    vec1->Add((void*) addr2);
    return PR_TRUE;
}

nsAutoLockBase::nsAutoLockBase(void* addr, nsAutoLockType type)
{
    if (LockStackTPI == PRUintn(-1)) {
        (void) PR_NewThreadPrivateIndex(&LockStackTPI, 0);
        OrderTable = PL_NewHashTable(64, _hash_pointer,
                                     PL_CompareValues, PL_CompareValues,
                                     0, 0);
    }
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
                char buf[200];
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
        nsNamedVector* value = new nsNamedVector(name, 0);
        if (value)
            PL_HashTableAdd(OrderTable, mon, value);
    }
#endif
    return mon;
}

#ifdef DEBUG
PR_STATIC_CALLBACK(PRIntn)
_purge_mon(PLHashEntry *he, PRIntn cnt, void* arg)
{
    nsNamedVector* vec = (nsNamedVector*) he->value;

    if (he->key == arg)
        return HT_ENUMERATE_REMOVE;
    for (PRUint32 i = 0, n = vec->GetSize(); i < n; i++) {
        if (vec->Get(i) == arg) {
            vec->Remove(i);
            break;
        }
    }
    return HT_ENUMERATE_NEXT;
}
#endif

void nsAutoMonitor::DestroyMonitor(PRMonitor* mon)
{
#ifdef DEBUG
    if (OrderTable)
        PL_HashTableEnumerateEntries(OrderTable, _purge_mon, mon);
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
