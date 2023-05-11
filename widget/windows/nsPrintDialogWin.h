/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintDialog_h__
#define nsPrintDialog_h__

#include "nsIPrintDialogService.h"

#include "nsCOMPtr.h"
#include "nsIWindowWatcher.h"

#include <windef.h>

class nsIPrintSettings;
class nsIDialogParamBlock;

class nsPrintDialogServiceWin final : public nsIPrintDialogService {
 public:
  nsPrintDialogServiceWin();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTDIALOGSERVICE

 private:
  virtual ~nsPrintDialogServiceWin();

  nsresult DoDialog(mozIDOMWindowProxy* aParent,
                    nsIDialogParamBlock* aParamBlock, nsIPrintSettings* aPS,
                    const char* aChromeURL);

  nsCOMPtr<nsIWindowWatcher> mWatcher;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintDialogServiceWin,
                              NS_IPRINTDIALOGSERVICE_IID)

#endif
