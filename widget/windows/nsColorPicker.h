/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorPicker_h__
#define nsColorPicker_h__

#include <windows.h>
#include <commdlg.h>

#include "nsCOMPtr.h"
#include "nsIColorPicker.h"
#include "nsThreadUtils.h"

class nsIWidget;

class AsyncColorChooser : public mozilla::Runnable {
 public:
  AsyncColorChooser(COLORREF aInitialColor,
                    const nsTArray<nsString>& aDefaultColors,
                    nsIWidget* aParentWidget,
                    nsIColorPickerShownCallback* aCallback);
  NS_IMETHOD Run() override;

 private:
  void Update(COLORREF aColor);

  static UINT_PTR CALLBACK HookProc(HWND aDialog, UINT aMsg, WPARAM aWParam,
                                    LPARAM aLParam);

  COLORREF mInitialColor;
  nsTArray<nsString> mDefaultColors;
  COLORREF mColor;
  nsCOMPtr<nsIWidget> mParentWidget;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
};

class nsColorPicker : public nsIColorPicker {
  virtual ~nsColorPicker();

 public:
  nsColorPicker();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICOLORPICKER

 private:
  COLORREF mInitialColor;
  nsTArray<nsString> mDefaultColors;
  nsCOMPtr<nsIWidget> mParentWidget;
};

#endif  // nsColorPicker_h__
