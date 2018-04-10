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
  NS_ENSURE_SUCCESS_VOID(mStatus);
}

NS_MutateURI::NS_MutateURI(const char* aContractID)
  : mStatus{ NS_ERROR_NOT_INITIALIZED }
{
  mMutator = do_CreateInstance(aContractID, &mStatus);
  MOZ_ASSERT(NS_SUCCEEDED(mStatus), "Called with wrong aContractID");
}
