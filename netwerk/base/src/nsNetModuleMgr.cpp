/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAutoLock.h"
#include "nsNetModuleMgr.h"
#include "nsArrayEnumerator.h" // for nsArrayEnumerator
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIEventQueue.h"
#include "nsCRT.h"

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
    PRInt32 cnt;

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
    cnt = mEntries.Count();
    for (PRInt32 i = 0; i < cnt; i++) 
    {
        nsINetModRegEntry* curEntry = mEntries[i];

        PRBool same = PR_FALSE;
        rv = newEntryI->Equals(curEntry, &same);
        if (NS_FAILED(rv)) return rv;

        // if we've already got this one registered, yank it, and replace it with the new one
        if (same) {
            mEntries.RemoveObjectAt(i);
            break;
        }
    }

    if (!mEntries.AppendObject(newEntryI))
        return NS_ERROR_FAILURE;
    return NS_OK;
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

    PRInt32 cnt;
    cnt = mEntries.Count();
    for (PRInt32 i = 0; i < cnt; i++) {
        nsINetModRegEntry* curEntry = mEntries[i];

        PRBool same = PR_FALSE;
        rv = tmpEntryI->Equals(curEntry, &same);
        if (NS_FAILED(rv)) return rv;

        if (same) {
            mEntries.RemoveObjectAt(i);
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

    PRInt32 cnt = mEntries.Count();

    // create the new array
    nsCOMArray<nsINetModRegEntry> topicEntries;

    // run through the main entry array looking for topic matches.
    for (PRInt32 i = 0; i < cnt; i++) {
        nsINetModRegEntry* entry = mEntries[i];

        nsXPIDLCString topic;
        rv = entry->GetTopic(getter_Copies(topic));
        if (NS_FAILED(rv)) return rv;

        if (0 == PL_strcmp(aTopic, topic)) {
            // found a match, add it to the list
            if (!topicEntries.AppendObject(entry))
                return NS_ERROR_FAILURE;
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
    mMonitor = nsAutoMonitor::NewMonitor("nsNetModuleMgr");
}

nsNetModuleMgr::~nsNetModuleMgr() {
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
