/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsIServiceManager.h"
#include "nsIServiceManagerObsolete.h"
#include "nsComponentManager.h"

extern PRBool gXPCOMShuttingDown;

// Global service manager interface (see nsIServiceManagerObsolete.h)

nsresult
nsServiceManager::GetGlobalServiceManager(nsIServiceManager* *result)
{
    if (gXPCOMShuttingDown)
        return NS_ERROR_UNEXPECTED;
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;
        
    // this method does not addref for historical reasons.
    // we return the nsIServiceManagerObsolete interface via a cast.
    *result =  (nsIServiceManager*) NS_STATIC_CAST(nsIServiceManagerObsolete*, 
                                                   nsComponentManagerImpl::gComponentManager);
    return NS_OK;
}

nsresult
nsServiceManager::ShutdownGlobalServiceManager(nsIServiceManager* *result)
{
    return NS_OK;
}

nsresult
nsServiceManager::GetService(const nsCID& aClass, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;
    
    return nsComponentManagerImpl::gComponentManager->GetService(aClass, aIID, (void**)result);
}

nsresult
nsServiceManager::ReleaseService(const nsCID& aClass, nsISupports* service,
                                 nsIShutdownListener* shutdownListener)
{
    NS_IF_RELEASE(service);
    return NS_OK;
}

nsresult
nsServiceManager::RegisterService(const nsCID& aClass, nsISupports* aService)
{
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;
    
    return nsComponentManagerImpl::gComponentManager->RegisterService(aClass, aService);
}

nsresult
nsServiceManager::UnregisterService(const nsCID& aClass)
{
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;
    
    return nsComponentManagerImpl::gComponentManager->UnregisterService(aClass);
}

////////////////////////////////////////////////////////////////////////////////
// let's do it again, this time with ContractIDs...

nsresult
nsServiceManager::GetService(const char* aContractID, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;

    return nsComponentManagerImpl::gComponentManager->GetServiceByContractID(aContractID, aIID, (void**)result);
}

nsresult
nsServiceManager::ReleaseService(const char* aContractID, nsISupports* service,
                                 nsIShutdownListener* shutdownListener)
{
    NS_RELEASE(service);
    return NS_OK;
}

nsresult
nsServiceManager::RegisterService(const char* aContractID, nsISupports* aService)
{
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;
    
    return nsComponentManagerImpl::gComponentManager->RegisterService(aContractID, aService);
}

nsresult
nsServiceManager::UnregisterService(const char* aContractID)
{
    // Don't create the global service manager here because we might
    // be shutting down, and releasing all the services in its
    // destructor
    if (gXPCOMShuttingDown)
        return NS_OK;
    
    if (nsComponentManagerImpl::gComponentManager == nsnull)
        return NS_ERROR_UNEXPECTED;

    return nsComponentManagerImpl::gComponentManager->UnregisterService(aContractID);
}

