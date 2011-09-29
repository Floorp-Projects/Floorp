/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#include "mozilla/ModuleUtils.h"
#include "mozilla/GenericFactory.h"

#include "nsICategoryManager.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCID.h"
#include "nsStringAPI.h"

namespace mozilla {

NS_IMPL_THREADSAFE_ISUPPORTS1(GenericModule, nsIModule)

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
    r->RegisterFactoryLocation(*e->cid, "", NULL, aLocation, aLoaderStr, aType);

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

    nsCAutoString r;
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
