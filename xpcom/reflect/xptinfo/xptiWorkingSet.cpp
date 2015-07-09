/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of xptiWorkingSet. */

#include "mozilla/XPTInterfaceInfoManager.h"

#include "xptiprivate.h"
#include "nsString.h"

using namespace mozilla;

#define XPTI_STRUCT_ARENA_BLOCK_SIZE    (1024 * 16)
#define XPTI_HASHTABLE_LENGTH           1024

XPTInterfaceInfoManager::xptiWorkingSet::xptiWorkingSet()
    : mTableReentrantMonitor("xptiWorkingSet::mTableReentrantMonitor")
    , mIIDTable(XPTI_HASHTABLE_LENGTH)
    , mNameTable(XPTI_HASHTABLE_LENGTH)
{
    MOZ_COUNT_CTOR(xptiWorkingSet);

    gXPTIStructArena = XPT_NewArena(XPTI_STRUCT_ARENA_BLOCK_SIZE, sizeof(double),
                                    "xptiWorkingSet structs");
}

void
XPTInterfaceInfoManager::xptiWorkingSet::InvalidateInterfaceInfos()
{
    ReentrantMonitorAutoEnter monitor(mTableReentrantMonitor);
    for (auto iter = mNameTable.Iter(); !iter.Done(); iter.Next()) {
        xptiInterfaceEntry* entry = iter.GetUserData();
        entry->LockedInvalidateInterfaceInfo();
    }
}

XPTInterfaceInfoManager::xptiWorkingSet::~xptiWorkingSet()
{
    MOZ_COUNT_DTOR(xptiWorkingSet);

    // Only destroy the arena if we're doing leak stats. Why waste shutdown
    // time touching pages if we don't have to?
#ifdef NS_FREE_PERMANENT_DATA
    XPT_DestroyArena(gXPTIStructArena);
#endif
}

XPTArena* gXPTIStructArena;
