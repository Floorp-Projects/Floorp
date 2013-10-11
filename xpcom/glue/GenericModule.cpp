/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "mozilla/GenericFactory.h"

#include "nsICategoryManager.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCID.h"
#include "nsStringAPI.h"

namespace mozilla {

NS_IMPL_ISUPPORTS1(GenericModule, nsIModule)

NS_IMETHODIMP
GenericModule::GetClassObject(nsIComponentManager* aCompMgr,
                              const nsCID& aCID,
                              const nsIID& aIID,
                              void** aResult)
{
  for (const Module::CIDEntry* e = mData->mCIDs; e->cid; ++e) {
    if (e->cid->Equals(aCID)) {
      nsCOMPtr<nsIFactory> f;
      if (e->getFactoryProc) {
        f = e->getFactoryProc(*mData, *e);
      }
      else {
        NS_ASSERTION(e->constructorProc, "No constructor proc?");
        f = new GenericFactory(e->constructorProc);
      }
      if (!f)
        return NS_ERROR_FAILURE;

      return f->QueryInterface(aIID, aResult);
    }
  }
  NS_ERROR("Asking a module for a CID it doesn't implement.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericModule::RegisterSelf(nsIComponentManager* aCompMgr,
                            nsIFile* aLocation,
                            const char* aLoaderStr,
                            const char* aType)
{
  nsCOMPtr<nsIComponentRegistrar> r = do_QueryInterface(aCompMgr);
  for (const Module::CIDEntry* e = mData->mCIDs; e->cid; ++e)
    r->RegisterFactoryLocation(*e->cid, "", nullptr, aLocation, aLoaderStr, aType);

  for (const Module::ContractIDEntry* e = mData->mContractIDs;
       e && e->contractid;
       ++e)
    r->RegisterFactoryLocation(*e->cid, "", e->contractid, aLocation, aLoaderStr, aType);

  nsCOMPtr<nsICategoryManager> catman;
  for (const Module::CategoryEntry* e = mData->mCategoryEntries;
       e && e->category;
       ++e) {
    if (!catman)
      catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);

    nsAutoCString r;
    catman->AddCategoryEntry(e->category, e->entry, e->value, true, true,
                             getter_Copies(r));
  }
  return NS_OK;
}

NS_IMETHODIMP
GenericModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                              nsIFile* aFile,
                              const char* aLoaderStr)
{
  NS_ERROR("Nobody should ever call UnregisterSelf!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GenericModule::CanUnload(nsIComponentManager* aCompMgr, bool* aResult)
{
  NS_ERROR("Nobody should ever call CanUnload!");
  *aResult = false;
  return NS_OK;
}

} // namespace mozilla
