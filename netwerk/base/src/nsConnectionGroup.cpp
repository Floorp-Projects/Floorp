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

#include "nsConnectionGroup.h"
#include "nsIProtocolConnection.h"
#include "nscore.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsConnectionGroup::nsConnectionGroup()
    : mElements(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult
nsConnectionGroup::Init()
{
    nsresult rv = NS_NewISupportsArray(&mElements);
    return rv;
}

nsConnectionGroup::~nsConnectionGroup()
{
    NS_IF_RELEASE(mElements);
}

NS_IMPL_ADDREF(nsConnectionGroup);
NS_IMPL_RELEASE(nsConnectionGroup);

NS_IMETHODIMP
nsConnectionGroup::QueryInterface(REFNSIID iid, void** result)
{
    if (result == nsnull)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIConnectionGroup::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIConnectionGroup*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (iid.Equals(nsICancelable::GetIID())) {
        *result = NS_STATIC_CAST(nsICancelable*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (iid.Equals(nsICollection::GetIID())) {
        *result = NS_STATIC_CAST(nsICollection*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// nsICancelable methods:

NS_IMETHODIMP
nsConnectionGroup::Cancel(void)
{
    nsresult rv = NS_OK;
    PRUint32 cnt = mElements->Count();
    for (PRUint32 i = 0; i < cnt; i++) {
        nsIProtocolConnection* connection = 
            (nsIProtocolConnection*)(*mElements)[i];
        rv = connection->Cancel();
        if (NS_FAILED(rv)) break;
    }
    return rv;
}

NS_IMETHODIMP
nsConnectionGroup::Suspend(void)
{
    nsresult rv = NS_OK;
    PRUint32 cnt = mElements->Count();
    for (PRUint32 i = 0; i < cnt; i++) {
        nsIProtocolConnection* connection = 
            (nsIProtocolConnection*)(*mElements)[i];
        rv = connection->Suspend();
        if (NS_FAILED(rv)) break;
    }
    return rv;
}

NS_IMETHODIMP
nsConnectionGroup::Resume(void)
{
    nsresult rv = NS_OK;
    PRUint32 cnt = mElements->Count();
    for (PRUint32 i = 0; i < cnt; i++) {
        nsIProtocolConnection* connection = 
            (nsIProtocolConnection*)(*mElements)[i];
        rv = connection->Resume();
        if (NS_FAILED(rv)) break;
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsICollection methods:

NS_IMETHODIMP_(PRUint32) 
nsConnectionGroup::Count(void) const
{
    return mElements->Count();
}

NS_IMETHODIMP
nsConnectionGroup::AppendElement(nsISupports* connection)
{
    return mElements->AppendElement(connection);
}

NS_IMETHODIMP
nsConnectionGroup::RemoveElement(nsISupports* connection)
{
    // XXX RemoveElement returns a bool instead of nsresult!
    PRBool result = mElements->RemoveElement(connection);
    return result ? NS_OK : NS_ERROR_FAILURE;
}
NS_IMETHODIMP
nsConnectionGroup::Enumerate(nsIEnumerator* *result)
{
    return mElements->Enumerate(result);
}

NS_IMETHODIMP
nsConnectionGroup::Clear(void)
{
    return mElements->Clear();
}

////////////////////////////////////////////////////////////////////////////////
// nsIConnectionGroup methods:

NS_IMETHODIMP
nsConnectionGroup::AddChildGroup(nsIConnectionGroup* aGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsConnectionGroup::RemoveChildGroup(nsIConnectionGroup* aGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
