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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//////////////////////////////
//// nsISupports
//////////////////////////////
NS_IMPL_ISUPPORTS(nsNetModRegEntry, nsINetModRegEntry::GetIID());

NS_IMETHODIMP
nsNetModRegEntry::QueryInterface(REFNSIID iid, void** result) {
    if (result == nsnull)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsINetModRegEntry::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsINetModRegEntry*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

//////////////////////////////
//// nsINetModRegEntry
//////////////////////////////

NS_IMETHODIMP
nsNetModRegEntry::GetmNotify(nsINetNotify **aNotify) {
    *aNotify = mNotify;
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::GetmEventQ(nsIEventQueue **aEventQ) {
    *aEventQ = mEventQ;
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::GetmTopic(char **aTopic) {
    PL_strcpy(*aTopic, mTopic);
    return NS_OK;
}

//////////////////////////////
//// nsNetModRegEntry
//////////////////////////////

nsNetModRegEntry::nsNetModRegEntry(char *aTopic, nsIEventQueue *aEventQ, nsINetNotify *aNotify)
    : mEventQ(aEventQ), mNotify(aNotify) {
    NS_INIT_REFCNT();
    PL_strcpy(mTopic, aTopic);
    NS_ADDREF(mEventQ);
    NS_ADDREF(mNotify);
}

nsNetModRegEntry::~nsNetModRegEntry() {
    delete [] mTopic;
    NS_RELEASE(mEventQ);
    NS_RELEASE(mNotify);
}