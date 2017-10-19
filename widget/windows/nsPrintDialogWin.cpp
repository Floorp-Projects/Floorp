/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintDialogWin.h"

#include "nsIBaseWindow.h"
#include "nsIPrintSettings.h"
#include "nsIWebBrowserChrome.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsPrintDialogServiceWin, nsIPrintDialogService)

nsPrintDialogServiceWin::nsPrintDialogServiceWin()
{
}

nsPrintDialogServiceWin::~nsPrintDialogServiceWin()
{
}

NS_IMETHODIMP
nsPrintDialogServiceWin::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPrintDialogServiceWin::Show(nsPIDOMWindowOuter *aParent,
                              nsIPrintSettings *aSettings,
                              nsIWebBrowserPrint *aWebBrowserPrint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPrintDialogServiceWin::ShowPageSetup(nsPIDOMWindowOuter *aParent,
                                       nsIPrintSettings *aNSSettings)
{
  return NS_OK;
}
