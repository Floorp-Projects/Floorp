/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsColorPickerProxy.h"

#include "mozilla/dom/BrowserChild.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsColorPickerProxy, nsIColorPicker)

NS_IMETHODIMP
nsColorPickerProxy::Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                         const nsAString& aInitialColor) {
  BrowserChild* browserChild = BrowserChild::GetFrom(aParent);
  if (!browserChild) {
    return NS_ERROR_FAILURE;
  }

  browserChild->SendPColorPickerConstructor(this, nsString(aTitle),
                                            nsString(aInitialColor));
  NS_ADDREF_THIS();
  return NS_OK;
}

NS_IMETHODIMP
nsColorPickerProxy::Open(
    nsIColorPickerShownCallback* aColorPickerShownCallback) {
  NS_ENSURE_STATE(!mCallback);
  mCallback = aColorPickerShownCallback;

  SendOpen();
  return NS_OK;
}

mozilla::ipc::IPCResult nsColorPickerProxy::RecvUpdate(const nsString& aColor) {
  if (mCallback) {
    mCallback->Update(aColor);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult nsColorPickerProxy::Recv__delete__(
    const nsString& aColor) {
  if (mCallback) {
    mCallback->Done(aColor);
    mCallback = nullptr;
  }
  return IPC_OK();
}
