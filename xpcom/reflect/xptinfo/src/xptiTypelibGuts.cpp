/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of xptiTypelibGuts. */

#include "xptiprivate.h"
#include "mozilla/XPTInterfaceInfoManager.h"

using namespace mozilla;

// static 
xptiTypelibGuts* 
xptiTypelibGuts::Create(XPTHeader* aHeader)
{
    NS_ASSERTION(aHeader, "bad param");
    void* place = XPT_MALLOC(gXPTIStructArena,
                             sizeof(xptiTypelibGuts) + 
                             (sizeof(xptiInterfaceEntry*) *
                              (aHeader->num_interfaces - 1)));
    if(!place)
        return nullptr;
    return new(place) xptiTypelibGuts(aHeader);
}

xptiInterfaceEntry*
xptiTypelibGuts::GetEntryAt(uint16_t i)
{
    static const nsID zeroIID =
        { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

    NS_ASSERTION(mHeader, "bad state");
    NS_ASSERTION(i < GetEntryCount(), "bad index");

    xptiInterfaceEntry* r = mEntryArray[i];
    if (r)
        return r;

    XPTInterfaceDirectoryEntry* iface = mHeader->interface_directory + i;

    XPTInterfaceInfoManager::xptiWorkingSet& set =
        XPTInterfaceInfoManager::GetSingleton()->mWorkingSet;

    {
        ReentrantMonitorAutoEnter monitor(set.mTableReentrantMonitor);
        if (iface->iid.Equals(zeroIID))
            r = set.mNameTable.Get(iface->name);
        else
            r = set.mIIDTable.Get(iface->iid);
    }

    if (r)
        SetEntryAt(i, r);

    return r;
}

const char*
xptiTypelibGuts::GetEntryNameAt(uint16_t i)
{
    NS_ASSERTION(mHeader, "bad state");
    NS_ASSERTION(i < GetEntryCount(), "bad index");

    XPTInterfaceDirectoryEntry* iface = mHeader->interface_directory + i;

    return iface->name;
}
