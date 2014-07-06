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
  // Call this method if you are done with this input, but the color picker needs to
  // stay open as it will be associated to another input
  void DoneWithRetarget();
  // Same as DoneWithRetarget + clean the static instance of sColorPanelWrapper,
  // as it is not needed anymore for now
  void Done();

private:
  ~nsColorPicker();

  static NSColor* GetNSColorFromHexString(const nsAString& aColor);
  static void GetHexStringFromNSColor(NSColor* aColor, nsAString& aResult);

  static NSColorPanelWrapper* sColorPanelWrapper;

  nsString             mTitle;
  nsString             mColor;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
};

#endif // nsColorPicker_h_
