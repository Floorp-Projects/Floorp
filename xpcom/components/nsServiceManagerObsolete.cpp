#include "nsIServiceManager.h"
#include "nsIServiceManagerObsolete.h"

nsServiceManagerObsolete* gServiceManagerObsolete = nsnull;

extern PRBool gXPCOMShuttingDown;

// Global service manager interface (see nsIServiceManagerObsolete.h)

nsresult
nsServiceManager::GetGlobalServiceManager(nsIServiceManager* *result)
{
    if (gXPCOMShuttingDown)
        return NS_ERROR_UNEXPECTED;
    
    if (gServiceManagerObsolete) {
        *result = (nsIServiceManager*)(void*)gServiceManagerObsolete;
        return NS_OK;
    }
        
    gServiceManagerObsolete = new nsServiceManagerObsolete();
    if (!gServiceManagerObsolete)
        return NS_ERROR_OUT_OF_MEMORY;

    *result =  (nsIServiceManager*)(void*)gServiceManagerObsolete;
    return NS_OK;
}

nsresult
nsServiceManager::ShutdownGlobalServiceManager(nsIServiceManager* *result)
{
    delete gServiceManagerObsolete;
    gServiceManagerObsolete = nsnull;
    return NS_OK;
}

nsresult
nsServiceManager::GetService(const nsCID& aClass, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    if (NS_FAILED(rv)) return rv;
    return mgr->GetService(aClass, aIID, (void**)result);
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
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    if (NS_FAILED(rv)) return rv;
    return NS_ERROR_NOT_IMPLEMENTED;//mgr->RegisterService(aClass, aService);
}

nsresult
nsServiceManager::UnregisterService(const nsCID& aClass)
{
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    if (NS_FAILED(rv)) return rv;
    return NS_ERROR_NOT_IMPLEMENTED;//mgr->UnregisterService(aClass);
}

////////////////////////////////////////////////////////////////////////////////
// let's do it again, this time with ContractIDs...

nsresult
nsServiceManager::GetService(const char* aContractID, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    if (NS_FAILED(rv)) return rv;
    return mgr->GetServiceByContractID(aContractID, aIID, (void**)result);
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
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    if (NS_FAILED(rv)) return rv;
    return NS_ERROR_NOT_IMPLEMENTED;//mgr->RegisterServiceByContractID(aContractID, aService);
}

nsresult
nsServiceManager::UnregisterService(const char* aContractID)
{
    // Don't create the global service manager here because we might
    // be shutting down, and releasing all the services in its
    // destructor
    if (gXPCOMShuttingDown)
        return NS_OK;
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    if (NS_FAILED(rv)) return rv;
    
    return NS_ERROR_NOT_IMPLEMENTED;//mgr->UnregisterServiceByContractID(aContractID);
}

////////////////////////////////////////////////////////////////////////////////

nsServiceManagerObsolete::nsServiceManagerObsolete() {
    NS_INIT_ISUPPORTS();    
    
}
nsServiceManagerObsolete::~nsServiceManagerObsolete() {
}

NS_IMPL_ISUPPORTS1(nsServiceManagerObsolete, 
                   nsIServiceManagerObsolete);

NS_IMETHODIMP 
nsServiceManagerObsolete::RegisterService(const nsCID& aClass, nsISupports* aService) {
    return nsServiceManager::RegisterService(aClass, aService);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::UnregisterService(const nsCID& aClass){
    return nsServiceManager::UnregisterService(aClass);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::GetService(const nsCID& aClass, const nsIID& aIID,
                                     nsISupports* *result,
                                     nsIShutdownListener* shutdownListener){
    return nsServiceManager::GetService(aClass, aIID, result, shutdownListener);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::ReleaseService(const nsCID& aClass, nsISupports* service,
                                         nsIShutdownListener* shutdownListener ){
    return nsServiceManager::ReleaseService(aClass, service, shutdownListener);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::RegisterService(const char* aContractID, nsISupports* aService){
    return nsServiceManager::RegisterService(aContractID, aService);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::UnregisterService(const char* aContractID){
    return nsServiceManager::UnregisterService(aContractID);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::GetService(const char* aContractID, const nsIID& aIID,
                                     nsISupports* *result,
                                     nsIShutdownListener* shutdownListener ){
    return nsServiceManager::GetService(aContractID, aIID, result, shutdownListener);
};

NS_IMETHODIMP 
nsServiceManagerObsolete::ReleaseService(const char* aContractID, nsISupports* service,
                                         nsIShutdownListener* shutdownListener ){
    return nsServiceManager::ReleaseService(aContractID, service, shutdownListener);
};



