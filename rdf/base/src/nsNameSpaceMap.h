/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
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
        Entry(const nsCSubstring& aURI, nsIAtom* aPrefix)
            : mURI(aURI), mPrefix(aPrefix), mNext(nsnull) {
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
    Put(const nsCSubstring& aURI, nsIAtom* aPrefix);

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

    const_iterator GetNameSpaceOf(const nsCSubstring& aURI) const;

protected:
    Entry* mEntries;
};


#endif // nsNameSpaceMap_h__
