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

static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
static NS_DEFINE_IID(kObserverCID, NS_OBSERVER_CID);

////////////////////////////////////////////////////////////////////////////////
// nsObserver Implementation


NS_IMPL_AGGREGATED(nsObserver)

NS_COM nsresult NS_NewObserver(nsIObserver** anObserver, nsISupports* outer)
{
    return nsObserver::Create(outer, kIObserverIID, (void**)anObserver);
}

NS_METHOD
nsObserver::Create(nsISupports* outer, const nsIID& aIID, void* *anObserver)
{
    if (anObserver == NULL)
        return NS_ERROR_NULL_POINTER;

    nsObserver* it = new nsObserver(outer);

    if (it == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsISupports* inner = outer ? it->GetInner() : it;
    nsresult rv = inner->QueryInterface(aIID, anObserver);
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
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIObserver::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = (nsIObserver*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsObserver::Observe( nsISupports *, const PRUnichar *, const PRUnichar * ) {
    nsresult rv = NS_OK;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
