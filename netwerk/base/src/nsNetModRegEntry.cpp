/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsNetModRegEntry.h"


//////////////////////////////
//// nsISupports
//////////////////////////////
NS_IMPL_ISUPPORTS(nsNetModRegEntry, nsINetModRegEntry::GetIID());


//////////////////////////////
//// nsINetModRegEntry
//////////////////////////////

NS_IMETHODIMP
nsNetModRegEntry::GetMNotify(nsINetNotify **aNotify) {
    *aNotify = mNotify;
    NS_ADDREF(*aNotify);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::GetMEventQ(nsIEventQueue **aEventQ) {
    *aEventQ = mEventQ;
    NS_ADDREF(*aEventQ);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::GetMTopic(char **aTopic) {
    PL_strcpy(*aTopic, mTopic);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::GetMCID(nsCID **aMCID) {
    *aMCID = mCID;
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::Equals(nsINetModRegEntry* aEntry, PRBool *_retVal) {
    nsresult rv = NS_OK;
    PRBool retVal = PR_TRUE;

    NS_ADDREF(aEntry);

    char * topic = nsnull;
    rv = aEntry->GetMTopic(&topic);
    if (NS_FAILED(rv)) {
        retVal = PR_FALSE;
        goto end;
    }
    if (PL_strcmp(topic, mTopic)) {
        retVal = PR_FALSE;
        goto end;
    }

    nsINetNotify* notify = nsnull;
    rv = aEntry->GetMNotify(&notify);
    if (NS_FAILED(rv)) {
        retVal = PR_FALSE;
        goto end;
    }
    if (notify != mNotify) {
        retVal = PR_FALSE;
        goto end;
    }

    nsIEventQueue* eventQ = nsnull;
    rv = aEntry->GetMEventQ(&eventQ);
    if (NS_FAILED(rv)) {
        retVal = PR_FALSE;
        goto end;
    }
    if (eventQ != mEventQ) {
        retVal = PR_FALSE;
        goto end;
    }

    nsCID *cid = nsnull;
    rv = aEntry->GetMCID(&cid);
    if (NS_FAILED(rv)) {
        retVal = PR_FALSE;
        goto end;
    }
    if (!mCID->Equals(*cid)) {
        retVal = PR_FALSE;
        goto end;
    }

:end
    NS_IF_RELEASE(notify);
    NS_IF_RELEASE(eventQ);
    *_retVal = retVal;
    NS_RELEASE(aEntry);
    return rv;
}


//////////////////////////////
//// nsNetModRegEntry
//////////////////////////////

nsNetModRegEntry::nsNetModRegEntry(const char *aTopic, nsIEventQueue *aEventQ, nsINetNotify *aNotify, nsCID aCID)
    : mEventQ(aEventQ), mNotify(aNotify) {
    NS_INIT_REFCNT();
    PL_strcpy(mTopic, aTopic);
    NS_ADDREF(mEventQ);
    NS_ADDREF(mNotify);
    mCID = aCID;
}

nsNetModRegEntry::~nsNetModRegEntry() {
    delete [] mTopic;
    NS_RELEASE(mEventQ);
    NS_RELEASE(mNotify);
}