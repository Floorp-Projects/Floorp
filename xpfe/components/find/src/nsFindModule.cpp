

#include "nsIGenericFactory.h"

#include "nsString.h"
#include "nsFindService.h"


NS_GENERIC_FACTORY_CONSTRUCTOR(nsFindService)

static const nsModuleComponentInfo gComponents[] = {
  { 
    "Find Service",
    NS_FIND_SERVICE_CID,
    NS_FIND_SERVICE_CONTRACTID,
    nsFindServiceConstructor
  }
};

NS_IMPL_NSGETMODULE(nsFindComponent, gComponents)

