/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIURIMutator.h"
#include "nsIURI.h"
#include "nsComponentManagerUtils.h"

static nsresult GetURIMutator(nsIURI* aURI, nsIURIMutator** aMutator) {
  if (NS_WARN_IF(!aURI)) {
    return NS_ERROR_INVALID_ARG;
  }
  return aURI->Mutate(aMutator);
}

NS_MutateURI::NS_MutateURI(nsIURI* aURI) {
  mStatus = GetURIMutator(aURI, getter_AddRefs(mMutator));
  NS_ENSURE_SUCCESS_VOID(mStatus);
}

NS_MutateURI::NS_MutateURI(const char* aContractID)
    : mStatus(NS_ERROR_NOT_INITIALIZED) {
  mMutator = do_CreateInstance(aContractID, &mStatus);
  MOZ_ASSERT(NS_SUCCEEDED(mStatus), "Called with wrong aContractID");
}
