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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsAutoLock.h"
#include "nsNetModuleMgr.h"
#include "nsNetModRegEntry.h"
#include "nsEnumeratorUtils.h" // for nsArrayEnumerator
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIEventQueue.h"

nsNetModuleMgr* nsNetModuleMgr::gManager;

///////////////////////////////////
//// nsISupports
///////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNetModuleMgr, nsINetModuleMgr);


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

    nsAutoMonitor mon(mMonitor);
    nsNetModRegEntry *newEntry = new nsNetModRegEntry(aTopic, aNotify, &rv);
    if (!newEntry)
        return NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv)) {
        delete newEntry;
        return rv;
    }

    nsCOMPtr<nsINetModRegEntry> newEntryI = do_QueryInterface(newEntry, &rv);
    if (NS_FAILED(rv)) {
        delete newEntry;
        return rv;
    }

    // Check for a previous registration
    mEntries->Count(&cnt);
    for (PRUint32 i = 0; i < cnt; i++) 
    {
        nsCOMPtr<nsINetModRegEntry> curEntry =
            dont_AddRef(NS_STATIC_CAST(nsINetModRegEntry*, mEntries->ElementAt(i)));

        PRBool same = PR_FALSE;
        rv = newEntryI->Equals(curEntry, &same);
        if (NS_FAILED(rv)) return rv;

        // if we've already got this one registered, yank it, and replace it with the new one
        if (same) {
            mEntries->DeleteElementAt(i);
            break;
        }
    }

    rv = mEntries->AppendElement(NS_STATIC_CAST(nsISupports*, newEntryI)) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
    return rv;
}

NS_IMETHODIMP
nsNetModuleMgr::UnregisterModule(const char *aTopic, nsINetNotify *aNotify) 
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;

    nsCOMPtr<nsINetModRegEntry> tmpEntryI;
    nsNetModRegEntry *tmpEntry = new nsNetModRegEntry(aTopic, aNotify, &rv);
    if (!tmpEntry)
        return NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv)) return rv;

    rv = tmpEntry->QueryInterface(NS_GET_IID(nsINetModRegEntry), getter_AddRefs(tmpEntryI));
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    mEntries->Count(&cnt);
    for (PRUint32 i = 0; i < cnt; i++) {
        nsCOMPtr<nsINetModRegEntry> curEntry = 
            dont_AddRef(NS_STATIC_CAST(nsINetModRegEntry*, mEntries->ElementAt(i)));

        PRBool same = PR_FALSE;
        rv = tmpEntryI->Equals(curEntry, &same);
        if (NS_FAILED(rv)) return rv;

        if (same) {
            mEntries->DeleteElementAt(i);
            break;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNetModuleMgr::EnumerateModules(const char *aTopic, nsISimpleEnumerator **aEnumerator) {

    nsresult rv;
    // get all the entries for this topic
    
    nsAutoMonitor mon(mMonitor);

    PRUint32 cnt;
    rv = mEntries->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    // create the new array
    nsCOMPtr<nsISupportsArray> topicEntries;
    rv = NS_NewISupportsArray(getter_AddRefs(topicEntries));
    if (NS_FAILED(rv)) return rv;

    // run through the main entry array looking for topic matches.
    for (PRUint32 i = 0; i < cnt; i++) {
        nsCOMPtr<nsINetModRegEntry> entry = 
            dont_AddRef(NS_STATIC_CAST(nsINetModRegEntry*, mEntries->ElementAt(i)));

        nsXPIDLCString topic;
        rv = entry->GetTopic(getter_Copies(topic));
        if (NS_FAILED(rv)) return rv;

        if (0 == PL_strcmp(aTopic, topic)) {
            // found a match, add it to the list
            rv = topicEntries->AppendElement(NS_STATIC_CAST(nsISupports*, entry)) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
            if (NS_FAILED(rv)) return rv;
        }
    }

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = NS_NewArrayEnumerator(getter_AddRefs(enumerator), topicEntries);
    if (NS_FAILED(rv)) return rv;

    *aEnumerator = enumerator;
    NS_ADDREF(*aEnumerator);
    return NS_OK;
}


///////////////////////////////////
//// nsNetModuleMgr
///////////////////////////////////

nsNetModuleMgr::nsNetModuleMgr() {
    NS_INIT_REFCNT();
    NS_NewISupportsArray(&mEntries);
    mMonitor = nsAutoMonitor::NewMonitor("nsNetModuleMgr");
}

nsNetModuleMgr::~nsNetModuleMgr() {
    NS_IF_RELEASE(mEntries);

    nsAutoMonitor::DestroyMonitor(mMonitor);
    gManager = nsnull;
}

NS_METHOD
nsNetModuleMgr::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    if (! gManager) {
        gManager = new nsNetModuleMgr();
        if (! gManager)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(gManager);
    nsresult rv = gManager->QueryInterface(aIID, aResult);
    NS_RELEASE(gManager);

    return rv;
}
