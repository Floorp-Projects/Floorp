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



#include "nsProfileCoreFactory.h"



#include "nsAppCores.h"

#include "nsProfileCore.h"

#include "pratom.h"

#include "nsISupportsUtils.h"



static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);



//----------------------------------------------------------------------------------------

nsProfileCoreFactory::nsProfileCoreFactory()

//----------------------------------------------------------------------------------------

{

    NS_INIT_REFCNT();

    IncInstanceCount();

}



//----------------------------------------------------------------------------------------

nsProfileCoreFactory::~nsProfileCoreFactory()

//----------------------------------------------------------------------------------------

{

    DecInstanceCount();

}



NS_IMPL_ADDREF(nsProfileCoreFactory)

NS_IMPL_RELEASE(nsProfileCoreFactory)



//----------------------------------------------------------------------------------------

NS_IMETHODIMP nsProfileCoreFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)

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

} // nsProfileCoreFactory::QueryInterface



//----------------------------------------------------------------------------------------

NS_IMETHODIMP nsProfileCoreFactory::CreateInstance(

    nsISupports* aOuter,

    REFNSIID aIID,

    void **aResult)

//----------------------------------------------------------------------------------------

{

    if (aResult == NULL)

        return NS_ERROR_NULL_POINTER;



    *aResult = NULL;



    nsProfileCore* inst = new nsProfileCore();

    if (!inst)

        return NS_ERROR_OUT_OF_MEMORY;

	nsresult result =  inst->QueryInterface(aIID, aResult);



	if (result != NS_OK)

		delete inst;



	return result;

} // nsProfileCoreFactory::CreateInstance



//----------------------------------------------------------------------------------------

NS_IMETHODIMP nsProfileCoreFactory::LockFactory(PRBool aLock)

//----------------------------------------------------------------------------------------

{

    if (aLock)

        IncLockCount();

    else

        DecLockCount();

    return NS_OK;

} // nsProfileCoreFactory::LockFactory

