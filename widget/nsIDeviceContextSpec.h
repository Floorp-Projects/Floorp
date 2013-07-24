/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDeviceContextSpec_h___
#define nsIDeviceContextSpec_h___

#include "nsISupports.h"

class nsIWidget;
class nsIPrintSettings;
class gfxASurface;

#define NS_IDEVICE_CONTEXT_SPEC_IID   \
{ 0xb5548fb1, 0xf43e, 0x4921, \
  { 0x82, 0x19, 0xc3, 0x82, 0x06, 0xee, 0x74, 0x5c } }

class nsIDeviceContextSpec : public nsISupports
{
public:
   NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_SPEC_IID)

   /**
    * Initialize the device context spec.
    * @param aWidget         A widget a dialog can be hosted in
    * @param aPrintSettings  Print settings for the print operation
    * @param aIsPrintPreview True if creating Spec for PrintPreview
    * @return NS_OK or a suitable error code.
    */
   NS_IMETHOD Init(nsIWidget *aWidget,
                   nsIPrintSettings* aPrintSettings,
                   bool aIsPrintPreview) = 0;

   NS_IMETHOD GetSurfaceForPrinter(gfxASurface **nativeSurface) = 0;

   NS_IMETHOD BeginDocument(const nsAString& aTitle,
                            PRUnichar*       aPrintToFileName,
                            int32_t          aStartPage,
                            int32_t          aEndPage) = 0;

   NS_IMETHOD EndDocument() = 0;
   NS_IMETHOD BeginPage() = 0;
   NS_IMETHOD EndPage() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDeviceContextSpec,
                              NS_IDEVICE_CONTEXT_SPEC_IID)
#endif
