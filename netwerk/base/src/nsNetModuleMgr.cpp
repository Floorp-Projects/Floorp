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
#include "nsNetModRegEntry.h"
#include "nsEnumeratorUtils.h" // for nsArrayEnumerator
#include "nsString2.h"
#include "nsIEventQueue.h"

// Entry routines.
static PRBool DeleteEntry(nsISupports *aElement, void *aData) {
    NS_ASSERTION(aElement, "null pointer");
    NS_RELEASE(aElement);
    return PR_TRUE;
}


///////////////////////////////////
//// nsISupports
///////////////////////////////////

NS_IMPL_ISUPPORTS(nsNetModuleMgr, nsCOMTypeInfo<nsINetModuleMgr>::GetIID());


///////////////////////////////////
//// nsINetModuleMgr
///////////////////////////////////

NS_IMETHODIMP
nsNetModuleMgr::RegisterModule(const char *aTopic, nsINetNotify *aNotify)
{
    nsresult rv;
    PRUint32 cnt;

    // XXX before registering an object for a particular topic
    // XXX QI the nsINetNotify interface passed in for the interfaces
    // XXX supported by the topic.

    PR_Lock(mLock);
    nsCOMPtr<nsINetModRegEntry> newEntryI;
    nsNetModRegEntry *newEntry = new nsNetModRegEntry(aTopic, aNotify, &rv);
    if (!newEntry)
        return NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv)) return rv;
    
    rv = newEntry->QueryInterface(nsCOMTypeInfo<nsINetModRegEntry>::GetIID(), getter_AddRefs(newEntryI));
    if (NS_FAILED(rv)) return rv;

    // Check for a previous registration
    mEntries->Count(&cnt);
    for (PRUint32 i = 0; i < cnt; i++) 
    {
        nsINetModRegEntry* curEntry = NS_STATIC_CAST(nsINetModRegEntry*, mEntries->ElementAt(i));
        PRBool same = PR_FALSE;
        rv = newEntryI->Equals(curEntry, &same);
        if (NS_FAILED(rv)) {
            PR_Unlock(mLock);
            return rv;
        }

        // if we've already got this one registered, yank it, and replace it with the new one
        if (same) {
            NS_RELEASE(curEntry);
            mEntries->DeleteElementAt(i);
            break;
        }
    }

    mEntries->AppendElement(NS_STATIC_CAST(nsISupports*, newEntryI));
    PR_Unlock(mLock);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModuleMgr::UnregisterModule(const char *aTopic, nsINetNotify *aNotify) 
{
    PR_Lock(mLock);
    nsresult rv;
    PRUint32 cnt;

    nsCOMPtr<nsINetModRegEntry> tmpEntryI;
    nsNetModRegEntry *tmpEntry = new nsNetModRegEntry(aTopic, aNotify, &rv);
    if (!tmpEntry)
        return NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv)) return rv;

    rv = tmpEntry->QueryInterface(nsCOMTypeInfo<nsINetModRegEntry>::GetIID(), getter_AddRefs(tmpEntryI));
    if (NS_FAILED(rv)) return rv;

    mEntries->Count(&cnt);
    for (PRUint32 i = 0; i < cnt; i++) {
        nsINetModRegEntry* curEntry = NS_STATIC_CAST(nsINetModRegEntry*, mEntries->ElementAt(i));
        NS_ADDREF(curEntry); // get our ref to it
        PRBool same = PR_FALSE;
        rv = tmpEntryI->Equals(curEntry, &same);
        if (NS_FAILED(rv)) {
            PR_Unlock(mLock);
            return rv;
        }
        if (same) {
            NS_RELEASE(curEntry);
            mEntries->DeleteElementAt(i);
            break;
        }
        NS_RELEASE(curEntry); // ditch our ref to it
    }
    PR_Unlock(mLock);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModuleMgr::EnumerateModules(const char *aTopic, nsISimpleEnumerator **aEnumerator) {

    nsresult rv;
    PRUint32 cnt;
    char *topic = nsnull;

    // get all the entries for this topic
    
    PR_Lock(mLock);
    rv = mEntries->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    // create the new array
    nsISupportsArray *topicEntries = nsnull;
    rv = NS_NewISupportsArray(&topicEntries);
    if (NS_FAILED(rv)) return rv;

    // run through the main entry array looking for topic matches.
    for (PRUint32 i = 0; i < cnt; i++) {
        nsINetModRegEntry *entry = NS_STATIC_CAST(nsINetModRegEntry*, mEntries->ElementAt(i));

        rv = entry->GetTopic(&topic);
        if (NS_FAILED(rv)) {
            NS_RELEASE(topicEntries);
            NS_RELEASE(entry);
            PR_Unlock(mLock);
            return rv;
        }

        if (!PL_strcmp(aTopic, topic)) {
            nsCRT::free(topic);
            topic = nsnull;
            // found a match, add it to the list
            rv = topicEntries->AppendElement(NS_STATIC_CAST(nsISupports*, entry));
            if (NS_FAILED(rv)) {
                NS_RELEASE(topicEntries);
                NS_RELEASE(entry);
                PR_Unlock(mLock);
                return rv;
            }
        }
        nsCRT::free(topic);
        topic = nsnull;
        NS_RELEASE(entry);
    }

    nsISimpleEnumerator *outEnum = nsnull;
    nsArrayEnumerator *arrEnum = new nsArrayEnumerator(topicEntries);
    NS_RELEASE(topicEntries);

    if (!arrEnum) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = arrEnum->QueryInterface(nsCOMTypeInfo<nsISimpleEnumerator>::GetIID(), (void**)&outEnum);
    if (NS_FAILED(rv)) {
        delete arrEnum;
        return rv;
    }
    *aEnumerator = outEnum;
    PR_Unlock(mLock);
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

NS_METHOD
nsNetModuleMgr::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    static nsNetModuleMgr* mgr = nsnull;
    if (!mgr) mgr = new nsNetModuleMgr();
    if (!mgr) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mgr);
    nsresult rv = mgr->QueryInterface(aIID, aResult);

    // don't release our ref as this is a singleton service.
    //NS_RELEASE(mgr);
    return rv;
}
