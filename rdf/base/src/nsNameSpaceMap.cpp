/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

#include "nsNameSpaceMap.h"
#include "nsReadableUtils.h"

nsNameSpaceMap::nsNameSpaceMap()
    : mEntries(nsnull)
{
    MOZ_COUNT_CTOR(nsNameSpaceMap);
}

nsNameSpaceMap::~nsNameSpaceMap()
{
    MOZ_COUNT_DTOR(nsNameSpaceMap);

    while (mEntries) {
        Entry* doomed = mEntries;
        mEntries = mEntries->mNext;
        delete doomed;
    }
}

nsresult
nsNameSpaceMap::Put(const nsAString& aURI, nsIAtom* aPrefix)
{
    Entry* entry;

    // Make sure we're not adding a duplicate
    for (entry = mEntries; entry != nsnull; entry = entry->mNext) {
        if (entry->mURI == aURI || entry->mPrefix == aPrefix)
            return NS_ERROR_FAILURE;
    }

    entry = new Entry(aURI, aPrefix);
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mNext = mEntries;
    mEntries = entry;
    return NS_OK;
}

nsNameSpaceMap::const_iterator
nsNameSpaceMap::GetNameSpaceOf(const nsAString& aURI) const
{
    for (Entry* entry = mEntries; entry != nsnull; entry = entry->mNext) {
        if (StringBeginsWith(aURI, entry->mURI))
            return const_iterator(entry);
    }

    return last();
}
