/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDatePickerProxy.h"

#include "mozilla/dom/TabChild.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsDatePickerProxy, nsIDatePicker)

/* void init (in nsIDOMWindow parent, in AString title, in short mode); */
NS_IMETHODIMP
nsDatePickerProxy::Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                         const nsAString& aInitialDate)
{
  TabChild* tabChild = TabChild::GetFrom(aParent);
  if (!tabChild) {
    return NS_ERROR_FAILURE;
  }

  tabChild->SendPDatePickerConstructor(this,
                                       nsString(aTitle),
                                       nsString(aInitialDate));
  NS_ADDREF_THIS(); //Released in DeallocPDatePickerChild
  return NS_OK;
}

/* void open (in nsIDatePickerShownCallback aDatePickerShownCallback); */
NS_IMETHODIMP
nsDatePickerProxy::Open(nsIDatePickerShownCallback* aDatePickerShownCallback)
{
  NS_ENSURE_STATE(!mCallback);
  mCallback = aDatePickerShownCallback;

  SendOpen();
  return NS_OK;
}

mozilla::ipc::IPCResult
nsDatePickerProxy::RecvCancel()
{
  if (mCallback) {
    mCallback->Cancel();
    mCallback = nullptr;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
nsDatePickerProxy::Recv__delete__(const nsString& aDate)
{
  if (mCallback) {
    mCallback->Done(aDate);
    mCallback = nullptr;
  }
  return IPC_OK();
}
