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

#include "nsSignonCoreFactory.h"

#include "nsAppCores.h"
#include "nsSignonCore.h"
#include "pratom.h"
#include "nsISupportsUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

//----------------------------------------------------------------------------------------
nsSignonCoreFactory::nsSignonCoreFactory()
//----------------------------------------------------------------------------------------
{
    NS_INIT_REFCNT();
    IncInstanceCount();
}

//----------------------------------------------------------------------------------------
nsSignonCoreFactory::~nsSignonCoreFactory()
//----------------------------------------------------------------------------------------
{
    DecInstanceCount();
}

NS_IMPL_ADDREF(nsSignonCoreFactory)
NS_IMPL_RELEASE(nsSignonCoreFactory)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCoreFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
//----------------------------------------------------------------------------------------
{
    if (aInstancePtr == NULL)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
        *aInstancePtr = (void*) this;
    else if ( aIID.Equals(kIFactoryIID) )
        *aInstancePtr = (void*) this;

    if (aInstancePtr == NULL)
        return NS_ERROR_NO_INTERFACE;

    AddRef();
    return NS_OK;
} // nsSignonCoreFactory::QueryInterface

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCoreFactory::CreateInstance(
    nsISupports* aOuter,
    REFNSIID aIID,
    void **aResult)
//----------------------------------------------------------------------------------------
{
    if (aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    *aResult = NULL;

#if 0
    // There is a stupid bug that I don't feel competent to fix. Basically, the
    // AppCore Manager is a waste of time (the manager gets created, an app core is
    // added to the list, and then the app core manager is deleted, along with its list).
    // I want to insist that
    // there is always one instance of the Signon app core. Hence this HACK.
    static nsSignonCore* inst = nsnull;
    PRBool firstTime = (inst == nsnull);
    if (firstTime)
    {
        inst = new nsSignonCore();
        if (!inst)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(inst);
    }
	nsresult result =  inst->QueryInterface(aIID, aResult);
	if (result != NS_OK && firstTime)
	    NS_RELEASE(inst);
#else
    static nsSignonCore* inst = new nsSignonCore();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
	nsresult result =  inst->QueryInterface(aIID, aResult);
#endif
    return result;
} // nsSignonCoreFactory::CreateInstance

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsSignonCoreFactory::LockFactory(PRBool aLock)
//----------------------------------------------------------------------------------------
{
    if (aLock)
        IncLockCount();
    else
        DecLockCount();
    return NS_OK;
} // nsSignonCoreFactory::LockFactory
