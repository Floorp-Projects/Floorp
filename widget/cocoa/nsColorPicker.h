/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorPicker_h_
#define nsColorPicker_h_

#include "nsIColorPicker.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIColorPickerShownCallback;
class mozIDOMWindowProxy;
@class NSColorPanelWrapper;
@class NSColor;

class nsColorPicker final : public nsIColorPicker {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                  const nsAString& aInitialColor) override;
  NS_IMETHOD Open(nsIColorPickerShownCallback* aCallback) override;

  // For NSColorPanelWrapper.
  void Update(NSColor* aColor);
  void Done();

 private:
  ~nsColorPicker();

  static NSColor* GetNSColorFromHexString(const nsAString& aColor);
  static void GetHexStringFromNSColor(NSColor* aColor, nsAString& aResult);

  NSColorPanelWrapper* mColorPanelWrapper;

  nsString mTitle;
  nsString mColor;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
};

#endif  // nsColorPicker_h_
