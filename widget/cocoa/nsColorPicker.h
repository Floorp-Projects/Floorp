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
class nsIDOMWindow;
@class NSColorPanelWrapper;
@class NSColor;

class nsColorPicker MOZ_FINAL : public nsIColorPicker
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDOMWindow* aParent, const nsAString& aTitle,
                  const nsAString& aInitialColor);
  NS_IMETHOD Open(nsIColorPickerShownCallback* aCallback);

  // For NSColorPanelWrapper.
  void Update(NSColor* aColor);
  void Done();

private:
  static NSColor* GetNSColorFromHexString(const nsAString& aColor);
  static void GetHexStringFromNSColor(NSColor* aColor, nsAString& aResult);

  nsString             mTitle;
  nsString             mColor;
  NSColorPanelWrapper* mColorPanel;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
};

#endif // nsColorPicker_h_
