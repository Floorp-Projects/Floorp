/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIServiceManager_h___
#define nsIServiceManager_h___

#include "nsIComponentManager.h"
#include "nsID.h"

class nsIShutdownListener;

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
 * finished with a service the release it by calling ReleaseService (instead
 * of releasing the service object directly):
 *
 *    nsICacheManager* cm;
 *    nsServiceManager::GetService(kCacheManagerCID, kICacheManagerIID, (nsISupports**)&cm);
 *
 *    ... use cm, and then sometime later ...
 *
 *    nsServiceManager::ReleaseService(kCacheManagerCID, cm);
 *
 * A user of a service may keep references to particular services indefinitely
 * and only must call ReleaseService when it shuts down. However if the user
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
               nsIShutdownListener* shutdownListener = NULL) = 0;

    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL) = 0;

    ////////////////////////////////////////////////////////////////////////////
    // let's do it again, this time with ProgIDs...

    NS_IMETHOD
    RegisterService(const char* aProgID, nsISupports* aService) = 0;

    NS_IMETHOD
    UnregisterService(const char* aProgID) = 0;

    NS_IMETHOD
    GetService(const char* aProgID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL) = 0;

    NS_IMETHOD
    ReleaseService(const char* aProgID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL) = 0;

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
               nsIShutdownListener* shutdownListener = NULL);

    static nsresult
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    ////////////////////////////////////////////////////////////////////////////
    // let's do it again, this time with ProgIDs...

    static nsresult
    RegisterService(const char* aProgID, nsISupports* aService);

    static nsresult
    UnregisterService(const char* aProgID);

    static nsresult
    GetService(const char* aProgID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    static nsresult
    ReleaseService(const char* aProgID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    ////////////////////////////////////////////////////////////////////////////

    static nsresult GetGlobalServiceManager(nsIServiceManager* *result);

    // This method can be called when shutting down the application. It  
    // releases all the global services, and deletes the global service manager.
    static nsresult ShutdownGlobalServiceManager(nsIServiceManager* *result);

    static nsIServiceManager* mGlobalServiceManager;

};

////////////////////////////////////////////////////////////////////////////////
// nsService: Template to make using services easier. Now you can replace this:
//
//      nsIMyService* service;
//      rv = nsServiceManager::GetService(cid, iid, &service);
//      if (NS_SUCCEEDED(rv)) {
//              service->Doit(...);     // use my service
//              rv = nsServiceManager::ReleaseService(cid, service);
//      }
//
// with this:
//
//      nsService<nsIMyService> service(cid, &rv);
//      if (NS_SUCCEEDED(rv)) {
//              service->Doit(...);     // use my service
//      }
//
// and the automatic destructor will take care of releasing the service.

#if 0   // sorry the T::GetIID() construct doesn't work with egcs

template<class T> class nsService {
protected:
  nsCID mCID;
  T* mService;

public:
  nsService(nsISupports* aServMgr, const nsCID& aClass, nsresult *rv)
    : mCID(aClass), mService(0)
  {
    nsIServiceManager* servMgr;
    *rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void**)&servMgr);
    if (NS_SUCCEEDED(*rv)) {
      *rv = servMgr->GetService(mCID, T::GetIID(), (nsISupports**)&mService);
      NS_RELEASE(servMgr);
    }
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get service.");
  }

  nsService(nsISupports* aServMgr, const char* aProgID, nsresult *rv)
    : mService(0)
  {
    *rv = nsComponentManager::ProgIDToCLSID(aProgID, &mCID);
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get CLSID.");
    if (NS_FAILED(*rv)) return;
  
    nsIServiceManager* servMgr;
    *rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void**)&servMgr);
    if (NS_SUCCEEDED(*rv)) {
      *rv = servMgr->GetService(mCID, T::GetIID(), (nsISupports**)&mService);
      NS_RELEASE(servMgr);
    }
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get service.");
  }

  nsService(const nsCID& aClass, nsresult *rv)
    : mCID(aClass), mService(0) {
    *rv = nsServiceManager::GetService(aClass, T::GetIID(),
                                       (nsISupports**)&mService);
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get service.");
  }

  ~nsService() {
    if (mService) {       // mService could be null if the constructor fails
      nsresult rv = nsServiceManager::ReleaseService(mCID, mService);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't release service.");
    }
  }

  T* operator->() const {
    NS_PRECONDITION(mService != 0, "Your code should test the error result from the constructor.");
    return mService;
  }

  PRBool operator==(const T* other) {
    return mService == other;
  }

  operator T*() const {
    return mService;
  }

};

#else

class nsService {
protected:
  nsCID mCID;
  nsISupports* mService;

public:
  nsService(nsISupports* aServMgr, const nsCID& aClass, const nsIID& aIID, nsresult *rv)
    : mCID(aClass), mService(0)
  {
    nsIServiceManager* servMgr;
    *rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void**)&servMgr);
    if (NS_SUCCEEDED(*rv)) {
      *rv = servMgr->GetService(mCID, aIID, &mService);
      NS_RELEASE(servMgr);
    }
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get service.");
  }

  nsService(nsISupports* aServMgr, const char* aProgID, const nsIID& aIID, nsresult *rv)
    : mService(0)
  {
    *rv = nsComponentManager::ProgIDToCLSID(aProgID, &mCID);
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get CLSID.");
    if (NS_FAILED(*rv)) return;
  
    nsIServiceManager* servMgr;
    *rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void**)&servMgr);
    if (NS_SUCCEEDED(*rv)) {
      *rv = servMgr->GetService(mCID, aIID, &mService);
      NS_RELEASE(servMgr);
    }
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get service.");
  }

  nsService(const nsCID& aClass, const nsIID& aIID, nsresult *rv)
    : mCID(aClass), mService(0) {
    *rv = nsServiceManager::GetService(aClass, aIID,
                                       (nsISupports**)&mService);
    NS_ASSERTION(NS_SUCCEEDED(*rv), "Couldn't get service.");
  }

  ~nsService() {
    if (mService) {       // mService could be null if the constructor fails
      nsresult rv = NS_OK;
      rv = nsServiceManager::ReleaseService(mCID, mService);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't release service.");
    }
  }

  nsISupports* operator->() const {
    NS_PRECONDITION(mService != 0, "Your code should test the error result from the constructor.");
    return mService;
  }

  PRBool operator==(const nsISupports* other) {
    return mService == other;
  }

  operator nsISupports*() const {
    return mService;
  }

};

#define NS_WITH_SERVICE(T, var, cid, rvAddr)      \
  nsService _serv##var(cid, T::GetIID(), rvAddr); \
  T* var = (T*)(nsISupports*)_serv##var;

#define NS_WITH_SERVICE1(T, var, isupports, cid, rvAddr)     \
  nsService _serv##var(isupports, cid, T::GetIID(), rvAddr); \
  T* var = (T*)(nsISupports*)_serv##var;

#endif

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
extern NS_COM nsresult
NS_InitXPCOM(nsIServiceManager* *result);

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIServiceManager_h___ */
