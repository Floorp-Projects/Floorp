/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

/*

   Maintains one-to-many mapping between element IDs and content
   nodes.

 */

#ifndef nsElementMap_h__
#define nsElementMap_h__

#include "nscore.h"
#include "nsError.h"
#include "plhash.h"
#include "nsIContent.h"

class nsString;
class nsISupportsArray;

class nsElementMap
{
private:
    PLHashTable* mMap;

    class ContentListItem {
    public:
        ContentListItem(nsIContent* aContent) : mNext(nsnull), mContent(aContent) {
            MOZ_COUNT_CTOR(XUL_nsElementMap_ContentListItem);
        }

        ~ContentListItem() {
            MOZ_COUNT_DTOR(XUL_nsElementMap_ContentListItem);
        }

        ContentListItem* mNext;
        nsCOMPtr<nsIContent> mContent;
    };

    static PLHashNumber PR_CALLBACK
    Hash(const void* akey);

    static PRIntn PR_CALLBACK
    Compare(const void* aLeft, const void* aRight);

    static PRIntn PR_CALLBACK
    ReleaseContentList(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure);

public:
    nsElementMap(void);
    virtual ~nsElementMap();

    nsresult
    Add(const nsString& aID, nsIContent* aContent);

    nsresult
    Remove(const nsString& aID, nsIContent* aContent);

    nsresult
    Find(const nsString& aID, nsISupportsArray* aResults);

    nsresult
    FindFirst(const nsString& aID, nsIContent** aContent);

    typedef PRIntn (*nsElementMapEnumerator)(const nsString& aID,
                                             nsIContent* aElement,
                                             void* aClosure);
    nsresult
    Enumerate(nsElementMapEnumerator aEnumerator, void* aClosure);

private:
    struct EnumerateClosure {
        nsElementMapEnumerator mEnumerator;
        void*                  mClosure;
    };
        
    static PRIntn PR_CALLBACK
    EnumerateImpl(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure);
};


#endif // nsElementMap_h__
