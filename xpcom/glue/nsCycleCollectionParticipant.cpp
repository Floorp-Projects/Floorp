/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

NS_IMETHODIMP_(void)
nsXPCOMCycleCollectionParticipant::Root(void* aPtr)
{
  nsISupports* s = static_cast<nsISupports*>(aPtr);
  NS_ADDREF(s);
}

NS_IMETHODIMP_(void)
nsXPCOMCycleCollectionParticipant::Unroot(void* aPtr)
{
  nsISupports* s = static_cast<nsISupports*>(aPtr);
  NS_RELEASE(s);
}

// We define a default trace function because some participants don't need
// to trace anything, so it is okay for them not to define one.
NS_IMETHODIMP_(void)
nsXPCOMCycleCollectionParticipant::Trace(void* aPtr, const TraceCallbacks& aCb,
                                         void* aClosure)
{
}

bool
nsXPCOMCycleCollectionParticipant::CheckForRightISupports(nsISupports* aSupports)
{
  nsISupports* foo;
  aSupports->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                            reinterpret_cast<void**>(&foo));
  return aSupports == foo;
}
