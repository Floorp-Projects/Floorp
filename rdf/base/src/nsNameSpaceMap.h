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

#ifndef nsNameSpaceMap_h__
#define nsNameSpaceMap_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"

class nsNameSpaceMap
{
public:
    class Entry {
    public:
        Entry(const nsAString& aURI, nsIAtom* aPrefix)
            : mURI(aURI), mPrefix(aPrefix), mNext(nsnull) {
            MOZ_COUNT_CTOR(nsNameSpaceMap::Entry); }

        ~Entry() { MOZ_COUNT_DTOR(nsNameSpaceMap::Entry); }
        
        nsString mURI;
        nsCOMPtr<nsIAtom> mPrefix; 

        Entry* mNext;
    };

    nsNameSpaceMap();
    ~nsNameSpaceMap();

    nsresult
    Put(const nsAString& aURI, nsIAtom* aPrefix);

    class const_iterator {
    protected:
        friend class nsNameSpaceMap;

        const_iterator(const Entry* aCurrent)
            : mCurrent(aCurrent) {}

        const Entry* mCurrent;

    public:
        const_iterator()
            : mCurrent(nsnull) {}

        const_iterator(const const_iterator& iter)
            : mCurrent(iter.mCurrent) {}

        const_iterator&
        operator=(const const_iterator& iter) {
            mCurrent = iter.mCurrent;
            return *this; }

        const_iterator&
        operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        const_iterator
        operator++(int) {
            const_iterator tmp(*this);
            mCurrent = mCurrent->mNext;
            return tmp; }

        const Entry* operator->() const { return mCurrent; }

        const Entry& operator*() const { return *mCurrent; }

        PRBool
        operator==(const const_iterator& iter) const {
            return mCurrent == iter.mCurrent; }

        PRBool
        operator!=(const const_iterator& iter) const {
            return ! iter.operator==(*this); }
    };

    const_iterator first() const {
        return const_iterator(mEntries); }

    const_iterator last() const {
        return const_iterator(nsnull); }

    const_iterator GetNameSpaceOf(const nsAString& aURI) const;

protected:
    Entry* mEntries;
};


#endif // nsNameSpaceMap_h__
