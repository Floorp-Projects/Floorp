

#include "nsIGenericFactory.h"

#include "nsString.h"
#include "nsFindService.h"


NS_GENERIC_FACTORY_CONSTRUCTOR(nsFindService)

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIFindService, nsFindService::GetSingleton)


static const nsModuleComponentInfo gComponents[] = {
  { 
    "Find Service",
    NS_FIND_SERVICE_CID,
    NS_FIND_SERVICE_CONTRACTID,
    nsFindServiceConstructor
  }

};


PR_STATIC_CALLBACK(void)
nsFindModuleDtor(nsIModule* self)
{
    // Release our singletons
    nsFindService::FreeSingleton();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsFindComponent, gComponents, nsFindModuleDtor)

