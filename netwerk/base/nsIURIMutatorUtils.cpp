#include "nsIURIMutator.h"
#include "nsIURI.h"
#include "nsComponentManagerUtils.h"

static nsresult
GetURIMutator(nsIURI* aURI, nsIURIMutator** aMutator)
{
    if (NS_WARN_IF(!aURI)) {
        return NS_ERROR_INVALID_ARG;
    }
    return aURI->Mutate(aMutator);
}

NS_MutateURI::NS_MutateURI(nsIURI* aURI)
{
  mStatus = GetURIMutator(aURI, getter_AddRefs(mMutator));
}

NS_MutateURI::NS_MutateURI(const char * aContractID)
{
  mMutator = do_CreateInstance(aContractID, &mStatus);
}
