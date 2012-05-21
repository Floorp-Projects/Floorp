/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsQt.h"
#include "nsPrintDialogQt.h"

// For Qt, we only support printing to PDF, and that doesn't need a
// print dialog at this point.  So, this class's methods are left
// un-implemented for now.

NS_IMPL_ISUPPORTS1(nsPrintDialogServiceQt, nsIPrintDialogService)

nsPrintDialogServiceQt::nsPrintDialogServiceQt()
{
}

nsPrintDialogServiceQt::~nsPrintDialogServiceQt()
{
}

NS_IMETHODIMP
nsPrintDialogServiceQt::Init()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrintDialogServiceQt::Show(nsIDOMWindow* aParent,
                             nsIPrintSettings* aSettings,
                             nsIWebBrowserPrint* aWebBrowserPrint)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrintDialogServiceQt::ShowPageSetup(nsIDOMWindow* aParent,
                                      nsIPrintSettings* aNSSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
