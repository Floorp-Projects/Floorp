/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsGenericFactory.h"

nsGenericFactory::nsGenericFactory(ConstructorProcPtr constructor)
	: mConstructor(constructor), mDestructor(NULL)
{
	NS_INIT_ISUPPORTS();
}

nsGenericFactory::~nsGenericFactory()
{
	if (mDestructor != NULL)
		(*mDestructor) ();
}

NS_IMPL_ISUPPORTS2(nsGenericFactory, nsIGenericFactory, nsIFactory)

NS_IMETHODIMP nsGenericFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
	return mConstructor(aOuter, aIID, aResult);
}

NS_IMETHODIMP nsGenericFactory::LockFactory(PRBool aLock)
{
	return NS_OK;
}

NS_IMETHODIMP nsGenericFactory::SetConstructor(ConstructorProcPtr constructor)
{
	mConstructor = constructor;
	return NS_OK;
}

NS_IMETHODIMP nsGenericFactory::SetDestructor(DestructorProcPtr destructor)
{
	mDestructor = destructor;
	return NS_OK;
}

NS_METHOD nsGenericFactory::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
	// sorry, aggregation not spoken here.
	nsresult res = NS_ERROR_NO_AGGREGATION;
	if (outer == NULL) {
		nsGenericFactory* factory = new nsGenericFactory;
		if (factory != NULL) {
			res = factory->QueryInterface(aIID, aInstancePtr);
			if (res != NS_OK)
				delete factory;
		} else {
			res = NS_ERROR_OUT_OF_MEMORY;
		}
	}
	return res;
}

NS_COM nsresult
NS_NewGenericFactory(nsIGenericFactory* *result,
                     nsIGenericFactory::ConstructorProcPtr constructor,
                     nsIGenericFactory::DestructorProcPtr destructor)
{
    nsresult rv;
    nsIGenericFactory* fact;
    rv = nsGenericFactory::Create(NULL, nsIGenericFactory::GetIID(), (void**)&fact);
    if (NS_FAILED(rv)) return rv;
    rv = fact->SetConstructor(constructor);
    if (NS_FAILED(rv)) goto error;
    rv = fact->SetDestructor(destructor);
    if (NS_FAILED(rv)) goto error;
    *result = fact;
    return rv;

  error:
    NS_RELEASE(fact);
    return rv;
}

