/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    nsCString uriUTF8;
    AppendUTF16toUTF8(aURI, uriUTF8);
    return Put(uriUTF8, aPrefix);
}

nsresult
nsNameSpaceMap::Put(const nsCSubstring& aURI, nsIAtom* aPrefix)
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
nsNameSpaceMap::GetNameSpaceOf(const nsCSubstring& aURI) const
{
    for (Entry* entry = mEntries; entry != nsnull; entry = entry->mNext) {
        if (StringBeginsWith(aURI, entry->mURI))
            return const_iterator(entry);
    }

    return last();
}
