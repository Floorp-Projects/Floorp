/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorPickerProxy_h
#define nsColorPickerProxy_h

#include "nsIColorPicker.h"

#include "mozilla/dom/PColorPickerChild.h"

class nsColorPickerProxy final : public nsIColorPicker,
                                 public mozilla::dom::PColorPickerChild
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOLORPICKER

  nsColorPickerProxy() {}

  virtual mozilla::ipc::IPCResult RecvUpdate(const nsString& aColor) override;
  virtual mozilla::ipc::IPCResult Recv__delete__(const nsString& aColor) override;

private:
  ~nsColorPickerProxy() {}

  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
  nsString mTitle;
  nsString mInitialColor;
};

#endif // nsColorPickerProxy_h
