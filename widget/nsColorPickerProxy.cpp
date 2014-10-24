/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsColorPickerProxy.h"

#include "mozilla/dom/TabChild.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsColorPickerProxy, nsIColorPicker)

/* void init (in nsIDOMWindow parent, in AString title, in short mode); */
NS_IMETHODIMP
nsColorPickerProxy::Init(nsIDOMWindow* aParent, const nsAString& aTitle,
                         const nsAString& aInitialColor)
{
  TabChild* tabChild = TabChild::GetFrom(aParent);
  if (!tabChild) {
    return NS_ERROR_FAILURE;
  }

  tabChild->SendPColorPickerConstructor(this,
                                        nsString(aTitle),
                                        nsString(aInitialColor));
  NS_ADDREF_THIS();
  return NS_OK;
}

/* void open (in nsIColorPickerShownCallback aColorPickerShownCallback); */
NS_IMETHODIMP
nsColorPickerProxy::Open(nsIColorPickerShownCallback* aColorPickerShownCallback)
{
  NS_ENSURE_STATE(!mCallback);
  mCallback = aColorPickerShownCallback;

  SendOpen();
  return NS_OK;
}

bool
nsColorPickerProxy::RecvUpdate(const nsString& aColor)
{
  if (mCallback) {
    mCallback->Update(aColor);
  }
  return true;
}

bool
nsColorPickerProxy::Recv__delete__(const nsString& aColor)
{
  if (mCallback) {
    mCallback->Done(aColor);
    mCallback = nullptr;
  }
  return true;
}