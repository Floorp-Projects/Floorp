/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 */

#ifndef nsIServiceManager_h___
#define nsIServiceManager_h___

#include "nsIComponentManager.h"
#include "nsID.h"

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

class nsIShutdownListener;
class nsFileSpec;

#define NS_ISERVICEMANAGER_IID                       \
{ /* cf0df3b0-3401-11d2-8163-006008119d7a */         \
    0xcf0df3b0,                                      \
    0x3401,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/**
 * The nsIServiceManager manager interface provides a means to obtain
 * global services in an application. The service manager depends on the 
 * repository to find and instantiate factories to obtain services.
 *
 * Users of the service manager must first obtain a pointer to the global
 * service manager by calling NS_GetGlobalServiceManager. After that, 
 * they can request specific services by calling GetService. When they are
 * finished they can NS_RELEASE() the service as usual.
 *
 *    nsICacheManager* cm;
 *    nsServiceManager::GetService(kCacheManagerCID, kICacheManagerIID, (nsISupports**)&cm);
 *
 *    ... use cm, and then sometime later ...
 *
 *    NS_RELEASE(cm);
 *
 * A user of a service may keep references to particular services indefinitely
 * and only must call Release when it shuts down.
 *
 * Shutdown Listeners: However if the user
 * wishes to voluntarily cooperate with the shutdown of the service it is 
 * using, it may supply an nsIShutdownListener to provide for asynchronous
 * release of the services it is using. The shutdown listener's OnShutdown
 * method will be called for a service that is being shut down, and it is
 * its responsiblity to release references obtained from that service if at
 * all possible.
 *
 * The process of shutting down a particular service is initiated by calling
 * the service manager's ShutdownService method. This will iterate through
 * all the registered shutdown listeners for the service being shut down, and
 * then will attempt to unload the library associated with the service if
 * possible. The status result of ShutdownService indicates whether the 
 * service was successfully shut down, failed, or was not in service.
 *
 * XXX QUESTIONS:
 * - Should a "service" be more than nsISupports? Should it be a factory 
 *   and/or have Startup(), Shutdown(), etc.
 * - If the asynchronous OnShutdown operation gets called, does the user
 *   of a service still need to call ReleaseService? (Or should they _not_
 *   call it?)
 */
class nsIServiceManager : public nsISupports {
public:

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISERVICEMANAGER_IID);

    /**
     * RegisterService may be called explicitly to register a service
     * with the service manager. If a service is not registered explicitly,
     * the component manager will be used to create an instance according
     * to the class ID specified.
     */
    NS_IMETHOD
    RegisterService(const nsCID& aClass, nsISupports* aService) = 0;

    /**
     * Requests a service to be shut down, possibly unloading its DLL.
     *
     * @returns NS_OK - if shutdown was successful and service was unloaded,
     * @returns NS_ERROR_SERVICE_NOT_FOUND - if shutdown failed because
     *          the service was not currently loaded
     * @returns NS_ERROR_SERVICE_IN_USE - if shutdown failed because some
     *          user of the service wouldn't voluntarily release it by using
     *          a shutdown listener.
     */
    NS_IMETHOD
    UnregisterService(const nsCID& aClass) = 0;

    NS_IMETHOD
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull) = 0;

    /* OBSOLETE: use NS_RELEASE(service) instead. */
    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // let's do it again, this time with ProgIDs...

    NS_IMETHOD
    RegisterService(const char* aProgID, nsISupports* aService) = 0;

    NS_IMETHOD
    UnregisterService(const char* aProgID) = 0;

    NS_IMETHOD
    GetService(const char* aProgID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull) = 0;

    /* OBSOLETE */
    NS_IMETHOD
    ReleaseService(const char* aProgID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull) = 0;

};

#define NS_ERROR_SERVICE_NOT_FOUND      NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_XPCOM, 22)
#define NS_ERROR_SERVICE_IN_USE         NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_XPCOM, 23)

////////////////////////////////////////////////////////////////////////////////

#define NS_ISHUTDOWNLISTENER_IID                     \
{ /* 56decae0-3406-11d2-8163-006008119d7a */         \
    0x56decae0,                                      \
    0x3406,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

class nsIShutdownListener : public nsISupports {
public:

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISHUTDOWNLISTENER_IID);

    NS_IMETHOD
    OnShutdown(const nsCID& aClass, nsISupports* service) = 0;

};

////////////////////////////////////////////////////////////////////////////////
// Interface to Global Services

class NS_COM nsServiceManager {
public:

    static nsresult
    RegisterService(const nsCID& aClass, nsISupports* aService);

    static nsresult
    UnregisterService(const nsCID& aClass);

    static nsresult
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull);

    /* OBSOLETE: use NS_RELEASE(service) instead. */
    static nsresult
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull);

    ////////////////////////////////////////////////////////////////////////////
    // let's do it again, this time with ProgIDs...

    static nsresult
    RegisterService(const char* aProgID, nsISupports* aService);

    static nsresult
    UnregisterService(const char* aProgID);

    static nsresult
    GetService(const char* aProgID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull);

    /* OBSOLETE: use NS_RELEASE(service) instead. */
    static nsresult
    ReleaseService(const char* aProgID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull);

    ////////////////////////////////////////////////////////////////////////////

    static nsresult GetGlobalServiceManager(nsIServiceManager* *result);

    // This method can be called when shutting down the application. It  
    // releases all the global services, and deletes the global service manager.
    static nsresult ShutdownGlobalServiceManager(nsIServiceManager* *result);

    static nsIServiceManager* mGlobalServiceManager;

};

////////////////////////////////////////////////////////////////////////////
// Using servicemanager with COMPtrs

class NS_EXPORT nsGetServiceByCID : public nsCOMPtr_helper
  {
    public:
      nsGetServiceByCID( const nsCID& aCID, nsresult* aErrorPtr )
          : mCID(aCID),
            mErrorPtr(aErrorPtr)
        {
          // nothing else to do
        }

  	  virtual nsresult operator()( const nsIID&, void** ) const;

    private:
      const nsCID& mCID;
      nsresult*    mErrorPtr;
  };

inline
const nsGetServiceByCID
do_GetService( const nsCID& aCID, nsresult* error = 0 )
  {
    return nsGetServiceByCID(aCID, error);
  }

class NS_EXPORT nsGetServiceByProgID : public nsCOMPtr_helper
  {
    public:
      nsGetServiceByProgID( const char* aProgID, nsresult* aErrorPtr )
          : mProgID(aProgID),
            mErrorPtr(aErrorPtr)
        {
          // nothing else to do
        }

  	  virtual nsresult operator()( const nsIID&, void** ) const;

    private:
      const char* mProgID;
      nsresult*   mErrorPtr;
  };

inline
const nsGetServiceByProgID
do_GetService( const char* aProgID, nsresult* error = 0 )
  {
    return nsGetServiceByProgID(aProgID, error);
  }

////////////////////////////////////////////////////////////////////////////////
// NS_WITH_SERVICE: macro to make using services easier. 
// 
// Services can be used with COMPtrs. GetService() is used to get a service.
// NS_RELEASE() can be called on the service got to release it.
//
// NOTE: ReleaseService() is OBSOLETE.
//
// Now you can replace this:
//  {
//      nsIMyService* service;
//      rv = nsServiceManager::GetService(kMyServiceCID, nsIMyService::GetIID(),
//                                        &service);
//      if (NS_FAILED(rv)) return rv;
//      service->Doit(...);     // use my service
//      rv = nsServiceManager::ReleaseService(kMyServiceCID, service);
//  }
// with this:
//  {
//      NS_WITH_SERVICE(nsIMyService, service, kMyServiceCID, &rv);
//      if (NS_FAILED(rv)) return rv;
//      service->Doit(...);     // use my service
//  }
// and the automatic destructor from COMPtr will take care of releasing the service. 
////////////////////////////////////////////////////////////////////////////////
#define NS_WITH_SERVICE(T, var, cid, rvAddr) \
    nsCOMPtr<T> var = do_GetService(cid, rvAddr);

#define NS_WITH_SERVICE1(T, var, isupportsServMgr, cid, rvAddr)     \
    nsCOMPtr<T> var; \
    { \
        nsCOMPtr<nsIServiceManager> _servMgr = do_QueryInterface(isupportsServMgr, rvAddr); \
        if (NS_SUCCEEDED(*rvAddr)) \
        { \
            *rvAddr = _servMgr->GetService(cid, NS_GET_IID(T), getter_AddRefs(var)); \
        } \
    }
    
////////////////////////////////////////////////////////////////////////////////
// NS_NewServiceManager: For when you want to create a service manager
// in a given context.

extern NS_COM nsresult
NS_NewServiceManager(nsIServiceManager* *result);

////////////////////////////////////////////////////////////////////////////////
// Initialization of XPCOM. Creates the global ComponentManager, ServiceManager
// and registers xpcom components with the ComponentManager. Should be called
// before any call can be made to XPCOM. Currently we are coping with this
// not being called and internally initializing XPCOM if not already.
//
// registryFileName is the absolute path to the registry file.  This
// file will be checked for existence.  If it does not exist, it
// will use the current process directory name, and tack on "component.reg"

extern NS_COM nsresult
NS_InitXPCOM(nsIServiceManager* *result, nsFileSpec* registryFile, nsFileSpec* componentDir);

extern NS_COM nsresult
NS_ShutdownXPCOM(nsIServiceManager* servMgr);

////////////////////////////////////////////////////////////////////////////////
// Observing xpcom shutdown

#define NS_XPCOM_SHUTDOWN_OBSERVER_ID "xpcom-shutdown"

#endif /* nsIServiceManager_h___ */
