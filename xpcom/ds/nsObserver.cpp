/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS
#include "pratom.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIObserver.h"
#include "nsObserver.h"

static NS_DEFINE_CID(kObserverCID, NS_OBSERVER_CID);

////////////////////////////////////////////////////////////////////////////////
// nsObserver Implementation


NS_IMPL_AGGREGATED(nsObserver)

NS_COM nsresult NS_NewObserver(nsIObserver** anObserver, nsISupports* outer)
{
    return nsObserver::Create(outer, NS_GET_IID(nsIObserver), (void**)anObserver);
}

NS_METHOD
nsObserver::Create(nsISupports* outer, const nsIID& aIID, void* *anObserver)
{
    NS_ENSURE_ARG_POINTER(anObserver);
	 NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsObserver* it = new nsObserver(outer);
    if (it == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = it->AggregatedQueryInterface(aIID, anObserver);
    if (NS_FAILED(rv)) {
        delete it;
        return rv;
    }
    return rv;
}

nsObserver::nsObserver(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

nsObserver::~nsObserver(void)
{
}

NS_IMETHODIMP
nsObserver::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

    if (aIID.Equals(NS_GET_IID(nsISupports)))
	     *aInstancePtr = GetInner();
	 else if(aIID.Equals(NS_GET_IID(nsIObserver)))
	     *aInstancePtr = NS_STATIC_CAST(nsIObserver*, this);
	 else {
	     *aInstancePtr = nsnull;
		  return NS_NOINTERFACE;
	 }

	 NS_ADDREF((nsISupports*)*aInstancePtr);
	 return NS_OK;
}

NS_IMETHODIMP
nsObserver::Observe( nsISupports *, const PRUnichar *, const PRUnichar * ) {
    nsresult rv = NS_OK;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
