/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIServiceManager_h___
#define nsIServiceManager_h___

#include "nsIComponentManager.h"
#include "nsID.h"

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

class nsIShutdownListener;
class nsIDirectoryServiceProvider;

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
    // let's do it again, this time with ContractIDs...

    NS_IMETHOD
    RegisterService(const char* aContractID, nsISupports* aService) = 0;

    NS_IMETHOD
    UnregisterService(const char* aContractID) = 0;

    NS_IMETHOD
    GetService(const char* aContractID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull) = 0;

    /* OBSOLETE */
    NS_IMETHOD
    ReleaseService(const char* aContractID, nsISupports* service,
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
    // let's do it again, this time with ContractIDs...

    static nsresult
    RegisterService(const char* aContractID, nsISupports* aService);

    static nsresult
    UnregisterService(const char* aContractID);

    static nsresult
    GetService(const char* aContractID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull);

    /* OBSOLETE: use NS_RELEASE(service) instead. */
    static nsresult
    ReleaseService(const char* aContractID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull);

    ////////////////////////////////////////////////////////////////////////////

    static nsresult GetGlobalServiceManager(nsIServiceManager* *result);

    // This method can be called when shutting down the application. It  
    // releases all the global services, and deletes the global service manager.
    static nsresult ShutdownGlobalServiceManager(nsIServiceManager* *result);
};

////////////////////////////////////////////////////////////////////////////
// Using servicemanager with COMPtrs

class NS_COM nsGetServiceByCID : public nsCOMPtr_helper
  {
    public:
      nsGetServiceByCID( const nsCID& aCID, nsISupports* aServiceManager, nsresult* aErrorPtr )
          : mCID(aCID),
            mServiceManager( do_QueryInterface(aServiceManager) ),
            mErrorPtr(aErrorPtr)
        {
          // nothing else to do
        }

  	  virtual nsresult operator()( const nsIID&, void** ) const;
	virtual ~nsGetServiceByCID() {};

    private:
      const nsCID&                mCID;
      nsCOMPtr<nsIServiceManager> mServiceManager;
      nsresult*                   mErrorPtr;
  };

inline
const nsGetServiceByCID
do_GetService( const nsCID& aCID, nsresult* error = 0 )
  {
    return nsGetServiceByCID(aCID, 0, error);
  }

inline
const nsGetServiceByCID
do_GetService( const nsCID& aCID, nsISupports* aServiceManager, nsresult* error = 0 )
  {
    return nsGetServiceByCID(aCID, aServiceManager, error);
  }

class NS_COM nsGetServiceByContractID : public nsCOMPtr_helper
  {
    public:
      nsGetServiceByContractID( const char* aContractID, nsISupports* aServiceManager, nsresult* aErrorPtr )
          : mContractID(aContractID),
            mServiceManager( do_QueryInterface(aServiceManager) ),
            mErrorPtr(aErrorPtr)
        {
          // nothing else to do
        }

  	  virtual nsresult operator()( const nsIID&, void** ) const;

	virtual ~nsGetServiceByContractID() {};

    private:
      const char*                 mContractID;
      nsCOMPtr<nsIServiceManager> mServiceManager;
      nsresult*                   mErrorPtr;
  };

inline
const nsGetServiceByContractID
do_GetService( const char* aContractID, nsresult* error = 0 )
  {
    return nsGetServiceByContractID(aContractID, 0, error);
  }

inline
const nsGetServiceByContractID
do_GetService( const char* aContractID, nsISupports* aServiceManager, nsresult* error = 0 )
  {
    return nsGetServiceByContractID(aContractID, aServiceManager, error);
  }

class NS_COM nsGetServiceFromCategory : public nsCOMPtr_helper
{
public:
  nsGetServiceFromCategory(const char* aCategory, const char* aEntry,
                           nsISupports* aServiceManager, 
                           nsresult* aErrorPtr)
    : mCategory(aCategory),
      mEntry(aEntry),
      mServiceManager( do_QueryInterface(aServiceManager) ),
      mErrorPtr(aErrorPtr)
  {
    // nothing else to do
  }
  
  virtual nsresult operator()( const nsIID&, void** ) const;
  virtual ~nsGetServiceFromCategory() {};
protected:
  const char*                 mCategory;
  const char*                 mEntry;
  nsCOMPtr<nsIServiceManager> mServiceManager;
  nsresult*                   mErrorPtr;
};
    
inline
const nsGetServiceFromCategory
do_GetServiceFromCategory( const char* category, const char* entry,
                           nsresult* error = 0)
  {
    return nsGetServiceFromCategory(category, entry, 0, error);
  }

// type-safe shortcuts for calling |GetService|
template <class DestinationType>
inline
nsresult
CallGetService( const nsCID &aClass,
                nsIShutdownListener* shutdownListener,
                DestinationType** aDestination)
  {
    NS_PRECONDITION(aDestination, "null parameter");

    return nsServiceManager::GetService(aClass,
               NS_GET_IID(DestinationType),
               NS_REINTERPRET_CAST(nsISupports**, aDestination),
               shutdownListener);
  }

template <class DestinationType>
inline
nsresult
CallGetService( const nsCID &aClass,
                DestinationType** aDestination)
  {
    NS_PRECONDITION(aDestination, "null parameter");

    return nsServiceManager::GetService(aClass,
               NS_GET_IID(DestinationType),
               NS_REINTERPRET_CAST(nsISupports**, aDestination),
               nsnull);
  }

template <class DestinationType>
inline
nsresult
CallGetService( const char *aContractID,
                nsIShutdownListener* shutdownListener,
                DestinationType** aDestination)
  {
    NS_PRECONDITION(aContractID, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

    return nsServiceManager::GetService(aContractID,
               NS_GET_IID(DestinationType),
               NS_REINTERPRET_CAST(nsISupports**, aDestination),
               shutdownListener);
  }

template <class DestinationType>
inline
nsresult
CallGetService( const char *aContractID,
                DestinationType** aDestination)
  {
    NS_PRECONDITION(aContractID, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

    return nsServiceManager::GetService(aContractID,
               NS_GET_IID(DestinationType),
               NS_REINTERPRET_CAST(nsISupports**, aDestination),
               nsnull);
  }


////////////////////////////////////////////////////////////////////////////////
// Initialization of XPCOM. Creates the global ComponentManager, ServiceManager
// and registers xpcom components with the ComponentManager. Should be called
// before any call can be made to XPCOM. Currently we are coping with this
// not being called and internally initializing XPCOM if not already.
//

// binDirectory is the absolute path to the mozilla bin directory.
// directory must contain a "components" directory and a component.reg file.

extern NS_COM nsresult
NS_InitXPCOM(nsIServiceManager* *result, nsIFile* binDirectory);

// appFileLocationProvider provides application relative file locations.
// If nsnull is passed, the default implementation will be created and used.

extern NS_COM nsresult
NS_InitXPCOM2(nsIServiceManager* *result, nsIFile* binDirectory, nsIDirectoryServiceProvider* appFileLocationProvider);

////////////////////////////////////////////////////////////////////////////////
// Shutdown of XPCOM. XPCOM hosts an observer (NS_XPCOM_SHUTDOWN_OBSERVER_ID)
// for modules to observer the shutdown. The first thing NS_ShutdownXPCOM()
// does is to notify observers of NS_XPCOM_SHUTDOWN_OBSERVER_ID. After the
// observers have been notified, access to ServiceManager will return
// NS_ERROR_UNEXPECTED.
//

extern NS_COM nsresult
NS_ShutdownXPCOM(nsIServiceManager* servMgr);

////////////////////////////////////////////////////////////////////////////////
// Observing xpcom shutdown

#define NS_XPCOM_SHUTDOWN_OBSERVER_ID "xpcom-shutdown"

#define NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID "xpcom-autoregistration"

////////////////////////////////////////////////////////////////////////////////
// Convenience functions
//
// CreateServicesFromCategory()
//
// Given a category, this convenience functions enumerates the category and 
// creates a service of every CID or ContractID registered under the category.
// If observerTopic is non null and the service implements nsIObserver,
// this will attempt to notify the observer with this observerTopic string
// and origin as parameter.
//
extern NS_COM nsresult
NS_CreateServicesFromCategory(const char* category, nsISupports *origin, const PRUnichar *observerTopic);

// Get a service with control to not create it
// The right way to do this is to add this into the nsIServiceManager
// api. Changing the api however will break a whole bunch of modules -
// plugins and other embedding clients. So until we figure out how to 
// make new apis and maintain backward compabitility etc, we are adding
// this api.
//
// WARNING: USE AT YOUR OWN RISK. THIS FUNCTION WILL NOT BE SUPPORTED
//          IN FUTURE RELEASES.
extern NS_COM nsresult
NS_GetService(const char *aContractID, const nsIID& aIID, PRBool aDontCreate, nsISupports** result);
#endif /* nsIServiceManager_h___ */
