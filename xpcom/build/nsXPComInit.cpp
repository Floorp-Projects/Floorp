/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsIRegistry.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsObserverService.h"
#include "nsObserver.h"
#include "nsProperties.h"

#include "nsAllocator.h"
#include "nsArena.h"
#include "nsBuffer.h"
#include "nsByteBuffer.h"
#include "nsPageMgr.h"
#include "nsSupportsArray.h"
#include "nsSupportsPrimitives.h"
#include "nsUnicharBuffer.h"

#include "nsComponentManager.h"
#include "nsIServiceManager.h"
#include "nsGenericFactory.h"

#include "nsEventQueueService.h"
#include "nsEventQueue.h"

#include "nsProxyObjectManager.h"
#include "nsProxyEventPrivate.h"  // access to the impl of nsProxyObjectManager for the generic factory registration.

#include "nsFileSpecImpl.h"

// base
static NS_DEFINE_CID(kAllocatorCID, NS_ALLOCATOR_CID);
// ds
static NS_DEFINE_CID(kArenaCID, NS_ARENA_CID);
static NS_DEFINE_CID(kBufferCID, NS_BUFFER_CID);
static NS_DEFINE_CID(kByteBufferCID, NS_BYTEBUFFER_CID);
static NS_DEFINE_CID(kPageManagerCID, NS_PAGEMANAGER_CID);
static NS_DEFINE_CID(kPropertiesCID, NS_PROPERTIES_CID);
static NS_DEFINE_CID(kSupportsArrayCID, NS_SUPPORTSARRAY_CID);
static NS_DEFINE_CID(kUnicharBufferCID, NS_UNICHARBUFFER_CID);
// ds/nsISupportsPrimitives
static NS_DEFINE_CID(kSupportsIDCID, NS_SUPPORTS_ID_CID);
static NS_DEFINE_CID(kSupportsStringCID, NS_SUPPORTS_STRING_CID);
static NS_DEFINE_CID(kSupportsWStringCID, NS_SUPPORTS_WSTRING_CID);
static NS_DEFINE_CID(kSupportsPRBoolCID, NS_SUPPORTS_PRBOOL_CID);
static NS_DEFINE_CID(kSupportsPRUint8CID, NS_SUPPORTS_PRUINT8_CID);
static NS_DEFINE_CID(kSupportsPRUnicharCID, NS_SUPPORTS_PRUNICHAR_CID);
static NS_DEFINE_CID(kSupportsPRUint16CID, NS_SUPPORTS_PRUINT16_CID);
static NS_DEFINE_CID(kSupportsPRUint32CID, NS_SUPPORTS_PRUINT32_CID);
static NS_DEFINE_CID(kSupportsPRUint64CID, NS_SUPPORTS_PRUINT64_CID);
static NS_DEFINE_CID(kSupportsPRTimeCID, NS_SUPPORTS_PRTIME_CID);
static NS_DEFINE_CID(kSupportsCharCID, NS_SUPPORTS_CHAR_CID);
static NS_DEFINE_CID(kSupportsPRInt16CID, NS_SUPPORTS_PRInt16_CID);
static NS_DEFINE_CID(kSupportsPRInt32CID, NS_SUPPORTS_PRInt32_CID);
static NS_DEFINE_CID(kSupportsPRInt64CID, NS_SUPPORTS_PRInt64_CID);
// io
static NS_DEFINE_CID(kFileSpecCID, NS_FILESPEC_CID);
static NS_DEFINE_CID(kDirectoryIteratorCID, NS_DIRECTORYITERATOR_CID);
// components
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
// threads
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
// proxy
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);


// ds/nsISupportsPrimitives
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsIDImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsStringImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsWStringImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRBoolImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint8Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUnicharImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint16Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint32Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRUint64Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRTimeImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsCharImpl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt16Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt32Impl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSupportsPRInt64Impl)

////////////////////////////////////////////////////////////////////////////////
// XPCOM initialization
//
// To Control the order of initialization of these key components I am putting
// this function.
//
//	- nsServiceManager
//		- nsComponentManager
//			- nsRegistry
//
// Here are key points to remember:
//	- A global of all these need to exist. nsServiceManager is an independent object.
//	  nsComponentManager uses both the globalServiceManager and its own registry.
//
//	- A static object of both the nsComponentManager and nsServiceManager
//	  are in use. Hence InitXPCOM() gets triggered from both
//	  NS_GetGlobale{Service/Component}Manager() calls.
//
//	- There exists no global Registry. Registry can be created from the component manager.
//

static nsresult
RegisterGenericFactory(nsIComponentManager* compMgr, const nsCID& cid, const char* className,
                       const char *progid, nsIGenericFactory::ConstructorProcPtr constr)
{
    nsresult rv;
    nsIGenericFactory* fact;
    rv = NS_NewGenericFactory(&fact, constr);
    if (NS_FAILED(rv)) return rv;
    rv = compMgr->RegisterFactory(cid, className, progid, fact, PR_TRUE);
    NS_RELEASE(fact);
    return rv;
}

nsIServiceManager* nsServiceManager::mGlobalServiceManager = NULL;
nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = NULL;

nsresult NS_COM NS_InitXPCOM(nsIServiceManager* *result)
{
    nsresult rv = NS_OK;

    // 1. Create the Global Service Manager
    nsIServiceManager* servMgr = NULL;
    if (nsServiceManager::mGlobalServiceManager == NULL)
    {
        rv = NS_NewServiceManager(&servMgr);
        if (NS_FAILED(rv)) return rv;
        nsServiceManager::mGlobalServiceManager = servMgr;
        if (result)
		{
	        NS_ADDREF(servMgr);
			*result = servMgr;
		}
    }

    // 2. Create the Component Manager and register with global service manager
    //    It is understood that the component manager can use the global service manager.
    nsComponentManagerImpl *compMgr = NULL;
    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        compMgr = new nsComponentManagerImpl();
        if (compMgr == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(compMgr);
        rv = compMgr->Init();
        if (NS_FAILED(rv))
        {
            NS_RELEASE(compMgr);
            return rv;
        }
        nsComponentManagerImpl::gComponentManager = compMgr;
    }
    
    rv = servMgr->RegisterService(kComponentManagerCID, compMgr);
    if (NS_FAILED(rv)) return rv;

    // 3. Register the global services with the component manager so that
    //    clients can create new objects.

    // Registry
    nsIFactory *registryFactory = NULL;
    rv = NS_RegistryGetFactory(&registryFactory);
    if (NS_FAILED(rv)) return rv;

    NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);

    rv = compMgr->RegisterFactory(kRegistryCID,
                                  NS_REGISTRY_CLASSNAME,
                                  NS_REGISTRY_PROGID,
                                  registryFactory, PR_TRUE);
    NS_RELEASE(registryFactory);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kAllocatorCID,
                                NS_ALLOCATOR_CLASSNAME,
                                NS_ALLOCATOR_PROGID,
                                nsAllocatorImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kArenaCID,
                                NS_ARENA_CLASSNAME,
                                NS_ARENA_PROGID,
                                ArenaImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kBufferCID,
                                NS_BUFFER_CLASSNAME,
                                NS_BUFFER_PROGID,
                                nsBuffer::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kByteBufferCID,
                                NS_BYTEBUFFER_CLASSNAME,
                                NS_BYTEBUFFER_PROGID,
                                ByteBufferImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kFileSpecCID,
                                NS_FILESPEC_CLASSNAME,
                                NS_FILESPEC_PROGID,
                                nsFileSpecImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kDirectoryIteratorCID,
                                NS_DIRECTORYITERATOR_CLASSNAME,
                                NS_DIRECTORYITERATOR_PROGID,
                                nsDirectoryIteratorImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kPageManagerCID,
                                NS_PAGEMANAGER_CLASSNAME,
                                NS_PAGEMANAGER_PROGID,
                                nsPageMgr::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kPropertiesCID,
                                NS_PROPERTIES_CLASSNAME,
                                NS_PROPERTIES_PROGID,
                                nsProperties::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kPersistentPropertiesCID,
                                NS_PERSISTENTPROPERTIES_CLASSNAME,
                                NS_PERSISTENTPROPERTIES_PROGID,
                                nsPersistentProperties::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsArrayCID,
                                NS_SUPPORTSARRAY_CLASSNAME,
                                NS_SUPPORTSARRAY_PROGID,
                                nsSupportsArray::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, nsObserver::GetCID(), 
                                NS_OBSERVER_CLASSNAME,
                                NS_OBSERVER_PROGID,
                                nsObserver::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, nsObserverService::GetCID(),
                                NS_OBSERVERSERVICE_CLASSNAME,
                                NS_OBSERVERSERVICE_PROGID,
                                nsObserverService::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kGenericFactoryCID,
                                NS_GENERICFACTORY_CLASSNAME,
                                NS_GENERICFACTORY_PROGID,
                                nsGenericFactory::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kEventQueueServiceCID,
                                NS_EVENTQUEUESERVICE_CLASSNAME,
                                NS_EVENTQUEUESERVICE_PROGID,
                                nsEventQueueServiceImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kEventQueueCID,
                                NS_EVENTQUEUE_CLASSNAME,
                                NS_EVENTQUEUE_PROGID,
                                nsEventQueueImpl::Create);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, 
                                kProxyObjectManagerCID, 
                                NS_XPCOMPROXY_CLASSNAME,
                                NS_XPCOMPROXY_PROGID,
                                nsProxyObjectManager::Create);
    if (NS_FAILED(rv)) return rv;


    rv = RegisterGenericFactory(compMgr, kSupportsIDCID,
                                NS_SUPPORTS_ID_CLASSNAME,
                                NS_SUPPORTS_ID_PROGID,
                                nsSupportsIDImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsStringCID,
                                NS_SUPPORTS_STRING_CLASSNAME,
                                NS_SUPPORTS_STRING_PROGID,
                                nsSupportsStringImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsWStringCID,
                                NS_SUPPORTS_WSTRING_CLASSNAME,
                                NS_SUPPORTS_WSTRING_PROGID,
                                nsSupportsWStringImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRBoolCID,
                                NS_SUPPORTS_PRBOOL_CLASSNAME,
                                NS_SUPPORTS_PRBOOL_PROGID,
                                nsSupportsPRBoolImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint8CID,
                                NS_SUPPORTS_PRUINT8_CLASSNAME,
                                NS_SUPPORTS_PRUINT8_PROGID,
                                nsSupportsPRUint8ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUnicharCID,
                                NS_SUPPORTS_PRUNICHAR_CLASSNAME,
                                NS_SUPPORTS_PRUNICHAR_PROGID,
                                nsSupportsPRUnicharImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint16CID,
                                NS_SUPPORTS_PRUINT16_CLASSNAME,
                                NS_SUPPORTS_PRUINT16_PROGID,
                                nsSupportsPRUint16ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint32CID,
                                NS_SUPPORTS_PRUINT32_CLASSNAME,
                                NS_SUPPORTS_PRUINT32_PROGID,
                                nsSupportsPRUint32ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRUint64CID,
                                NS_SUPPORTS_PRUINT64_CLASSNAME,
                                NS_SUPPORTS_PRUINT64_PROGID,
                                nsSupportsPRUint64ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRTimeCID,
                                NS_SUPPORTS_PRTIME_CLASSNAME,
                                NS_SUPPORTS_PRTIME_PROGID,
                                nsSupportsPRTimeImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsCharCID,
                                NS_SUPPORTS_CHAR_CLASSNAME,
                                NS_SUPPORTS_CHAR_PROGID,
                                nsSupportsCharImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRInt16CID,
                                NS_SUPPORTS_PRInt16_CLASSNAME,
                                NS_SUPPORTS_PRInt16_PROGID,
                                nsSupportsPRInt16ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRInt32CID,
                                NS_SUPPORTS_PRInt32_CLASSNAME,
                                NS_SUPPORTS_PRInt32_PROGID,
                                nsSupportsPRInt32ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    rv = RegisterGenericFactory(compMgr, kSupportsPRInt64CID,
                                NS_SUPPORTS_PRInt64_CLASSNAME,
                                NS_SUPPORTS_PRInt64_PROGID,
                                nsSupportsPRInt64ImplConstructor);
    if (NS_FAILED(rv)) return rv;

    // Prepopulate registry for performance
    // Ignore return value. It is ok if this fails.
    nsComponentManagerImpl::gComponentManager->PlatformPrePopulateRegistry();

    return rv;
}

//
// NS_ShutdownXPCOM()
//
// The shutdown sequence for xpcom would be
//
// - Release the Global Service Manager
//   - Release all service instances held by the global service manager
//   - Release the Global Service Manager itself
// - Release the Component Manager
//   - Release all factories cached by the Component Manager
//   - Unload Libraries
//   - Release Progid Cache held by Component Manager
//   - Release dll abstraction held by Component Manager
//   - Release the Registry held by Component Manager
//   - Finally, release the component manager itself
//
nsresult NS_COM NS_ShutdownXPCOM(nsIServiceManager* servMgr)
{
    NS_RELEASE(nsServiceManager::mGlobalServiceManager);
    NS_RELEASE(nsComponentManagerImpl::gComponentManager);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
