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


NS_IMPL_ISUPPORTS(nsObserver, kIObserverIID);

NS_BASE nsresult NS_NewObserver(nsIObserver** anObserver)
{
    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    } 
    
    nsObserver* it = new nsObserver();

    if (it == 0) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return it->QueryInterface(kIObserverIID, (void **) anObserver);
}

nsObserver::nsObserver()
{
    NS_INIT_REFCNT();
}

nsObserver::~nsObserver(void)
{

}

nsresult nsObserver::Notify(nsISupports** result)
{
    return NS_OK;
    
}


////////////////////////////////////////////////////////////////////////////////
// nsObserverFactory Implementation

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsObserverFactory, kIFactoryIID);

nsObserverFactory::nsObserverFactory(void)
{
    NS_INIT_REFCNT();
}

nsObserverFactory::~nsObserverFactory(void)
{

}

nsresult
nsObserverFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;
    
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    nsIObserver* inst = nsnull;

    if (NS_FAILED(rv = NS_NewObserver(&inst)))
        return rv;

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(rv)) {
        *aResult = NULL;
    }
    return rv;
}

nsresult
nsObserverFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////////////
