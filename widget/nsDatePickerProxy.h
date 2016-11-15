/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDatePickerProxy_h
#define nsDatePickerProxy_h

#include "nsIDatePicker.h"

#include "mozilla/dom/PDatePickerChild.h"

class nsDatePickerProxy final : public nsIDatePicker,
                                public mozilla::dom::PDatePickerChild
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDATEPICKER

  nsDatePickerProxy() {}

  virtual mozilla::ipc::IPCResult RecvCancel() override;
  virtual mozilla::ipc::IPCResult Recv__delete__(const nsString& aDate) override;

private:
  ~nsDatePickerProxy() {}

  nsCOMPtr<nsIDatePickerShownCallback> mCallback;
  nsString mTitle;
  nsString mInitialDate;
};

#endif // nsDatePickerProxy_h
