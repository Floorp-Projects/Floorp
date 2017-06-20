/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        Entry(const nsACString& aURI, nsIAtom* aPrefix)
            : mURI(aURI), mPrefix(aPrefix), mNext(nullptr) {
            MOZ_COUNT_CTOR(nsNameSpaceMap::Entry); }

        ~Entry() { MOZ_COUNT_DTOR(nsNameSpaceMap::Entry); }
        
        nsCString mURI;
        nsCOMPtr<nsIAtom> mPrefix; 

        Entry* mNext;
    };

    nsNameSpaceMap();
    ~nsNameSpaceMap();

    nsresult
    Put(const nsAString& aURI, nsIAtom* aPrefix);

    nsresult
    Put(const nsACString& aURI, nsIAtom* aPrefix);

    class const_iterator {
    protected:
        friend class nsNameSpaceMap;

        explicit const_iterator(const Entry* aCurrent)
            : mCurrent(aCurrent) {}

        const Entry* mCurrent;

    public:
        const_iterator()
            : mCurrent(nullptr) {}

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

        bool
        operator==(const const_iterator& iter) const {
            return mCurrent == iter.mCurrent; }

        bool
        operator!=(const const_iterator& iter) const {
            return ! iter.operator==(*this); }
    };

    const_iterator first() const {
        return const_iterator(mEntries); }

    const_iterator last() const {
        return const_iterator(nullptr); }

    const_iterator GetNameSpaceOf(const nsACString& aURI) const;

protected:
    Entry* mEntries;
};


#endif // nsNameSpaceMap_h__
