/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"

NS_IMETHODIMP_(MozExternalRefCountType)
GenericClassInfo::AddRef()
{
  return 2;
}

NS_IMETHODIMP_(MozExternalRefCountType)
GenericClassInfo::Release()
{
  return 1;
}

NS_IMPL_QUERY_INTERFACE(GenericClassInfo, nsIClassInfo)

NS_IMETHODIMP
GenericClassInfo::GetInterfaces(uint32_t* countp, nsIID*** array)
{
  return mData->getinterfaces(countp, array);
}

NS_IMETHODIMP
GenericClassInfo::GetHelperForLanguage(uint32_t language, nsISupports** helper)
{
  if (mData->getlanguagehelper)
    return mData->getlanguagehelper(language, helper);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetContractID(char** contractid)
{
  NS_ERROR("GetContractID not implemented");
  *contractid = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetClassDescription(char** description)
{
  *description = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetClassID(nsCID** classid)
{
  NS_ERROR("GetClassID not implemented");
  *classid = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericClassInfo::GetImplementationLanguage(uint32_t* language)
{
  *language = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
GenericClassInfo::GetFlags(uint32_t* flags)
{
  *flags = mData->flags;
  return NS_OK;
}

NS_IMETHODIMP
GenericClassInfo::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
  *aClassIDNoAlloc = mData->cid;
  return NS_OK;
}
