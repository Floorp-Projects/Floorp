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

