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
#include "nsString.h"
#include "nsThreadUtils.h"

class nsIWidget;

class AsyncColorChooser :
  public nsRunnable
{
public:
  AsyncColorChooser(const nsAString& aInitialColor,
                    nsIWidget* aParentWidget,
                    nsIColorPickerShownCallback* aCallback);
  NS_IMETHOD Run() MOZ_OVERRIDE;

private:
  nsString mInitialColor;
  nsCOMPtr<nsIWidget> mParentWidget;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
  nsString mColor;
};

class nsColorPicker :
  public nsIColorPicker
{
public:
  nsColorPicker();
  virtual ~nsColorPicker();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIDOMWindow* parent, const nsAString& title,
                  const nsAString& aInitialColor);
  NS_IMETHOD Open(nsIColorPickerShownCallback* aCallback);

protected:
  nsString mInitialColor;
  nsCOMPtr<nsIWidget> mParentWidget;
};

#endif // nsColorPicker_h__
