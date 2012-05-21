/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIDeviceContextSpec.h"
#include "nsCOMPtr.h"

class nsDeviceContextSpecAndroid : public nsIDeviceContextSpec
{
public:
    NS_DECL_ISUPPORTS
    
    NS_IMETHOD GetSurfaceForPrinter(gfxASurface** surface);

    NS_IMETHOD Init(nsIWidget* aWidget,
                    nsIPrintSettings* aPS,
                    bool aIsPrintPreview);
    NS_IMETHOD BeginDocument(PRUnichar* aTitle,
                             PRUnichar* aPrintToFileName,
                             PRInt32 aStartPage,
                             PRInt32 aEndPage);
    NS_IMETHOD EndDocument();
    NS_IMETHOD BeginPage() { return NS_OK; }
    NS_IMETHOD EndPage() { return NS_OK; }

    NS_IMETHOD GetPath (const char** aPath);
private:
    nsCOMPtr<nsIPrintSettings> mPrintSettings;
    nsCOMPtr<nsIFile> mTempFile;
};
