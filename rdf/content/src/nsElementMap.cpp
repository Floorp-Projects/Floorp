/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

   Maps IDs to elements.

 */

#include "nsCRT.h"
#include "nsElementMap.h"
#include "nsIContent.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gMapLog;
#endif

nsElementMap::nsElementMap()
{
    // Create a table for mapping IDs to elements in the content tree.
    static PRInt32 kInitialResourceTableSize = 1023;
    mMap = PL_NewHashTable(kInitialResourceTableSize,
                                 Hash,
                                 Compare,
                                 PL_CompareValues,
                                 nsnull,
                                 nsnull);

    NS_ASSERTION(mMap != nsnull, "could not create hash table for resources");

#ifdef PR_LOGGING
    if (! gMapLog)
        gMapLog = PR_NewLogModule("nsElementMap");


    PR_LOG(gMapLog, PR_LOG_ALWAYS,
           ("xulelemap(%p) created", this));
#endif
}

nsElementMap::~nsElementMap()
{
    if (mMap) {
        PL_HashTableEnumerateEntries(mMap, ReleaseContentList, nsnull);
        PL_HashTableDestroy(mMap);
    }

    PR_LOG(gMapLog, PR_LOG_ALWAYS,
           ("xulelemap(%p) destroyed", this));
}


PRIntn
nsElementMap::ReleaseContentList(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
    PRUnichar* id =
        NS_REINTERPRET_CAST(PRUnichar*, NS_CONST_CAST(void*, aHashEntry->key));

    delete[] id;
        
    ContentListItem* head =
        NS_REINTERPRET_CAST(ContentListItem*, aHashEntry->value);

    while (head) {
        ContentListItem* doomed = head;
        head = head->mNext;
        delete doomed;
    }

    return HT_ENUMERATE_NEXT;
}


nsresult
nsElementMap::Add(const nsString& aID, nsIContent* aContent)
{
    NS_PRECONDITION(mMap != nsnull, "not initialized");
    if (! mMap)
        return NS_ERROR_NOT_INITIALIZED;

    ContentListItem* head =
        (ContentListItem*) PL_HashTableLookup(mMap, aID.GetUnicode());

    if (! head) {
        head = new ContentListItem(aContent);
        if (! head)
            return NS_ERROR_OUT_OF_MEMORY;

        PRUnichar* key = new PRUnichar[aID.Length() + 1];
        if (! key)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCRT::memcpy(key, aID.GetUnicode(), aID.Length() * sizeof(PRUnichar));
        key[aID.Length()] = PRUnichar(0);

        PL_HashTableAdd(mMap, key, head);
        NS_ADDREF(aContent);
    }
    else {
        while (1) {
            if (head->mContent == aContent) {
                // This can happen if an element that was created via
                // frame construction code is then "appended" to the
                // content model with aNotify == PR_TRUE. If you see
                // this warning, it's an indication that you're
                // unnecessarily notifying the frame system, and
                // potentially causing unnecessary reflow.
                //NS_ERROR("element was already in the map");
                PR_LOG(gMapLog, PR_LOG_ALWAYS,
                       ("xulelemap(%p) dup    [%p] <-- %s\n",
                        this, aContent, (const char*) nsCAutoString(aID)));

                return NS_OK;
            }
            if (! head->mNext)
                break;

            head = head->mNext;
        }

        head->mNext = new ContentListItem(aContent);
        if (! head->mNext)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(aContent);
    }

    PR_LOG(gMapLog, PR_LOG_ALWAYS,
           ("xulelemap(%p) add    [%p] <-- %s\n",
            this, aContent, (const char*) nsCAutoString(aID)));


    return NS_OK;
}


nsresult
nsElementMap::Remove(const nsString& aID, nsIContent* aContent)
{
    NS_PRECONDITION(mMap != nsnull, "not initialized");
    if (! mMap)
        return NS_ERROR_NOT_INITIALIZED;

    PR_LOG(gMapLog, PR_LOG_ALWAYS,
           ("xulelemap(%p) remove [%p] <-- %s\n",
            this, aContent, (const char*) nsCAutoString(aID)));

    PLHashEntry** hep = PL_HashTableRawLookup(mMap,
                                              Hash(aID.GetUnicode()),
                                              aID.GetUnicode());

    // XXX Don't comment out this assert: if you get here, something
    // has gone dreadfully, horribly wrong. Curse. Scream. File a bug
    // against waterson@netscape.com.
    NS_ASSERTION(hep != nsnull && *hep != nsnull, "attempt to remove an element that was never added");
    if (!hep || !*hep)
        return NS_ERROR_ILLEGAL_VALUE;

    ContentListItem* head = NS_REINTERPRET_CAST(ContentListItem*, (*hep)->value);

    if (head->mContent == aContent) {
        NS_RELEASE(aContent);
        ContentListItem* next = head->mNext;
        if (next) {
            (*hep)->value = next;
        }
        else {
            // It was the last reference in the table
            PRUnichar* key = NS_REINTERPRET_CAST(PRUnichar*, NS_CONST_CAST(void*, (*hep)->key));
            PL_HashTableRawRemove(mMap, hep, *hep);
            delete[] key;
        }
        delete head;
    }
    else {
        ContentListItem* item = head->mNext;
        while (item) {
            if (item->mContent == aContent) {
                head->mNext = item->mNext;
                NS_RELEASE(aContent);
                delete item;
                break;
            }
            head = item;
            item = item->mNext;
        }
    }

    return NS_OK;
}



nsresult
nsElementMap::Find(const nsString& aID, nsISupportsArray* aResults)
{
    NS_PRECONDITION(mMap != nsnull, "not initialized");
    if (! mMap)
        return NS_ERROR_NOT_INITIALIZED;

    aResults->Clear();
    ContentListItem* item =
        NS_REINTERPRET_CAST(ContentListItem*, PL_HashTableLookup(mMap, aID.GetUnicode()));

    while (item) {
        aResults->AppendElement(item->mContent);
        item = item->mNext;
    }
    return NS_OK;
}


nsresult
nsElementMap::FindFirst(const nsString& aID, nsIContent** aResult)
{
    NS_PRECONDITION(mMap != nsnull, "not initialized");
    if (! mMap)
        return NS_ERROR_NOT_INITIALIZED;

    ContentListItem* item =
        NS_REINTERPRET_CAST(ContentListItem*, PL_HashTableLookup(mMap, aID.GetUnicode()));

    if (item) {
        *aResult = item->mContent;
        NS_ADDREF(*aResult);
    }
    else {
        *aResult = nsnull;
    }

    return NS_OK;
}

nsresult
nsElementMap::Enumerate(nsElementMapEnumerator aEnumerator, void* aClosure)
{
    EnumerateClosure closure = { aEnumerator, aClosure };
    PL_HashTableEnumerateEntries(mMap, EnumerateImpl, &closure);
    return NS_OK;
}


PRIntn
nsElementMap::EnumerateImpl(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
    EnumerateClosure* closure = NS_REINTERPRET_CAST(EnumerateClosure*, aClosure);

    const PRUnichar* id =
        NS_REINTERPRET_CAST(const PRUnichar*, aHashEntry->key);

    ContentListItem** link = 
        NS_REINTERPRET_CAST(ContentListItem**, &aHashEntry->value);

    ContentListItem* item = *link;

    while (item) {
        PRIntn result = (*closure->mEnumerator)(id, item->mContent, closure->mClosure);

        if (result == HT_ENUMERATE_REMOVE) {
            NS_RELEASE(item->mContent);
            *link = item->mNext;

            if ((! *link) && (link == NS_REINTERPRET_CAST(ContentListItem**, &aHashEntry->value))) {
                PRUnichar* key = NS_CONST_CAST(PRUnichar*, id);
                delete[] key;
                return HT_ENUMERATE_REMOVE;
            }
        }
        else {
            link = &item->mNext;
        }

        item = item->mNext;
    }

    return HT_ENUMERATE_NEXT;
}


PLHashNumber
nsElementMap::Hash(const void* aKey)
{
    PLHashNumber result = 0;
    const PRUnichar* s = NS_REINTERPRET_CAST(const PRUnichar*, aKey);
    while (*s != nsnull) {
        result = (result >> 28) ^ (result << 4) ^ *s;
        ++s;
    }
    return result;
}


PRIntn
nsElementMap::Compare(const void* aLeft, const void* aRight)
{
    return 0 == nsCRT::strcmp(NS_REINTERPRET_CAST(const PRUnichar*, aLeft),
                              NS_REINTERPRET_CAST(const PRUnichar*, aRight));
}
