/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintDialogQt_h__
#define nsPrintDialogQt_h__

#include "nsIPrintDialogService.h"

class nsIPrintSettings;

class nsPrintDialogServiceQt : public nsIPrintDialogService
{
public:
    nsPrintDialogServiceQt();
    virtual ~nsPrintDialogServiceQt();

    NS_DECL_ISUPPORTS

    NS_IMETHODIMP Init();
    NS_IMETHODIMP Show(nsIDOMWindow* aParent, 
                       nsIPrintSettings* aSettings,
                       nsIWebBrowserPrint* aWebBrowserPrint);
    NS_IMETHODIMP ShowPageSetup(nsIDOMWindow* aParent,
                                nsIPrintSettings* aSettings);
};

#endif
