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
#include "nsXPComFactory.h"
#include "nsXPComCIID.h"
#include "nsAllocator.h"
#include "nsGenericFactory.h"

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

nsresult NS_NewEventQueueServiceFactory(nsIFactory** aResult);

/*
 * Define the global NSGetFactory(...) entry point for the xpcom DLL...
 */
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_EXPORT nsresult 
NSGetFactory_XPCOM_DLL(nsISupports* serviceMgr,
                       const nsCID &aClass,
                       const char *aClassName,
                       const char *aProgID,
                       nsIFactory **aFactory)
#else
extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
#endif
{
	if (NULL == aFactory) {
		return NS_ERROR_NULL_POINTER;
	}

	nsresult rv = NS_OK;
	if (aClass.Equals(kEventQueueServiceCID)) {
		rv = NS_NewEventQueueServiceFactory(aFactory);
	} else {
		// Use generic factories for the rest.
		nsGenericFactory* factory = NULL;
		if (aClass.Equals(nsAllocatorImpl::CID())) {
			factory = new nsGenericFactory(&nsAllocatorImpl::Create);
		} else if (aClass.Equals(nsGenericFactory::CID())) {
			// whoa, create a generic factory that creates generic factories!
			factory = new nsGenericFactory(&nsGenericFactory::Create);
		}
		if (factory != NULL) {
			factory->AddRef();
			*aFactory = factory;
		} else {
			rv = NS_ERROR_OUT_OF_MEMORY;
		}
	}
	return rv;
}
