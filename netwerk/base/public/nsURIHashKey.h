/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Hashtable Keys.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Portions created by the Initial Developer are Copyright (C) 2003
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsURIHashKey_h__
#define nsURIHashKey_h__

#include "pldhash.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIURI.h"

/**
 * Hashtable key class to use with nsTHashtable/nsBaseHashtable
 */
class nsURIHashKey : public PLDHashEntryHdr
{
public:
    typedef nsIURI* KeyType;
    typedef const nsIURI* KeyTypePointer;

    nsURIHashKey(const nsIURI* aKey) :
        mKey(const_cast<nsIURI*>(aKey)) { MOZ_COUNT_CTOR(nsURIHashKey); }
    nsURIHashKey(const nsURIHashKey& toCopy) :
        mKey(toCopy.mKey) { MOZ_COUNT_CTOR(nsURIHashKey); }
    ~nsURIHashKey() { MOZ_COUNT_DTOR(nsURIHashKey); }

    nsIURI* GetKey() const { return mKey; }

    PRBool KeyEquals(const nsIURI* aKey) const {
        PRBool eq;
        if (NS_SUCCEEDED(mKey->Equals(const_cast<nsIURI*>(aKey), &eq))) {
            return eq;
        }
        return PR_FALSE;
    }

    static const nsIURI* KeyToPointer(nsIURI* aKey) { return aKey; }
    static PLDHashNumber HashKey(const nsIURI* aKey) {
        nsCAutoString spec;
        const_cast<nsIURI*>(aKey)->GetSpec(spec);
        return nsCRT::HashCode(spec.get());
    }
    
    enum { ALLOW_MEMMOVE = PR_TRUE };

protected:
    nsCOMPtr<nsIURI> mKey;
};

#endif // nsURIHashKey_h__
