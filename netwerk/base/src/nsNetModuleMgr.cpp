/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsNetModuleMgr.h"
#include "nsEnumeratorUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// The entry class
class nsNetModRegEntry {
public:
    nsINetNotify        *mNotify;
    nsIEventQueue       *mEventQ;
    nsString2            mTopic;
};

// Entry routines.
static PRBool (*nsVoidArrayEnumFunc)DeleteEntry(void *aElement, void *aData) {
    nsNetModRegEntry *entry = (nsNetModRegEntry*)aElement;
    // XXX clean it up
    delete entry;
}

PRBool CompareEntries(nsNetModRegEntry* a, nsNetModRegEntry* b) {
    if (a->mTopic != b->mTopic)
        return PR_FALSE;    
    if (a->mNotify != b->mNotify)
        return PR_FALSE;
    if (a->mEventQ != b->mEventQ)
        return PR_FALSE;
    return PR_TRUE;
}

PRBool CompareEntries(nsNetModRegEntry* aEntry, nsString2 aTopic, nsIEventQueue *mEventQ, nsINetNotify *aNotify) {
    if (aEntry->mTopic != aTopic)
        return PR_FALSE;
    if (aEntry->mNotify != aNotify)
        return PR_FALSE;
    if (aEntry->mEventQ != aEventQ)
        return PR_FALSE;
    return PR_TRUE;
}

///////////////////////////////////
//// nsISupports
///////////////////////////////////

NS_IMPL_ADDREF(nsNetModuleMgr);
NS_IMPL_RELEASE(nsNetModuleMgr);

NS_IMETHODIMP
nsNetModuleMgr::QueryInterface(REFNSIID iid, void** result)
{
    if (result == nsnull)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsINetModuleMgr::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsINetModuleMgr*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

///////////////////////////////////
//// nsINetModuleMgr
///////////////////////////////////

NS_IMETHODIMP
nsNetModuleMgr::RegisterModule(const char *aTopic, nsIEventQueue *aEventQueue, nsINetNotify *aNotify) {
    nsNetModRegEntry *newEntry = new nsNetModRegEntry;
    if (!newEntry)
        return NS_ERROR_OUT_OF_MEMORY;
    newEntry->mTopic = aTopic;
    newEntry->mEventQ = aEventQueue;
    NS_ADDREF(newEntry->mEventQ);
    newEntry->mNotify = aNotify;
    NS_ADDREF(newEntry->mNotify);

    // Check for a previous registration
    PR_Lock(mLock);
    PRInt8 cnt;
    mEntries->Count(&cnt);
    for (PRInt8 i = 0; i < cnt; i++) {
        if (CompareEntries(mEntries->ElementAt(i), newEntry) ) {
            delete newEntry;
            PR_Unlock(mLock);
            return NS_ERROR; // XXX should be more descriptive
        }
    }

    mEntries->AppendElement(newEntry);
    PR_Unlock(mLock);

    return NS_OK;
}

NS_IMETHODIMP
nsNetModuleMgr::UnregisterModule(const char *aTopic, nsIEventQueue *aEventQueue, nsINetNotify *aNotify) {

    PR_Lock(mLock);
    PRInt8 cnt;
    mEntries->Count(&cnt);
    for (PRInt32 i = 0; i < cnt; i++) {
        if (CompareEntries(mEntries->ElementAt(i), aTopic, aEventQueue, aNotify) ) {
            nsNetModRegEntry *entry= (nsNetModRegEntry*)mEntries->ElementAt(i);
            NS_RELEASE(entry->mEventQ);
            NS_RELEASE(entry->mNotify);
            delete entry;
        }
    }
    PR_Unlock(mLock);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModuleMgr::EnumerateModules(const char *aTopic, nsISimpleEnumerator **aEnumerator) {

    nsresult rv;
    PRUint32 cnt;

    // get all the entries for this topic
    
    rv = mEntries->Count(&cnt);
    if (NS_FAILED(rv))
        return rv;

    // create the new array
    nsISupportsArray *topicEntries = nsnull;
    rv = NS_NewISupportsArray(&topicEntries);
    if (NS_FAILED(rv))
        return rv;

    // run through the main entry array looking for topic matches.
    for (PRUint32 i = 0; i < cnt; i++) {
        nsISupports *entryS = nsnull;
        nsINetModRegEntry *entry = nsnull;
        entryS = mEntry[i];
        rv = entryS->QueryInterface(nsINetModRegEntry::GetIID(), (void**)&entry);
        if (NS_FAILED(rv))
            return rv;

        rv = entry->GetTopic(&topic);
        if (NS_FAILED(rv)) {
            NS_RELEASE(entry);
            return rv;
        }

        if (!PL_strcmp(aTopic, topic)) {
            // found a match, add it to the list
            rv = topicEntries->AppendElement(entryS);
            if (NS_FAILED(rv)) {
                NS_RELEASE(entry);
                return rv;
            }
        }
        NS_RELEASE(entry);
    }

    nsISimpleEnumerator *outEnum = nsnull;
    nsArrayEnumerator *arrEnum = new nsArrayEnumerator(topicEntries);
    NS_RELEASE(topicEntries);

    if (!arrEnum) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = arrEnum->QueryInterface(nsISimpleEnumerator::GetIID(), (void**)&outEnum);
    if (NS_FAILED(rv)) {
        delete arrEnum;
        return rv;
    }

    *aEnumerator = outEnum;

    return NS_OK;
}


///////////////////////////////////
//// nsNetModuleMgr
///////////////////////////////////

nsNetModuleMgr::nsNetModuleMgr() {
    NS_INIT_REFCNT();
    NS_NewISupportsArray(&mEntries);
    mLock    = PR_NewLock();
}

nsNetModuleMgr::~nsNetModuleMgr() {
    if (mEntries) {
        mEntries->EnumerateForwards(DeleteEntry, nsnull);
        NS_RELEASE(mEntries);
    }
    PR_DestroyLock(mLock);
}
