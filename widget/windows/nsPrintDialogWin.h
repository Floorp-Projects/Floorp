/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintDialog_h__
#define nsPrintDialog_h__

#include "nsIPrintDialogService.h"

#include "nsCOMPtr.h"
#include "nsIWindowWatcher.h"

class nsIPrintSettings;
class nsIDialogParamBlock;

class nsPrintDialogServiceWin : public nsIPrintDialogService {
  virtual ~nsPrintDialogServiceWin();

 public:
  nsPrintDialogServiceWin();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() override;
  NS_IMETHOD Show(nsPIDOMWindowOuter* aParent,
                  nsIPrintSettings* aSettings) override;
  NS_IMETHOD ShowPageSetup(nsPIDOMWindowOuter* aParent,
                           nsIPrintSettings* aSettings) override;

 private:
  nsresult DoDialog(mozIDOMWindowProxy* aParent,
                    nsIDialogParamBlock* aParamBlock, nsIPrintSettings* aPS,
                    const char* aChromeURL);

  HWND GetHWNDForDOMWindow(mozIDOMWindowProxy* aWindow);

  nsCOMPtr<nsIWindowWatcher> mWatcher;
};

#endif
