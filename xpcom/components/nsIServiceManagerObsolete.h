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
 * The Original Code is XPCOM
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsIServiceManagerObsolete_h___
#define nsIServiceManagerObsolete_h___

////////////////////////////////////////////////////////////////////
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Functions, classes, interfaces and types in this file  are 
// obsolete.  Use at your own risk.  
// Please see nsIServiceManager.idl for the supported interface
// to the service manager.
//
////////////////////////////////////////////////////////////////////

#include "nsIComponentManager.h"
#include "nsID.h"

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

class nsIServiceManager;
class nsIShutdownListener;
class nsIDirectoryServiceProvider;

class nsServiceManagerObsolete;

#define NS_ISERVICEMANAGER_OBSOLETE_IID              \
{ /* cf0df3b0-3401-11d2-8163-006008119d7a */         \
    0xcf0df3b0,                                      \
    0x3401,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

/**
 * The nsIServiceManagerObsolete manager is obsolete.  Please refer
 * to nsIServiceManager.
 */
class nsIServiceManagerObsolete : public nsISupports {
public:

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISERVICEMANAGER_OBSOLETE_IID);

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


class nsServiceManagerObsolete : public nsIServiceManagerObsolete {
public:
    nsServiceManagerObsolete();
    virtual ~nsServiceManagerObsolete();

    NS_DECL_ISUPPORTS
    
    NS_IMETHOD
    RegisterService(const nsCID& aClass, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const nsCID& aClass);

    NS_IMETHOD
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull);

    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull);

    NS_IMETHOD
    RegisterService(const char* aContractID, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const char* aContractID);

    NS_IMETHOD
    GetService(const char* aContractID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = nsnull);

    NS_IMETHOD
    ReleaseService(const char* aContractID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = nsnull);

};


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
    // These methods return really nsIServiceManagerObsolete, but they are 
    // statically cast to nsIServiceManager to preserve backwards compatiblity.
    static nsresult GetGlobalServiceManager(nsIServiceManager* *result);
    static nsresult ShutdownGlobalServiceManager(nsIServiceManager* *result);
};

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

////////////////////////////////////////////////////////////////////////////////


#endif /* nsIServiceManagerObsolete_h___ */










