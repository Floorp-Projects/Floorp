/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintDialog_h__
#define nsPrintDialog_h__

#include "nsIPrintDialogService.h"

class nsIPrintSettings;

class nsPrintDialogServiceWin : public nsIPrintDialogService
{
  virtual ~nsPrintDialogServiceWin();

public:
  nsPrintDialogServiceWin();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() override;
  NS_IMETHOD Show(nsPIDOMWindowOuter *aParent, nsIPrintSettings *aSettings,
                  nsIWebBrowserPrint *aWebBrowserPrint) override;
  NS_IMETHOD ShowPageSetup(nsPIDOMWindowOuter *aParent,
                           nsIPrintSettings *aSettings) override;

};

#endif
