/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsAppCoresManagerFactory.h"
#include "nsAppCoresManager.h"
#include "nsAppCores.h"
#include "pratom.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

/////////////////////////////////////////////////////////////////////////
// nsAppCoresImplFactory
/////////////////////////////////////////////////////////////////////////

nsAppCoresManagerFactory::nsAppCoresManagerFactory(void)
{
    mRefCnt=0;
    IncInstanceCount();
}

nsAppCoresManagerFactory::~nsAppCoresManagerFactory(void)
{
    IncInstanceCount();
}



NS_IMETHODIMP 
nsAppCoresManagerFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*) this;
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = (void*) this;
    }

    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMETHODIMP
nsAppCoresManagerFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsAppCoresManagerFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsAppCoresManagerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsAppCoresManager *inst = new nsAppCoresManager();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;

}

NS_IMETHODIMP
nsAppCoresManagerFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        IncLockCount();
    else
        DecLockCount();

    return NS_OK;
}
