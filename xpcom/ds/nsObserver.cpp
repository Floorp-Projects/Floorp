/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#define NS_IMPL_IDS
#include "pratom.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsRepository.h"
#include "nsIObserver.h"
#include "nsObserver.h"
#include "nsString.h"

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

    if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	     *aInstancePtr = GetInner();
	 else if(aIID.Equals(nsIObserver::GetIID()))
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
