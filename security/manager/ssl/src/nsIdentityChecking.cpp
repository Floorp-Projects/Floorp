#include "nsNSSIOLayer.h"

NS_IMETHODIMP
nsNSSSocketInfo::GetIsExtendedValidation(PRBool* aIsEV)
{
  NS_ENSURE_ARG(aIsEV);
  *aIsEV = PR_FALSE;

  return NS_OK;
}

